/*
 * android.webkit.WebView JNI dispatcher.
 *
 * The actual web engine lives behind the pluggable backend interface in
 * webview_backend.h (WPE WebKit builtin; others, e.g. Qt WebEngine, loaded
 * from libatl_webview_<name>.so). This file selects the backend at first
 * WebView creation — honouring ATL_WEBVIEW_MODULE — and provides the host
 * side: it owns the CPU frame bitmap each backend renders into, blits it into
 * the raster Skia scene from native_draw, and routes callbacks (load state,
 * JS results, asset reads) between the backend and the Java WebView.
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "include/core/SkBitmap.h"
#include "include/core/SkImage.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkSamplingOptions.h"

#include "../defines.h"
#include "../ATLWindow.h"
#include "../graphics/ATLCanvas.h"
#include "webview_backend.h"

/* from util.c; declared here to avoid util.h -> handle_cache.h, which uses the
 * C++ keyword `class` as a struct member and won't compile in C++ */
extern "C" JNIEnv *get_jni_env(void);

extern "C" {
#include "../generated_headers/android_webkit_WebView.h"
}

struct webview_host_peer {
	void *backend_peer = nullptr;
	jobject java_webview = nullptr; // global ref, for callbacks into Java
	SkBitmap frame;                 // latest web frame (CPU), backend-filled
	bool has_frame = false;
};

static const struct atl_webview_backend *active_backend;
static bool backend_selection_done;

/* an app AssetManager (global ref), captured from the first WebView created,
 * so backends can serve android-asset:/// out of the APK */
static jobject g_asset_manager;

static void *host_acquire_frame(void *host_peer, int width, int height,
                                enum atl_webview_format format, size_t *stride_out)
{
	webview_host_peer *peer = (webview_host_peer *)host_peer;
	SkColorType color = format == ATL_WEBVIEW_FORMAT_RGBA8888_PREMUL ?
		kRGBA_8888_SkColorType : kBGRA_8888_SkColorType;
	SkAlphaType alpha = format == ATL_WEBVIEW_FORMAT_BGRX8888 ?
		kOpaque_SkAlphaType : kPremul_SkAlphaType;
	if (peer->frame.width() != width || peer->frame.height() != height ||
	    peer->frame.colorType() != color || peer->frame.alphaType() != alpha)
		peer->frame.allocPixels(SkImageInfo::Make(width, height, color, alpha));
	*stride_out = peer->frame.rowBytes();
	return peer->frame.getPixels();
}

static void host_commit_frame(void *host_peer)
{
	webview_host_peer *peer = (webview_host_peer *)host_peer;
	peer->has_frame = true;
	atl_window_invalidate_all();
}

static void host_load_changed(void *host_peer, int load_event, const char *url)
{
	webview_host_peer *peer = (webview_host_peer *)host_peer;
	JNIEnv *env = get_jni_env();
	jclass cls = env->GetObjectClass(peer->java_webview);
	jmethodID mid = env->GetMethodID(cls, "internalLoadChanged", "(ILjava/lang/String;)V");
	jstring jurl = url ? env->NewStringUTF(url) : NULL;
	env->CallVoidMethod(peer->java_webview, mid, (jint)load_event, jurl);
	if (jurl)
		env->DeleteLocalRef(jurl);
	env->DeleteLocalRef(cls);
}

static void host_js_result(void *host_peer, int64_t callback_id, const char *result_json)
{
	webview_host_peer *peer = (webview_host_peer *)host_peer;
	JNIEnv *env = get_jni_env();
	jclass cls = env->GetObjectClass(peer->java_webview);
	jmethodID mid = env->GetMethodID(cls, "internalJsResult", "(JLjava/lang/String;)V");
	jstring jresult = result_json ? env->NewStringUTF(result_json) : NULL;
	env->CallVoidMethod(peer->java_webview, mid, (jlong)callback_id, jresult);
	if (jresult)
		env->DeleteLocalRef(jresult);
	env->DeleteLocalRef(cls);
}

static bool host_read_asset(const char *path, void **data, size_t *size)
{
	if (!g_asset_manager || !path)
		return false;

	JNIEnv *env = get_jni_env();
	env->PushLocalFrame(8);

	jclass am_cls = env->GetObjectClass(g_asset_manager);
	jmethodID open = env->GetMethodID(am_cls, "open", "(Ljava/lang/String;)Ljava/io/InputStream;");
	jobject is = env->CallObjectMethod(g_asset_manager, open, env->NewStringUTF(path));
	if (env->ExceptionCheck() || !is) {
		env->ExceptionClear();
		env->PopLocalFrame(NULL);
		return false;
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

	*size = out->len;
	*data = g_byte_array_free(out, FALSE);
	return true;
}

static bool host_intercept_request(void *host_peer, const char *method, const char *url,
                                   bool for_main_frame, struct atl_webview_response *response)
{
	webview_host_peer *peer = (webview_host_peer *)host_peer;
	JNIEnv *env = get_jni_env();
	env->PushLocalFrame(16);

	jclass cls = env->GetObjectClass(peer->java_webview);
	jmethodID mid = env->GetMethodID(cls, "internalInterceptRequest",
		"(Ljava/lang/String;Ljava/lang/String;Z)[Ljava/lang/Object;");
	jobjectArray result = (jobjectArray)env->CallObjectMethod(peer->java_webview, mid,
		env->NewStringUTF(method), env->NewStringUTF(url), (jboolean)for_main_frame);
	if (env->ExceptionCheck()) {
		env->ExceptionDescribe();
		env->ExceptionClear();
		result = NULL;
	}
	if (!result) {
		env->PopLocalFrame(NULL);
		return false;
	}

	memset(response, 0, sizeof(*response));
	jstring jmime = (jstring)env->GetObjectArrayElement(result, 0);
	jstring jencoding = (jstring)env->GetObjectArrayElement(result, 1);
	jbyteArray jbody = (jbyteArray)env->GetObjectArrayElement(result, 2);
	jobjectArray jheaders = (jobjectArray)env->GetObjectArrayElement(result, 3);
	if (jmime) {
		const char *mime = env->GetStringUTFChars(jmime, NULL);
		response->mime = g_strdup(mime);
		env->ReleaseStringUTFChars(jmime, mime);
	}
	if (jencoding) {
		const char *encoding = env->GetStringUTFChars(jencoding, NULL);
		response->encoding = g_strdup(encoding);
		env->ReleaseStringUTFChars(jencoding, encoding);
	}
	jsize body_len = env->GetArrayLength(jbody);
	response->data = (uint8_t *)g_malloc(body_len > 0 ? body_len : 1);
	env->GetByteArrayRegion(jbody, 0, body_len, (jbyte *)response->data);
	response->size = body_len;
	jsize num_headers = env->GetArrayLength(jheaders);
	response->headers = g_new0(char *, num_headers + 1);
	for (jsize i = 0; i < num_headers; i++) {
		jstring jheader = (jstring)env->GetObjectArrayElement(jheaders, i);
		const char *header = env->GetStringUTFChars(jheader, NULL);
		response->headers[i] = g_strdup(header);
		env->ReleaseStringUTFChars(jheader, header);
		env->DeleteLocalRef(jheader);
	}

	env->PopLocalFrame(NULL);
	return true;
}

static const struct atl_webview_host_ops host_ops = {
	.acquire_frame = host_acquire_frame,
	.commit_frame = host_commit_frame,
	.load_changed = host_load_changed,
	.js_result = host_js_result,
	.read_asset = host_read_asset,
	.intercept_request = host_intercept_request,
};

static const struct atl_webview_backend *const builtin_backends[] = {
	&atl_webview_backend_wpe,
};

/* mirrors input_method.c: ATL_WEBVIEW_MODULE forces a backend by name ("none"
 * disables WebView); otherwise the first available builtin wins. A forced name
 * with no builtin match is dlopen'd from libatl_webview_<name>.so. */
static void select_backend(void)
{
	const char *force = getenv("ATL_WEBVIEW_MODULE");
	if (force && !strcmp(force, "none"))
		return;

	for (size_t i = 0; i < sizeof(builtin_backends) / sizeof(builtin_backends[0]); i++) {
		const struct atl_webview_backend *backend = builtin_backends[i];
		if (!backend) /* weak symbol, backend not compiled in */
			continue;
		if (force && strcmp(force, backend->name))
			continue;
		if (backend->init(&host_ops)) {
			fprintf(stderr, "WebView: using '%s' backend\n", backend->name);
			active_backend = backend;
			return;
		}
	}
	if (!force) {
		fprintf(stderr, "WebView: no backend available\n");
		return;
	}

	char soname[128];
	snprintf(soname, sizeof(soname), "libatl_webview_%s.so", force);
	void *handle = dlopen(soname, RTLD_NOW | RTLD_LOCAL);
	if (!handle) {
		fprintf(stderr, "WebView: requested backend '%s' is not available (%s)\n", force, dlerror());
		return;
	}
	const struct atl_webview_backend *(*entry)(void) =
		(const struct atl_webview_backend *(*)(void))dlsym(handle, "atl_webview_backend_entry");
	const struct atl_webview_backend *backend = entry ? entry() : NULL;
	if (backend && backend->init(&host_ops)) {
		fprintf(stderr, "WebView: using '%s' backend (%s)\n", backend->name, soname);
		active_backend = backend;
	} else {
		fprintf(stderr, "WebView: backend module %s failed to initialize\n", soname);
		dlclose(handle);
	}
}

JNIEXPORT jlong JNICALL Java_android_webkit_WebView_native_1create(JNIEnv *env, jobject thiz, jint width, jint height)
{
	if (!backend_selection_done) {
		backend_selection_done = true;
		select_backend();
	}
	if (!active_backend)
		return 0;

	/* capture an AssetManager for android-asset:/// asset reads */
	if (!g_asset_manager) {
		jmethodID get_am = env->GetMethodID(env->GetObjectClass(thiz),
			"internalGetAssetManager", "()Landroid/content/res/AssetManager;");
		jobject am = env->CallObjectMethod(thiz, get_am);
		if (am)
			g_asset_manager = env->NewGlobalRef(am);
	}

	webview_host_peer *peer = new webview_host_peer();
	peer->java_webview = env->NewGlobalRef(thiz);
	peer->backend_peer = active_backend->create(peer, width, height);
	if (!peer->backend_peer) {
		env->DeleteGlobalRef(peer->java_webview);
		delete peer;
		return 0;
	}
	return _INTPTR(peer);
}

JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1destroy(JNIEnv *env, jobject thiz, jlong peer_ptr)
{
	webview_host_peer *peer = (webview_host_peer *)_PTR(peer_ptr);
	if (!peer)
		return;
	active_backend->destroy(peer->backend_peer);
	env->DeleteGlobalRef(peer->java_webview);
	delete peer;
}

JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1setSize(JNIEnv *env, jobject thiz, jlong peer_ptr, jint width, jint height)
{
	webview_host_peer *peer = (webview_host_peer *)_PTR(peer_ptr);
	if (!peer || width <= 0 || height <= 0)
		return;
	active_backend->set_size(peer->backend_peer, width, height);
}

JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1loadUrl(JNIEnv *env, jobject thiz, jlong peer_ptr, jstring url)
{
	webview_host_peer *peer = (webview_host_peer *)_PTR(peer_ptr);
	if (!peer)
		return;
	const char *curl = env->GetStringUTFChars(url, NULL);
	active_backend->load_url(peer->backend_peer, curl);
	env->ReleaseStringUTFChars(url, curl);
}

JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1loadHtml(JNIEnv *env, jobject thiz, jlong peer_ptr, jstring html, jstring base_uri)
{
	webview_host_peer *peer = (webview_host_peer *)_PTR(peer_ptr);
	if (!peer)
		return;
	const char *chtml = env->GetStringUTFChars(html, NULL);
	const char *cbase = base_uri ? env->GetStringUTFChars(base_uri, NULL) : NULL;
	active_backend->load_html(peer->backend_peer, chtml, cbase);
	env->ReleaseStringUTFChars(html, chtml);
	if (cbase)
		env->ReleaseStringUTFChars(base_uri, cbase);
}

JNIEXPORT jboolean JNICALL Java_android_webkit_WebView_native_1motionEvent(JNIEnv *env, jobject thiz, jlong peer_ptr, jint action, jfloat x, jfloat y, jlong time_ms)
{
	webview_host_peer *peer = (webview_host_peer *)_PTR(peer_ptr);
	if (!peer || !active_backend->motion_event)
		return JNI_FALSE;
	active_backend->motion_event(peer->backend_peer, action, x, y, (uint64_t)time_ms);
	return JNI_TRUE;
}

/* returns false when the backend can't run JS; the Java side then completes
 * the callback with "null" itself */
JNIEXPORT jboolean JNICALL Java_android_webkit_WebView_native_1runJs(JNIEnv *env, jobject thiz, jlong peer_ptr, jstring script, jlong callback_id)
{
	webview_host_peer *peer = (webview_host_peer *)_PTR(peer_ptr);
	if (!peer || !active_backend->run_javascript)
		return JNI_FALSE;
	const char *cscript = env->GetStringUTFChars(script, NULL);
	active_backend->run_javascript(peer->backend_peer, cscript, (int64_t)callback_id);
	env->ReleaseStringUTFChars(script, cscript);
	return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1draw(JNIEnv *env, jobject thiz, jlong peer_ptr, jlong canvas_ptr, jint width, jint height)
{
	webview_host_peer *peer = (webview_host_peer *)_PTR(peer_ptr);
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(canvas_ptr);
	if (!peer || !atl_canvas || !atl_canvas->canvas || !peer->has_frame)
		return;

	sk_sp<SkImage> image = peer->frame.asImage();
	atl_canvas->canvas->drawImageRect(image,
		SkRect::MakeWH(width, height),
		SkSamplingOptions(SkFilterMode::kLinear));
}
