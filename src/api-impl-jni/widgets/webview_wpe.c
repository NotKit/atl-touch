/*
 * WPE WebKit backend for android.webkit.WebView (see webview_backend.h).
 *
 * WebKit renders into a WPEBackend-fdo "exportable" view backend, which hands
 * us one buffer per produced frame on the GLib main thread. Two buffer paths,
 * chosen once at first WebView creation:
 *
 *  - EGLImage (GPU): bind the exported EGLImage to a GL texture, read it back
 *    through an FBO into the host's CPU frame buffer. GPU->CPU readback per
 *    frame is deliberate: the host composites into a raster Skia scene.
 *
 *  - wl_shm (CPU): when there is no usable GPU (no DRM render node, or EGL
 *    without EGL_WL_bind_wayland_display — hybris on Ubuntu Touch), the
 *    EGLImage path is broken, not just slow: WebKit exports dmabuf-backed
 *    wl_buffers that such EGL cannot honour, and WPEBackend-fdo dereferences a
 *    NULL request handler while dispatching one -> SIGSEGV. Instead the
 *    WebProcess software-paints into wl_shm buffers we memcpy into the host
 *    frame, with no GL at all.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gio/gio.h>
#include <GL/gl.h>
#include <EGL/egl.h>

#include <wpe/webkit.h>
#include <wpe/fdo.h>
#include <wpe/fdo-egl.h>
#include <wpe/wpe.h>
#include <wpe/unstable/fdo-shm.h>
#include <jsc/jsc.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include "../ATLWindow.h"
#include "webview_backend.h"

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

typedef void (*PFN_glEGLImageTargetTexture2DOES)(GLenum target, void *image);
typedef void (*PFN_glGenFramebuffers)(GLsizei n, GLuint *framebuffers);
typedef void (*PFN_glBindFramebuffer)(GLenum target, GLuint framebuffer);
typedef void (*PFN_glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);

/* FBO enums are missing from GL/gl.h (GL 1.x); same values in GL and GLES2 */
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif

struct wpe_webview_peer {
	struct wpe_view_backend_exportable_fdo *exportable;
	struct wpe_view_backend *backend;
	WebKitWebView *web_view;
	void *host_peer;
	GCancellable *cancellable; /* cancels in-flight evaluate_javascript on destroy */

	GLuint texture;
	GLuint fbo; /* for reading the EGLImage texture back (GLES has no glGetTexImage) */
	int width, height;
};

static const struct atl_webview_host_ops *host;

static bool wpe_initialized = false;
/* true once we've decided to ship web frames as CPU wl_shm buffers rather than
 * EGLImages (set in ensure_wpe_initialized, process-wide). */
static bool use_shm = false;
static PFN_glEGLImageTargetTexture2DOES image_target_texture_2d = NULL;
static PFN_glGenFramebuffers gen_framebuffers = NULL;
static PFN_glBindFramebuffer bind_framebuffer = NULL;
static PFN_glFramebufferTexture2D framebuffer_texture_2d = NULL;

/* The EGLImage buffer-sharing path needs a real GPU; without one WebKit hands
 * WPEBackend-fdo dmabuf wl_buffers it cannot back, and fdo crashes on a NULL
 * handler (see file comment). ATL_WEBVIEW_SHM=1/0 forces the SHM/EGL path. */
static bool webview_should_use_shm(void)
{
	const char *force = getenv("ATL_WEBVIEW_SHM");
	if (force)
		return atoi(force) != 0;

	/* dmabuf buffer sharing (what the EGLImage path relies on) needs a DRM
	 * render node / GBM. On a machine without one — headless VPS, many
	 * containers — there is simply no dmabuf path, so ship CPU (wl_shm) frames.
	 * Real GPU devices (incl. the Ubuntu Touch target) expose renderD128+. */
	for (int minor = 128; minor < 128 + 16; minor++) {
		char path[64];
		snprintf(path, sizeof(path), "/dev/dri/renderD%d", minor);
		if (access(path, R_OK | W_OK) == 0)
			return false;
	}
	return true;
}

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

/* serve android-asset:///<path> from the app's assets (via the host, which
 * reads them out of the APK). Runs on the GLib main thread. */
static void android_asset_scheme_handler(WebKitURISchemeRequest *request, gpointer user_data)
{
	const char *path = webkit_uri_scheme_request_get_path(request);
	const char *asset = path && path[0] == '/' ? path + 1 : path;
	void *data;
	size_t len;

	if (!asset || !host->read_asset(asset, &data, &len)) {
		GError *e = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "asset not found: %s", asset ? asset : "(null)");
		webkit_uri_scheme_request_finish_error(request, e);
		g_error_free(e);
		return;
	}

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

	use_shm = webview_should_use_shm();
	/* The EGLImage path also needs the display to accept WPEBackend-fdo's
	 * buffer sharing (EGL_WL_bind_wayland_display). Hybris/Android EGL on
	 * Ubuntu Touch exposes a DRM render node yet lacks the extension, so
	 * exports would never arrive — fall back to CPU (wl_shm) frames. */
	if (!use_shm && !getenv("ATL_WEBVIEW_SHM")) {
		const char *egl_exts = eglQueryString(display, EGL_EXTENSIONS);
		if (!egl_exts || !strstr(egl_exts, "EGL_WL_bind_wayland_display"))
			use_shm = true;
	}
	atl_primary_make_context_current();
	const char *gl_renderer = (const char *)glGetString(GL_RENDERER);
	fprintf(stderr, "WebView: GL renderer='%s' -> %s buffer path\n",
	        gl_renderer ? gl_renderer : "(null)", use_shm ? "SHM/CPU" : "EGLImage/GPU");

	/* libwpe's loader otherwise dlopen()s the hardcoded default backend name
	 * "libWPEBackend-default.so", which several distros (e.g. Manjaro) don't
	 * ship as a symlink — only the real "libWPEBackend-fdo-1.0.so". Missing it,
	 * libwpe abort()s the whole process (under ART the abort message is
	 * swallowed, so it looks like a silent exit). Point the loader at fdo
	 * explicitly; this also propagates to the WPEWebProcess subprocess. */
	wpe_loader_init("libWPEBackend-fdo-1.0.so");

	if (use_shm) {
		/* We ship CPU (wl_shm) frames, but the WebProcess still has to paint them.
		 * With no usable GPU its accelerated (GL) compositor fails to initialize
		 * (Mesa: "ZINK: failed to choose pdev" / "failed to create dri2 screen")
		 * and produces no frames at all -> blank WebView. Force non-accelerated,
		 * CPU-side painting so pages render without a GPU. Set before the first
		 * WebProcess is spawned; WebKit forwards these to the subprocess. */
		setenv("WEBKIT_DISABLE_COMPOSITING_MODE", "1", 1);
		setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", 1);
		if (!wpe_fdo_initialize_shm()) {
			fprintf(stderr, "WebView: wpe_fdo_initialize_shm() failed - WebView unavailable\n");
			return false;
		}
	} else {
		image_target_texture_2d =
			(PFN_glEGLImageTargetTexture2DOES)eglGetProcAddress("glEGLImageTargetTexture2DOES");
		gen_framebuffers = (PFN_glGenFramebuffers)eglGetProcAddress("glGenFramebuffers");
		bind_framebuffer = (PFN_glBindFramebuffer)eglGetProcAddress("glBindFramebuffer");
		framebuffer_texture_2d = (PFN_glFramebufferTexture2D)eglGetProcAddress("glFramebufferTexture2D");
		if (!image_target_texture_2d || !gen_framebuffers || !bind_framebuffer || !framebuffer_texture_2d) {
			fprintf(stderr, "WebView: EGLImage/FBO entry points unavailable\n");
			return false;
		}
		wpe_fdo_initialize_for_egl_display(display);
	}

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

/* WebKit produced a frame: bind the EGLImage, read it back into the host's
 * frame buffer, release it and let WebKit render the next one. Runs on the
 * GLib main thread. */
static void on_export_egl_image(void *data, struct wpe_fdo_egl_exported_image *image)
{
	struct wpe_webview_peer *peer = data;

	atl_primary_make_context_current();

	int w = wpe_fdo_egl_exported_image_get_width(image);
	int h = wpe_fdo_egl_exported_image_get_height(image);

	if (!peer->texture)
		glGenTextures(1, &peer->texture);
	glBindTexture(GL_TEXTURE_2D, peer->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	image_target_texture_2d(GL_TEXTURE_2D, wpe_fdo_egl_exported_image_get_egl_image(image));

	/* read the texture back through an FBO; unlike glGetTexImage this also
	 * works on GLES2 contexts (Ubuntu Touch / hybris). glReadPixels writes
	 * tightly packed rows, which is exactly the host's RGBA stride. */
	size_t stride;
	void *pixels = host->acquire_frame(peer->host_peer, w, h,
	                                   ATL_WEBVIEW_FORMAT_RGBA8888_PREMUL, &stride);
	if (pixels && stride == (size_t)w * 4) {
		if (!peer->fbo)
			gen_framebuffers(1, &peer->fbo);
		bind_framebuffer(GL_FRAMEBUFFER, peer->fbo);
		framebuffer_texture_2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, peer->texture, 0);
		glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		bind_framebuffer(GL_FRAMEBUFFER, 0);
		host->commit_frame(peer->host_peer);
	}

	wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(peer->exportable, image);
	wpe_view_backend_exportable_fdo_dispatch_frame_complete(peer->exportable);
}

/* Software path: WebKit software-painted a wl_shm buffer. Copy it into the
 * host's frame buffer (no GL involved) and hand the buffer back. Runs on the
 * GLib main thread. Wayland ARGB8888/XRGB8888 is little-endian B,G,R,A in
 * memory, i.e. BGRA. */
static void on_export_shm_buffer(void *data, struct wpe_fdo_shm_exported_buffer *buffer)
{
	struct wpe_webview_peer *peer = data;
	struct wl_shm_buffer *shm = wpe_fdo_shm_exported_buffer_get_shm_buffer(buffer);
	if (shm) {
		int w = wl_shm_buffer_get_width(shm);
		int h = wl_shm_buffer_get_height(shm);
		int32_t src_stride = wl_shm_buffer_get_stride(shm);
		uint32_t format = wl_shm_buffer_get_format(shm);

		wl_shm_buffer_begin_access(shm);
		const uint8_t *src = wl_shm_buffer_get_data(shm);
		if (src && w > 0 && h > 0 && src_stride > 0) {
			enum atl_webview_format fmt = format == WL_SHM_FORMAT_XRGB8888 ?
				ATL_WEBVIEW_FORMAT_BGRX8888 : ATL_WEBVIEW_FORMAT_BGRA8888_PREMUL;
			size_t dst_stride;
			uint8_t *dst = host->acquire_frame(peer->host_peer, w, h, fmt, &dst_stride);
			if (dst) {
				size_t row = (size_t)src_stride < dst_stride ? (size_t)src_stride : dst_stride;
				for (int y = 0; y < h; y++)
					memcpy(dst + (size_t)y * dst_stride, src + (size_t)y * src_stride, row);
				host->commit_frame(peer->host_peer);
			}
		}
		wl_shm_buffer_end_access(shm);
	}

	wpe_view_backend_exportable_fdo_dispatch_release_shm_exported_buffer(peer->exportable, buffer);
	wpe_view_backend_exportable_fdo_dispatch_frame_complete(peer->exportable);
}

static void on_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, gpointer data)
{
	struct wpe_webview_peer *peer = data;
	host->load_changed(peer->host_peer, (int)load_event, webkit_web_view_get_uri(web_view));
}

static bool wpe_backend_init(const struct atl_webview_host_ops *host_ops)
{
	host = host_ops;
	return true; /* heavy init happens in the first create() */
}

static void *wpe_backend_create(void *host_peer, int width, int height)
{
	if (!ensure_wpe_initialized())
		return NULL;

	struct wpe_webview_peer *peer = g_new0(struct wpe_webview_peer, 1);
	peer->host_peer = host_peer;
	peer->width = width > 0 ? width : 1;
	peer->height = height > 0 ? height : 1;
	peer->cancellable = g_cancellable_new();

	if (use_shm) {
		static const struct wpe_view_backend_exportable_fdo_client shm_client = {
			.export_shm_buffer = on_export_shm_buffer,
		};
		peer->exportable = wpe_view_backend_exportable_fdo_create(&shm_client, peer, peer->width, peer->height);
	} else {
		static const struct wpe_view_backend_exportable_fdo_egl_client egl_client = {
			.export_fdo_egl_image = on_export_egl_image,
		};
		peer->exportable = wpe_view_backend_exportable_fdo_egl_create(&egl_client, peer, peer->width, peer->height);
	}
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

	return peer;
}

static void wpe_backend_destroy(void *peer_)
{
	struct wpe_webview_peer *peer = peer_;
	g_cancellable_cancel(peer->cancellable);
	g_object_unref(peer->cancellable);
	g_object_unref(peer->web_view);
	wpe_view_backend_exportable_fdo_destroy(peer->exportable);
	g_free(peer);
}

static void wpe_backend_set_size(void *peer_, int width, int height)
{
	struct wpe_webview_peer *peer = peer_;
	peer->width = width;
	peer->height = height;
	wpe_view_backend_dispatch_set_size(peer->backend, width, height);
}

static void wpe_backend_load_url(void *peer_, const char *url)
{
	struct wpe_webview_peer *peer = peer_;
	webkit_web_view_load_uri(peer->web_view, url);
}

static void wpe_backend_load_html(void *peer_, const char *html, const char *base_uri)
{
	struct wpe_webview_peer *peer = peer_;
	webkit_web_view_load_html(peer->web_view, html, base_uri);
}

/* android MotionEvent actions -> WPE touch events. WebKit synthesizes clicks
 * from taps and scrolls from drags, so a single-point touch stream is enough
 * for cards. libwpe has no cancel type; map CANCEL to up. */
static void wpe_backend_motion_event(void *peer_, int action, float x, float y, uint64_t time_ms)
{
	struct wpe_webview_peer *peer = peer_;
	enum wpe_input_touch_event_type type;
	switch (action) {
	case 0: type = wpe_input_touch_event_type_down; break;   /* ACTION_DOWN */
	case 1: type = wpe_input_touch_event_type_up; break;     /* ACTION_UP */
	case 2: type = wpe_input_touch_event_type_motion; break; /* ACTION_MOVE */
	case 3: type = wpe_input_touch_event_type_up; break;     /* ACTION_CANCEL */
	default:
		return;
	}
	struct wpe_input_touch_event_raw point = {
		.type = type,
		.time = (uint32_t)time_ms,
		.id = 0,
		.x = (int32_t)x,
		.y = (int32_t)y,
	};
	struct wpe_input_touch_event event = {
		.touchpoints = &point,
		.touchpoints_length = 1,
		.type = type,
		.id = 0,
		.time = (uint32_t)time_ms,
	};
	wpe_view_backend_dispatch_touch_event(peer->backend, &event);
}

struct js_callback_data {
	void *host_peer;
	int64_t callback_id;
	GCancellable *cancellable; /* owned ref; skip host callback when cancelled */
};

static void on_js_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
	struct js_callback_data *data = user_data;
	GError *error = NULL;
	JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source), result, &error);

	/* cancelled = the WebView was destroyed; host_peer is gone */
	if (!g_cancellable_is_cancelled(data->cancellable)) {
		if (value) {
			char *json = jsc_value_to_json(value, 0);
			host->js_result(data->host_peer, data->callback_id, json);
			g_free(json);
		} else {
			if (error)
				fprintf(stderr, "WebView: evaluateJavascript failed: %s\n", error->message);
			host->js_result(data->host_peer, data->callback_id, NULL);
		}
	}
	if (value)
		g_object_unref(value);
	g_clear_error(&error);
	g_object_unref(data->cancellable);
	g_free(data);
}

static void wpe_backend_run_javascript(void *peer_, const char *script, int64_t callback_id)
{
	struct wpe_webview_peer *peer = peer_;
	struct js_callback_data *data = NULL;
	if (callback_id) {
		data = g_new0(struct js_callback_data, 1);
		data->host_peer = peer->host_peer;
		data->callback_id = callback_id;
		data->cancellable = g_object_ref(peer->cancellable);
	}
	webkit_web_view_evaluate_javascript(peer->web_view, script, -1, NULL, NULL,
	                                    peer->cancellable,
	                                    callback_id ? on_js_finished : NULL, data);
}

const struct atl_webview_backend atl_webview_backend_wpe = {
	.name = "wpe",
	.init = wpe_backend_init,
	.create = wpe_backend_create,
	.destroy = wpe_backend_destroy,
	.set_size = wpe_backend_set_size,
	.load_url = wpe_backend_load_url,
	.load_html = wpe_backend_load_html,
	.motion_event = wpe_backend_motion_event,
	.run_javascript = wpe_backend_run_javascript,
};
