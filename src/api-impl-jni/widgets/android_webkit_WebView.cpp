/*
 * android.webkit.WebView backed by WPE WebKit, rendered offscreen and
 * composited into the Skia scene.
 *
 * WebKit renders into a WPEBackend-fdo "exportable" view backend, which hands
 * us one EGLImage per produced frame (export_fdo_egl_image). On the GLib main
 * thread (the same thread that runs the Android Looper and the render tick) we
 * bind that EGLImage to a GL texture, read it back into a CPU SkBitmap, and
 * release it. WebView.onDraw() then draws that bitmap into the view's rect via
 * the normal Skia draw pass, so the web content clips, scrolls and z-orders
 * like any other view.
 *
 * A GPU->CPU readback per frame is deliberate: ATL's Skia scene is raster, so
 * this is the simplest correct path. Zero-copy texture compositing would need
 * Skia on a Ganesh/GPU backend and is a later optimization.
 */

#include <stdio.h>
#include <string.h>

#include <gio/gio.h>
#include <GL/gl.h>
#include <EGL/egl.h>

#include <wpe/webkit.h>
#include <wpe/fdo.h>
#include <wpe/fdo-egl.h>
#include <wpe/wpe.h>

#include "include/core/SkBitmap.h"
#include "include/core/SkImage.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkSamplingOptions.h"

#include "../defines.h"
#include "../ATLWindow.h"
#include "../graphics/ATLCanvas.h"

/* from util.c; declared here to avoid util.h -> handle_cache.h, which uses the
 * C++ keyword `class` as a struct member and won't compile in C++ */
extern "C" JNIEnv *get_jni_env(void);

extern "C" {
#include "../generated_headers/android_webkit_WebView.h"
}

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

typedef void (*PFN_glEGLImageTargetTexture2DOES)(GLenum target, void *image);

struct webview_peer {
	struct wpe_view_backend_exportable_fdo *exportable = nullptr;
	struct wpe_view_backend *backend = nullptr;
	WebKitWebView *web_view = nullptr;
	jobject java_webview = nullptr; // global ref, for load-changed callbacks

	GLuint texture = 0;
	SkBitmap frame; // latest decoded web frame (CPU)
	bool has_frame = false;
	int width = 1, height = 1;
};

static bool wpe_initialized = false;
static PFN_glEGLImageTargetTexture2DOES image_target_texture_2d = nullptr;
/* an app AssetManager (global ref), captured from the first WebView created, so
 * the android-asset:// scheme handler can serve files out of the APK. */
static jobject g_asset_manager = nullptr;

/* minimal content-type guess: the values that matter for cards are CSS and JS
 * (browsers ignore <link>/<script> resources served with the wrong type). */
static const char *guess_content_type(const char *path)
{
	const char *dot = strrchr(path, '.');
	if (!dot)
		return "application/octet-stream";
	if (!strcmp(dot, ".css"))  return "text/css";
	if (!strcmp(dot, ".js"))   return "text/javascript";
	if (!strcmp(dot, ".html") || !strcmp(dot, ".htm")) return "text/html";
	if (!strcmp(dot, ".json") || !strcmp(dot, ".map")) return "application/json";
	if (!strcmp(dot, ".svg"))  return "image/svg+xml";
	if (!strcmp(dot, ".png"))  return "image/png";
	if (!strcmp(dot, ".jpg") || !strcmp(dot, ".jpeg")) return "image/jpeg";
	if (!strcmp(dot, ".gif"))  return "image/gif";
	if (!strcmp(dot, ".webp")) return "image/webp";
	if (!strcmp(dot, ".woff2")) return "font/woff2";
	if (!strcmp(dot, ".woff")) return "font/woff";
	if (!strcmp(dot, ".ttf"))  return "font/ttf";
	if (!strcmp(dot, ".otf"))  return "font/otf";
	if (!strcmp(dot, ".mp3"))  return "audio/mpeg";
	if (!strcmp(dot, ".ogg"))  return "audio/ogg";
	if (!strcmp(dot, ".wav"))  return "audio/wav";
	return "application/octet-stream";
}

/* serve android-asset:///<path> from the app's AssetManager. Registered on the
 * default web context, so it runs on the GLib main thread (a Java thread). It
 * is NOT a JNI entry point, so its local refs must be freed explicitly. */
static void android_asset_scheme_handler(WebKitURISchemeRequest *request, gpointer user_data)
{
	const char *path = webkit_uri_scheme_request_get_path(request);
	if (!g_asset_manager || !path) {
		GError *e = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "no asset manager / path");
		webkit_uri_scheme_request_finish_error(request, e);
		g_error_free(e);
		return;
	}
	const char *asset = path[0] == '/' ? path + 1 : path;

	JNIEnv *env = get_jni_env();
	env->PushLocalFrame(8);

	jclass am_cls = env->GetObjectClass(g_asset_manager);
	jmethodID open = env->GetMethodID(am_cls, "open", "(Ljava/lang/String;)Ljava/io/InputStream;");
	jobject is = env->CallObjectMethod(g_asset_manager, open, env->NewStringUTF(asset));
	if (env->ExceptionCheck() || !is) {
		env->ExceptionClear();
		env->PopLocalFrame(NULL);
		GError *e = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "asset not found: %s", asset);
		webkit_uri_scheme_request_finish_error(request, e);
		g_error_free(e);
		return;
	}

	jclass is_cls = env->GetObjectClass(is);
	jmethodID read = env->GetMethodID(is_cls, "read", "([B)I");
	jmethodID close = env->GetMethodID(is_cls, "close", "()V");
	jbyteArray buf = env->NewByteArray(65536);
	GByteArray *out = g_byte_array_new();
	jint n;
	while ((n = env->CallIntMethod(is, read, buf)) > 0) {
		jbyte *p = env->GetByteArrayElements(buf, NULL);
		g_byte_array_append(out, (guint8 *)p, n);
		env->ReleaseByteArrayElements(buf, p, JNI_ABORT);
	}
	env->CallVoidMethod(is, close);
	env->PopLocalFrame(NULL);

	gsize len = out->len;
	guint8 *data = g_byte_array_free(out, FALSE);
	GInputStream *stream = g_memory_input_stream_new_from_data(data, len, g_free);
	webkit_uri_scheme_request_finish(request, stream, (gint64)len, guess_content_type(asset));
	g_object_unref(stream);
}

static bool ensure_wpe_initialized(void)
{
	if (wpe_initialized)
		return true;
	EGLDisplay display = (EGLDisplay)atl_primary_egl_display();
	if (display == EGL_NO_DISPLAY) {
		fprintf(stderr, "WebView: no EGLDisplay (is a window open yet?) - cannot init WPE\n");
		return false;
	}
	image_target_texture_2d =
		(PFN_glEGLImageTargetTexture2DOES)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if (!image_target_texture_2d) {
		fprintf(stderr, "WebView: glEGLImageTargetTexture2DOES unavailable (no GL_OES_EGL_image)\n");
		return false;
	}
	/* libwpe's loader otherwise dlopen()s the hardcoded default backend name
	 * "libWPEBackend-default.so", which several distros (e.g. Manjaro) don't
	 * ship as a symlink — only the real "libWPEBackend-fdo-1.0.so". Missing it,
	 * libwpe abort()s the whole process (under ART the abort message is
	 * swallowed, so it looks like a silent exit). Point the loader at fdo
	 * explicitly; this also propagates to the WPEWebProcess subprocess. */
	wpe_loader_init("libWPEBackend-fdo-1.0.so");
	wpe_fdo_initialize_for_egl_display(display);

	/* serve the app's assets (card CSS/JS/images linked as file:///android_asset,
	 * rewritten to android-asset:// in WebView.loadDataWithBaseURL) */
	WebKitWebContext *ctx = webkit_web_context_get_default();
	webkit_web_context_register_uri_scheme(ctx, "android-asset", android_asset_scheme_handler, NULL, NULL);
	WebKitSecurityManager *sm = webkit_web_context_get_security_manager(ctx);
	webkit_security_manager_register_uri_scheme_as_secure(sm, "android-asset");
	webkit_security_manager_register_uri_scheme_as_cors_enabled(sm, "android-asset");

	wpe_initialized = true;
	return true;
}

/* WebKit produced a frame: bind the EGLImage, read it back to peer->frame,
 * release it and let WebKit render the next one. Runs on the GLib main thread. */
static void on_export_egl_image(void *data, struct wpe_fdo_egl_exported_image *image)
{
	webview_peer *peer = (webview_peer *)data;

	atl_primary_make_context_current();

	int w = wpe_fdo_egl_exported_image_get_width(image);
	int h = wpe_fdo_egl_exported_image_get_height(image);

	if (!peer->texture)
		glGenTextures(1, &peer->texture);
	glBindTexture(GL_TEXTURE_2D, peer->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	image_target_texture_2d(GL_TEXTURE_2D, wpe_fdo_egl_exported_image_get_egl_image(image));

	if (peer->frame.width() != w || peer->frame.height() != h)
		peer->frame.allocPixels(SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kPremul_SkAlphaType));

	/* desktop GL: pull the whole texture level back to the CPU bitmap */
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, peer->frame.getPixels());
	peer->has_frame = true;

	wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(peer->exportable, image);
	wpe_view_backend_exportable_fdo_dispatch_frame_complete(peer->exportable);

	atl_window_invalidate_all();
}

static void on_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, gpointer data)
{
	webview_peer *peer = (webview_peer *)data;
	if (!peer->java_webview)
		return;
	JNIEnv *env = get_jni_env();
	jclass cls = env->GetObjectClass(peer->java_webview);
	jmethodID mid = env->GetMethodID(cls, "internalLoadChanged", "(ILjava/lang/String;)V");
	const char *uri = webkit_web_view_get_uri(web_view);
	jstring juri = uri ? env->NewStringUTF(uri) : NULL;
	env->CallVoidMethod(peer->java_webview, mid, (jint)load_event, juri);
}

JNIEXPORT jlong JNICALL Java_android_webkit_WebView_native_1create(JNIEnv *env, jobject thiz, jint width, jint height)
{
	if (!ensure_wpe_initialized())
		return 0;

	webview_peer *peer = new webview_peer();
	peer->width = width > 0 ? width : 1;
	peer->height = height > 0 ? height : 1;
	peer->java_webview = env->NewGlobalRef(thiz);

	/* capture an AssetManager for the android-asset:// scheme handler */
	if (!g_asset_manager) {
		jmethodID get_am = env->GetMethodID(env->GetObjectClass(thiz),
			"internalGetAssetManager", "()Landroid/content/res/AssetManager;");
		jobject am = env->CallObjectMethod(thiz, get_am);
		if (am)
			g_asset_manager = env->NewGlobalRef(am);
	}

	static const struct wpe_view_backend_exportable_fdo_egl_client client = {
		.export_egl_image = nullptr,
		.export_fdo_egl_image = on_export_egl_image,
		.export_shm_buffer = nullptr,
		._wpe_reserved0 = nullptr,
		._wpe_reserved1 = nullptr,
	};
	peer->exportable = wpe_view_backend_exportable_fdo_egl_create(&client, peer, peer->width, peer->height);
	peer->backend = wpe_view_backend_exportable_fdo_get_view_backend(peer->exportable);

	WebKitWebViewBackend *view_backend =
		webkit_web_view_backend_new(peer->backend, NULL, NULL);
	peer->web_view = webkit_web_view_new(view_backend);
	g_object_ref_sink(peer->web_view);

	/* WebKit only paints a view it believes is on-screen */
	wpe_view_backend_add_activity_state(peer->backend,
		wpe_view_activity_state_visible | wpe_view_activity_state_focused | wpe_view_activity_state_in_window);
	wpe_view_backend_dispatch_set_size(peer->backend, peer->width, peer->height);

	g_signal_connect(peer->web_view, "load-changed", G_CALLBACK(on_load_changed), peer);

	return _INTPTR(peer);
}

JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1setSize(JNIEnv *env, jobject thiz, jlong peer_ptr, jint width, jint height)
{
	webview_peer *peer = (webview_peer *)_PTR(peer_ptr);
	if (!peer || width <= 0 || height <= 0)
		return;
	peer->width = width;
	peer->height = height;
	wpe_view_backend_dispatch_set_size(peer->backend, width, height);
}

JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1loadUrl(JNIEnv *env, jobject thiz, jlong peer_ptr, jstring url)
{
	webview_peer *peer = (webview_peer *)_PTR(peer_ptr);
	if (!peer)
		return;
	const char *curl = env->GetStringUTFChars(url, NULL);
	webkit_web_view_load_uri(peer->web_view, curl);
	env->ReleaseStringUTFChars(url, curl);
}

JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1loadHtml(JNIEnv *env, jobject thiz, jlong peer_ptr, jstring html, jstring base_uri)
{
	webview_peer *peer = (webview_peer *)_PTR(peer_ptr);
	if (!peer)
		return;
	const char *chtml = env->GetStringUTFChars(html, NULL);
	const char *cbase = base_uri ? env->GetStringUTFChars(base_uri, NULL) : NULL;
	webkit_web_view_load_html(peer->web_view, chtml, cbase);
	env->ReleaseStringUTFChars(html, chtml);
	if (cbase)
		env->ReleaseStringUTFChars(base_uri, cbase);
}

JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1draw(JNIEnv *env, jobject thiz, jlong peer_ptr, jlong canvas_ptr, jint width, jint height)
{
	webview_peer *peer = (webview_peer *)_PTR(peer_ptr);
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(canvas_ptr);
	if (!peer || !atl_canvas || !atl_canvas->canvas || !peer->has_frame)
		return;

	sk_sp<SkImage> image = peer->frame.asImage();
	atl_canvas->canvas->drawImageRect(image,
		SkRect::MakeWH(width, height),
		SkSamplingOptions(SkFilterMode::kLinear));
}
