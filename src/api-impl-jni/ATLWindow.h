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

/* IME event injection, used by input method backends (src/api-impl-jni/input/):
 * text and key events go through the same dispatch as hardware keyboard
 * input; the inset shrinks the layout so the panel doesn't cover the UI.
 *
 * commit/composing carry Maliit's replacement range: replace_length characters
 * starting replace_start from the composing region (from the cursor when there
 * is none) are deleted before the new text goes in, which is how a keyboard
 * corrects a word it already committed. cursor_pos < 0 means "after the text". */
void atl_windows_ime_commit_text(const char *utf8, int replace_start, int replace_length, int cursor_pos);
void atl_windows_ime_set_composing(const char *utf8, int replace_start, int replace_length, int cursor_pos);
void atl_windows_ime_finish_composing(void);
void atl_windows_ime_key(int action, int keycode);
void atl_windows_ime_set_selection(int start, int length);
/* the focused editor's selected text, UTF-8; caller g_free()s. NULL if there is
 * no editor */
char *atl_windows_ime_get_selection(void);
/* the input method closed its panel by itself: drop the editor's focus */
void atl_windows_ime_initiated_hide(void);
void atl_windows_set_ime_inset(int inset);
bool atl_debug_ime(void);

ATLWindow *atl_window_new(int width, int height, bool visible, bool decorated);
void atl_window_set_view_root(ATLWindow *window, JNIEnv *env, jobject view_root);
void atl_window_set_title(ATLWindow *window, const char *title);
void atl_window_set_default_size(ATLWindow *window, int width, int height);
void atl_window_show(ATLWindow *window);
void atl_window_hide(ATLWindow *window);
bool atl_window_is_visible(ATLWindow *window);
void atl_window_focus(ATLWindow *window);
void atl_display_set_window_size(JNIEnv *env, int width, int height);
int atl_window_get_width(ATLWindow *window);
int atl_window_get_height(ATLWindow *window);
void atl_window_set_jobject(ATLWindow *window, JNIEnv *env, jobject window_obj);
jobject atl_window_get_jobject(ATLWindow *window);
void atl_window_focus(ATLWindow *window);
void atl_window_set_clipboard(ATLWindow *window, const char *text);
const char *atl_window_get_clipboard(ATLWindow *window);
bool atl_window_is_maximized(ATLWindow *window);

/* --- Wayland objects the subsurface layers hang off (NULL off Wayland) --- */
struct wl_compositor;
struct wl_subcompositor;
struct wp_viewporter;
struct wl_surface;
struct wl_compositor *atl_wayland_compositor(void);
struct wl_subcompositor *atl_wayland_subcompositor(void);
struct wp_viewporter *atl_wayland_viewporter(void);
struct wl_surface *atl_window_wl_surface(ATLWindow *window);
/* framebuffer pixels per logical pixel */
double atl_window_scale(ATLWindow *window);
void atl_window_invalidate(ATLWindow *window);

/* size of the primary monitor in pixels; false if there is no monitor
 * (or GLFW is not initialized yet), in which case *width/*height are
 * left untouched */

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
