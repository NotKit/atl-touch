#pragma once

#include <jni.h>
#include <stdbool.h>

/*
 * ATLWindow: a GLFW window hosting an Android view hierarchy.
 *
 * This is the windowing replacement for GtkWindow: the Skia-rendered view
 * tree (driven through android.view.ViewRootImpl) is blitted to the window's
 * GL framebuffer, and GLFW input callbacks feed the java input dispatch.
 * Event pumping and rendering run from a GLib timeout source so the GLib
 * main loop (which ART and the Android Looper integrate with) stays in
 * charge.
 */

typedef struct ATLWindow ATLWindow;

/* call once on the main thread before creating windows */
void atl_windows_init(void);

ATLWindow *atl_window_new(int width, int height, bool visible, bool decorated);
void atl_window_set_view_root(ATLWindow *window, JNIEnv *env, jobject view_root);
void atl_window_set_title(ATLWindow *window, const char *title);
void atl_window_set_default_size(ATLWindow *window, int width, int height);
void atl_window_show(ATLWindow *window);
void atl_window_hide(ATLWindow *window);
bool atl_window_is_visible(ATLWindow *window);
void atl_window_focus(ATLWindow *window);
int atl_window_get_width(ATLWindow *window);
void atl_window_set_jobject(ATLWindow *window, JNIEnv *env, jobject window_obj);
jobject atl_window_get_jobject(ATLWindow *window);
void atl_window_focus(ATLWindow *window);
void atl_window_set_clipboard(ATLWindow *window, const char *text);
const char *atl_window_get_clipboard(ATLWindow *window);

/* WPE WebView offscreen integration. Called from the C++ WebView module, so
 * these must keep C linkage to match their definitions in ATLWindow.c. */
#ifdef __cplusplus
extern "C" {
#endif
void *atl_primary_egl_display(void);
void atl_primary_make_context_current(void);
void atl_window_invalidate_all(void);
#ifdef __cplusplus
}
#endif
