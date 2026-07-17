#ifndef ATL_WEBVIEW_BACKEND_H
#define ATL_WEBVIEW_BACKEND_H

/*
 * Pluggable rendering backends for android.webkit.WebView, mirroring the
 * input-method framework (input/input_method.h): the dispatcher in
 * android_webkit_WebView.cpp picks a backend at first WebView creation,
 * honouring ATL_WEBVIEW_MODULE (a backend name, or "none" to disable).
 * Backends not listed in the builtin table are dlopen'd from
 * libatl_webview_<name>.so so that e.g. the Qt WebEngine backend can link Qt
 * without making it a dependency of the main library.
 *
 * The contract is plain C and deliberately free of JNI, Skia and EGL: a
 * backend renders web content however it likes and delivers each finished
 * frame as CPU pixels through acquire_frame/commit_frame; the dispatcher owns
 * the SkBitmap behind it and blits it into the raster Skia scene. All calls,
 * both directions, happen on the GLib main thread.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum atl_webview_format {
	ATL_WEBVIEW_FORMAT_RGBA8888_PREMUL,
	ATL_WEBVIEW_FORMAT_BGRA8888_PREMUL,
	ATL_WEBVIEW_FORMAT_BGRX8888,
};

/* load_event values follow WebKitLoadEvent (what WebView.internalLoadChanged
 * already speaks): 0 = started, 3 = finished */
#define ATL_WEBVIEW_LOAD_STARTED 0
#define ATL_WEBVIEW_LOAD_FINISHED 3

/* services the dispatcher provides to backends; host_peer identifies the
 * WebView the call is about (first argument of create) */
struct atl_webview_host_ops {
	/* frame delivery: acquire a width x height pixel buffer (stride_out gets
	 * its row length in bytes), fill it, commit. Committing marks the WebView
	 * dirty and schedules a scene redraw. */
	void *(*acquire_frame)(void *host_peer, int width, int height,
	                       enum atl_webview_format format, size_t *stride_out);
	void (*commit_frame)(void *host_peer);
	/* forward a load-state change to the app's WebViewClient */
	void (*load_changed)(void *host_peer, int load_event, const char *url);
	/* deliver the result of run_javascript; result_json is the value as JSON,
	 * or NULL on error (callback_id as passed to run_javascript) */
	void (*js_result)(void *host_peer, int64_t callback_id, const char *result_json);
	/* read an app asset (the android-asset:/// scheme). On success returns
	 * true and a g_malloc'd buffer the caller frees with g_free. */
	bool (*read_asset)(const char *path, void **data, size_t *size);
};

struct atl_webview_backend {
	const char *name;
	/* cheap availability probe; heavy setup belongs in the first create().
	 * host stays valid for the process lifetime. */
	bool (*init)(const struct atl_webview_host_ops *host);
	/* returns a backend peer for one WebView, or NULL on failure */
	void *(*create)(void *host_peer, int width, int height);
	void (*destroy)(void *peer);
	void (*set_size)(void *peer, int width, int height);
	void (*load_url)(void *peer, const char *url);
	void (*load_html)(void *peer, const char *html, const char *base_uri);
	/* optional, may be NULL: */
	/* action = android MotionEvent action (0 down, 1 up, 2 move, 3 cancel),
	 * x/y in view pixels, time in SystemClock.uptimeMillis() base */
	void (*motion_event)(void *peer, int action, float x, float y, uint64_t time_ms);
	/* run script, report via host->js_result(host_peer, callback_id, ...);
	 * callback_id 0 = fire and forget */
	void (*run_javascript)(void *peer, const char *script, int64_t callback_id);
};

/* builtin backends (weak: absent when not compiled in) */
extern const struct atl_webview_backend atl_webview_backend_wpe __attribute__((weak));

#ifdef __cplusplus
}
#endif

#endif
