#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_EGL
#include <GLFW/glfw3native.h>
#include <glib.h>

#include "defines.h"
#include "util.h"

#include "ATLWindow.h"
#include "graphics/ATLCanvas.h"

#include "generated_headers/android_view_ViewRootImpl.h"

struct ATLWindow {
	GLFWwindow *glfw_window;
	jobject view_root; // global ref, NULL until attached
	jmethodID perform_layout;
	jmethodID perform_draw;
	jmethodID dispatch_touch_event;
	jmethodID dispatch_key_event;
	jmethodID dispatch_character;
	jobject window_jobj;    // weak ref to the java android.view.Window
	bool needs_redraw;
	bool pointer_down;
	double pointer_x, pointer_y;
	int layout_width, layout_height;
	unsigned int gl_texture;
	struct ATLWindow *next;
};

static struct ATLWindow *windows = NULL;

extern void activity_close_all(void); // app/android_app_Activity.c

/* --- input dispatch --- */

#define ACTION_DOWN   0
#define ACTION_UP     1
#define ACTION_MOVE   2
#define ACTION_CANCEL 3

#define SOURCE_TOUCHSCREEN 0x1002

#define KEY_ACTION_DOWN 0
#define KEY_ACTION_UP   1

/* MotionEvent/KeyEvent times must share the time base of SystemClock.uptimeMillis()
 * (CLOCK_MONOTONIC ms), because GestureDetector and the Handler/Looper schedule
 * timeout messages (LONG_PRESS, tap) at event.getDownTime()+timeout against that
 * clock. Using glfwGetTime() (seconds since GLFW init) put events in a tiny,
 * unrelated time base, so those messages were always "overdue" and fired
 * immediately — every tap was misread as a long-press and onSingleTapUp never ran,
 * making clickable widgets like the FAB only fire intermittently. */
static jlong atl_uptime_millis(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (jlong)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void dispatch_pointer_event(ATLWindow *window, int action)
{
	if (!window->view_root)
		return;
	JNIEnv *env = get_jni_env();
	/* A pending exception (e.g. thrown by an app callback under a nested native
	 * frame that couldn't handle it) makes every JNI call below misbehave —
	 * object creation returns NULL and the dispatch call aborts the runtime
	 * ("with unexpected pending exception"). Surface and clear it instead. */
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	/* GLFW reports the cursor in window (logical) coordinates, but the scene is
	 * laid out and rendered in framebuffer pixels. On a scaled/HiDPI output the
	 * two differ by the content scale, so convert before dispatching or touches
	 * land in the wrong place and miss their targets. */
	int fb_w = 0, fb_h = 0, win_w = 0, win_h = 0;
	glfwGetFramebufferSize(window->glfw_window, &fb_w, &fb_h);
	glfwGetWindowSize(window->glfw_window, &win_w, &win_h);
	float scale_x = win_w > 0 ? (float)fb_w / win_w : 1.0f;
	float scale_y = win_h > 0 ? (float)fb_h / win_h : 1.0f;
	float px = (float)window->pointer_x * scale_x;
	float py = (float)window->pointer_y * scale_y;
	if (getenv("ATL_DEBUG_INPUT"))
		fprintf(stderr, "ATLWindow: pointer action=%d window=(%.1f,%.1f) scale=(%.2f,%.2f) scene=(%.1f,%.1f) fb=%dx%d win=%dx%d\n",
		        action, window->pointer_x, window->pointer_y, scale_x, scale_y, px, py, fb_w, fb_h, win_w, win_h);
	jint id = 1;
	jfloat values[4] = {px, py, px, py};
	jintArray ids = (*env)->NewIntArray(env, 1);
	jfloatArray coords = (*env)->NewFloatArray(env, 4);
	(*env)->SetIntArrayRegion(env, ids, 0, 1, &id);
	(*env)->SetFloatArrayRegion(env, coords, 0, 4, values);
	jobject motion_event = (*env)->NewObject(env, handle_cache.motion_event.class, handle_cache.motion_event.constructor,
	                                         SOURCE_TOUCHSCREEN, action, atl_uptime_millis(), ids, coords);
	jboolean handled = (*env)->CallBooleanMethod(env, window->view_root, window->dispatch_touch_event, motion_event);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	if (getenv("ATL_DEBUG_INPUT"))
		fprintf(stderr, "ATLWindow: dispatchTouchEvent handled=%d\n", handled);
	(*env)->DeleteLocalRef(env, motion_event);
	(*env)->DeleteLocalRef(env, ids);
	(*env)->DeleteLocalRef(env, coords);
}

static void on_cursor_pos(GLFWwindow *glfw_window, double x, double y)
{
	ATLWindow *window = glfwGetWindowUserPointer(glfw_window);
	window->pointer_x = x;
	window->pointer_y = y;
	if (window->pointer_down)
		dispatch_pointer_event(window, ACTION_MOVE);
}

static void on_mouse_button(GLFWwindow *glfw_window, int button, int action, int mods)
{
	ATLWindow *window = glfwGetWindowUserPointer(glfw_window);
	if (button != GLFW_MOUSE_BUTTON_LEFT)
		return;
	if (action == GLFW_PRESS) {
		window->pointer_down = true;
		dispatch_pointer_event(window, ACTION_DOWN);
	} else if (action == GLFW_RELEASE) {
		dispatch_pointer_event(window, ACTION_UP);
		window->pointer_down = false;
	}
}

#define KEYCODE_BACK        4
#define KEYCODE_0           7
#define KEYCODE_DPAD_UP     19
#define KEYCODE_DPAD_DOWN   20
#define KEYCODE_DPAD_LEFT   21
#define KEYCODE_DPAD_RIGHT  22
#define KEYCODE_A           29
#define KEYCODE_COMMA       55
#define KEYCODE_PERIOD      56
#define KEYCODE_TAB         61
#define KEYCODE_SPACE       62
#define KEYCODE_ENTER       66
#define KEYCODE_DEL         67
#define KEYCODE_PAGE_UP     92
#define KEYCODE_PAGE_DOWN   93
#define KEYCODE_FORWARD_DEL 112
#define KEYCODE_MOVE_HOME   122
#define KEYCODE_MOVE_END    123
#define KEYCODE_INSERT      124
#define KEYCODE_F1          131
#define KEYCODE_NUMPAD_0    144

#define META_SHIFT_ON (1 << 0)
#define META_ALT_ON   (1 << 1)
#define META_CTRL_ON  (1 << 12)

static int map_key_code(int key)
{
	if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
		return key - GLFW_KEY_0 + KEYCODE_0;
	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
		return key - GLFW_KEY_A + KEYCODE_A;
	if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12)
		return key - GLFW_KEY_F1 + KEYCODE_F1;
	if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
		return key - GLFW_KEY_KP_0 + KEYCODE_NUMPAD_0;
	switch (key) {
		case GLFW_KEY_ESCAPE: return KEYCODE_BACK;
		case GLFW_KEY_UP: return KEYCODE_DPAD_UP;
		case GLFW_KEY_DOWN: return KEYCODE_DPAD_DOWN;
		case GLFW_KEY_LEFT: return KEYCODE_DPAD_LEFT;
		case GLFW_KEY_RIGHT: return KEYCODE_DPAD_RIGHT;
		case GLFW_KEY_COMMA: return KEYCODE_COMMA;
		case GLFW_KEY_PERIOD: return KEYCODE_PERIOD;
		case GLFW_KEY_TAB: return KEYCODE_TAB;
		case GLFW_KEY_SPACE: return KEYCODE_SPACE;
		case GLFW_KEY_ENTER: return KEYCODE_ENTER;
		case GLFW_KEY_KP_ENTER: return KEYCODE_ENTER;
		case GLFW_KEY_BACKSPACE: return KEYCODE_DEL;
		case GLFW_KEY_DELETE: return KEYCODE_FORWARD_DEL;
		case GLFW_KEY_PAGE_UP: return KEYCODE_PAGE_UP;
		case GLFW_KEY_PAGE_DOWN: return KEYCODE_PAGE_DOWN;
		case GLFW_KEY_HOME: return KEYCODE_MOVE_HOME;
		case GLFW_KEY_END: return KEYCODE_MOVE_END;
		case GLFW_KEY_INSERT: return KEYCODE_INSERT;
		default: return 0;
	}
}

static int key_unicode(int key, int mods)
{
	if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
		return '0' + key - GLFW_KEY_0;
	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
		return ((mods & GLFW_MOD_SHIFT) ? 'A' : 'a') + key - GLFW_KEY_A;
	if (key == GLFW_KEY_SPACE)
		return ' ';
	return 0;
}

static void on_key(GLFWwindow *glfw_window, int key, int scancode, int action, int mods)
{
	ATLWindow *window = glfwGetWindowUserPointer(glfw_window);
	if (!window->view_root)
		return;
	JNIEnv *env = get_jni_env();
	int meta_state = ((mods & GLFW_MOD_SHIFT) ? META_SHIFT_ON : 0) |
	                 ((mods & GLFW_MOD_CONTROL) ? META_CTRL_ON : 0) |
	                 ((mods & GLFW_MOD_ALT) ? META_ALT_ON : 0);
	// PRESS and REPEAT both dispatch a key-down so held control keys (Backspace,
	// arrows, Delete) auto-repeat; printable repeats arrive via the char callback.
	int repeat_count = (action == GLFW_REPEAT) ? 1 : 0;
	jobject key_event = (*env)->NewObject(env, handle_cache.key_event.class, handle_cache.key_event.constructor,
	                                      (jlong)0, (jlong)0, action == GLFW_RELEASE ? KEY_ACTION_UP : KEY_ACTION_DOWN,
	                                      map_key_code(key), repeat_count, meta_state);
	_SET_INT_FIELD(key_event, "unicodeValue", key_unicode(key, mods));
	(*env)->CallBooleanMethod(env, window->view_root, window->dispatch_key_event, key_event);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	(*env)->DeleteLocalRef(env, key_event);
}

/* Text entry: GLFW delivers the final Unicode codepoint here after applying the
 * OS keyboard layout (Cyrillic, dead keys, AltGr, etc.), so this — not the key
 * callback's keycode mapping — is what produces typed characters. Control keys
 * (backspace, arrows, enter) do not generate char events and stay on on_key. */
static void on_char(GLFWwindow *glfw_window, unsigned int codepoint)
{
	ATLWindow *window = glfwGetWindowUserPointer(glfw_window);
	if (!window->view_root || !window->dispatch_character)
		return;
	JNIEnv *env = get_jni_env();
	(*env)->CallBooleanMethod(env, window->view_root, window->dispatch_character, (jint)codepoint);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
}

static void on_framebuffer_size(GLFWwindow *glfw_window, int width, int height)
{
	ATLWindow *window = glfwGetWindowUserPointer(glfw_window);
	window->needs_redraw = true;
}

static void on_window_close(GLFWwindow *glfw_window)
{
	fprintf(stderr, "ATLWindow: window close event from compositor -> closing all activities and exiting\n");
	activity_close_all();
	exit(0);
}

/* --- rendering: raster Skia scene blitted through GL --- */

/* Per-phase frame timing, gated behind ATL_DEBUG_RENDER. Layout and draw are
 * reported separately: they are very different costs (the first layout of a
 * text-heavy tree is ~1s cold, while the actual raster draw is a couple of ms),
 * and lumping them together as "draw" is misleading. */
static bool debug_render(void)
{
	static int cached = -1;
	if (cached < 0)
		cached = getenv("ATL_DEBUG_RENDER") != NULL;
	return cached;
}

static void atl_window_render(ATLWindow *window)
{
	if (!window->view_root || !glfwGetWindowAttrib(window->glfw_window, GLFW_VISIBLE))
		return;
	int width, height;
	glfwGetFramebufferSize(window->glfw_window, &width, &height);
	if (width < 1 || height < 1)
		return;
	JNIEnv *env = get_jni_env();
	jlong layout_ms = 0;
	if (width != window->layout_width || height != window->layout_height) {
		window->layout_width = width;
		window->layout_height = height;
		jlong t = debug_render() ? atl_uptime_millis() : 0;
		(*env)->CallVoidMethod(env, window->view_root, window->perform_layout, width, height);
		if ((*env)->ExceptionCheck(env))
			(*env)->ExceptionDescribe(env);
		if (debug_render())
			layout_ms = atl_uptime_millis() - t;
	}

	jlong t_draw = debug_render() ? atl_uptime_millis() : 0;
	void *canvas = atl_canvas_new_raster(width, height);
	(*env)->CallVoidMethod(env, window->view_root, window->perform_draw, _INTPTR(canvas), width, height);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	jlong draw_ms = debug_render() ? atl_uptime_millis() - t_draw : 0;

	int pixel_width, pixel_height, stride;
	const void *pixels = atl_canvas_get_pixels(canvas, &pixel_width, &pixel_height, &stride);

	glfwMakeContextCurrent(window->glfw_window);
	if (!window->gl_texture) {
		glGenTextures(1, &window->gl_texture);
		glBindTexture(GL_TEXTURE_2D, window->gl_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	} else {
		glBindTexture(GL_TEXTURE_2D, window->gl_texture);
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixel_width, pixel_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glViewport(0, 0, width, height);
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(0, 1);
	glVertex2f(-1, -1);
	glTexCoord2f(1, 1);
	glVertex2f(1, -1);
	glTexCoord2f(0, 0);
	glVertex2f(-1, 1);
	glTexCoord2f(1, 0);
	glVertex2f(1, 1);
	glEnd();
	glfwSwapBuffers(window->glfw_window);

	atl_canvas_free(canvas);
	window->needs_redraw = false;

	/* Only a handful of frames should ever be slow (the first layout, genuine
	 * resizes); a steady stream of these means something is scheduling
	 * needless relayouts/redraws. */
	if (debug_render() && (layout_ms > 50 || draw_ms > 50))
		fprintf(stderr, "ATLWindow: slow frame: layout %lldms, draw %lldms (%dx%d)\n",
		        (long long)layout_ms, (long long)draw_ms, width, height);
}

/* --- event pump on the GLib main loop --- */

static gboolean atl_windows_tick(gpointer user_data)
{
	glfwPollEvents();
	for (ATLWindow *window = windows; window; window = window->next) {
		if (window->needs_redraw)
			atl_window_render(window);
	}
	return G_SOURCE_CONTINUE;
}

static void atl_glfw_error_callback(int code, const char *desc)
{
	fprintf(stderr, "GLFW error 0x%x: %s\n", code, desc);
}

void atl_windows_init(void)
{
	glfwSetErrorCallback(atl_glfw_error_callback);

	/* GLFW auto-detects Wayland vs X11; ATL_GLFW_PLATFORM=x11|wayland forces one
	 * (useful to fall back to XWayland, or to confirm which backend is in use) */
	const char *platform = getenv("ATL_GLFW_PLATFORM");
	if (platform) {
		if (!strcmp(platform, "x11"))
			glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
		else if (!strcmp(platform, "wayland"))
			glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
	}

	/* Do NOT let GLFW load libdecor for Wayland window decorations: libdecor's
	 * GTK plugin pulls in GTK3, and ATL already has GTK4 loaded (GApplication,
	 * libandroid), so both try to register types like GtkWidget into one
	 * process -> GType meltdown ("cannot register existing type 'GtkWidget'")
	 * and the app hangs. We draw our own chrome anyway; the compositor can
	 * still provide server-side decorations via xdg-decoration. */
	glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);

	if (!glfwInit()) {
		const char *desc = NULL;
		glfwGetError(&desc);
		fprintf(stderr, "atl_windows_init: glfwInit failed: %s\n", desc ? desc : "(no detail)");
		exit(1);
	}
	fprintf(stderr, "ATLWindow: GLFW platform = %s\n",
	        glfwGetPlatform() == GLFW_PLATFORM_WAYLAND ? "wayland" :
	        glfwGetPlatform() == GLFW_PLATFORM_X11 ? "x11" : "other");
	g_timeout_add(4, atl_windows_tick, NULL);
}

ATLWindow *atl_window_new(int width, int height, bool visible, bool decorated)
{
	ATLWindow *window = calloc(1, sizeof(ATLWindow));
	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_DECORATED, decorated ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	/* WPE WebKit (android.webkit.WebView) needs an EGLDisplay to share its
	 * rendered EGLImages with. On Wayland GLFW only ever uses EGL, so
	 * glfwGetEGLDisplay() works and WebView is available. On X11 GLFW defaults
	 * to GLX (glfwGetEGLDisplay() then returns EGL_NO_DISPLAY and WebView
	 * disables itself gracefully); ATL_WEBVIEW_EGL=1 forces EGL there too for
	 * users who want WebView on X11, at the cost of switching the GL backend. */
	if (getenv("ATL_WEBVIEW_EGL"))
		glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
	window->glfw_window = glfwCreateWindow(width, height, "android-translation-layer", NULL, NULL);
	if (!window->glfw_window) {
		const char *desc = NULL;
		glfwGetError(&desc);
		fprintf(stderr, "atl_window_new: glfwCreateWindow failed: %s\n", desc ? desc : "(no detail)");
		exit(1);
	}
	glfwSetWindowUserPointer(window->glfw_window, window);
	glfwSetCursorPosCallback(window->glfw_window, on_cursor_pos);
	glfwSetMouseButtonCallback(window->glfw_window, on_mouse_button);
	glfwSetKeyCallback(window->glfw_window, on_key);
	glfwSetCharCallback(window->glfw_window, on_char);
	glfwSetFramebufferSizeCallback(window->glfw_window, on_framebuffer_size);
	glfwSetWindowCloseCallback(window->glfw_window, on_window_close);
	glfwMakeContextCurrent(window->glfw_window);
	glfwSwapInterval(0); // frame pacing comes from the render tick, don't block on vsync

	/* Commit an initial frame immediately. On Wayland a surface is not mapped
	 * until its first buffer is committed; without this the window would stay
	 * invisible until the app's first real frame (which only happens once a
	 * ViewRootImpl attaches, several seconds into startup) — or never, if the
	 * app never draws. On X11 the window maps regardless, so this is harmless. */
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glfwSwapBuffers(window->glfw_window);
	glfwPollEvents();

	window->next = windows;
	windows = window;
	return window;
}

/* --- accessors for the WPE WebView offscreen integration --- */

/* the EGLDisplay GLFW created its contexts on; WPE renders its EGLImages here */
void *atl_primary_egl_display(void)
{
	return (void *)glfwGetEGLDisplay();
}

/* make a GL context current on the calling (main) thread so WebView can bind a
 * WPE-exported EGLImage to a texture and read it back. Any window's context
 * works since they share the same EGLDisplay; the render tick re-makes its own
 * context current before blitting, so leaving this one current is harmless. */
void atl_primary_make_context_current(void)
{
	if (windows)
		glfwMakeContextCurrent(windows->glfw_window);
}

/* schedule a repaint (e.g. when a WebView has a fresh frame to composite) */
void atl_window_invalidate_all(void)
{
	for (ATLWindow *window = windows; window; window = window->next)
		window->needs_redraw = true;
}

void atl_window_set_view_root(ATLWindow *window, JNIEnv *env, jobject view_root)
{
	if (window->view_root)
		(*env)->DeleteGlobalRef(env, window->view_root);
	window->view_root = (*env)->NewGlobalRef(env, view_root);
	jclass view_root_class = (*env)->GetObjectClass(env, view_root);
	window->perform_layout = (*env)->GetMethodID(env, view_root_class, "performLayout", "(II)V");
	window->perform_draw = (*env)->GetMethodID(env, view_root_class, "performDraw", "(JII)V");
	window->dispatch_touch_event = (*env)->GetMethodID(env, view_root_class, "dispatchTouchEvent", "(Landroid/view/MotionEvent;)Z");
	window->dispatch_key_event = (*env)->GetMethodID(env, view_root_class, "dispatchKeyEvent", "(Landroid/view/KeyEvent;)Z");
	window->dispatch_character = (*env)->GetMethodID(env, view_root_class, "dispatchCharacter", "(I)Z");
	(*env)->SetLongField(env, view_root, (*env)->GetFieldID(env, view_root_class, "scene", "J"), _INTPTR(window));
	window->layout_width = window->layout_height = 0; // force a layout pass
	window->needs_redraw = true;
}

void atl_window_set_title(ATLWindow *window, const char *title)
{
	glfwSetWindowTitle(window->glfw_window, title);
}

void atl_window_set_default_size(ATLWindow *window, int width, int height)
{
	if (width > 0 && height > 0)
		glfwSetWindowSize(window->glfw_window, width, height);
}

void atl_window_show(ATLWindow *window)
{
	glfwShowWindow(window->glfw_window);
	window->needs_redraw = true;
}

void atl_window_hide(ATLWindow *window)
{
	glfwHideWindow(window->glfw_window);
}

bool atl_window_is_visible(ATLWindow *window)
{
	return glfwGetWindowAttrib(window->glfw_window, GLFW_VISIBLE);
}

void atl_window_focus(ATLWindow *window)
{
	glfwFocusWindow(window->glfw_window);
}

void atl_window_set_clipboard(ATLWindow *window, const char *text)
{
	glfwSetClipboardString(window->glfw_window, text);
}

/* returns a string owned by GLFW, valid until the next clipboard call */
const char *atl_window_get_clipboard(ATLWindow *window)
{
	return glfwGetClipboardString(window->glfw_window);
}

bool atl_window_is_maximized(ATLWindow *window)
{
	return glfwGetWindowAttrib(window->glfw_window, GLFW_MAXIMIZED);
}

bool atl_screen_size(int *width, int *height)
{
	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
	if (!monitor)
		return false;
	const GLFWvidmode *mode = glfwGetVideoMode(monitor);
	if (!mode)
		return false;
	*width = mode->width;
	*height = mode->height;
	return true;
}

int atl_window_get_width(ATLWindow *window)
{
	int width, height;
	glfwGetFramebufferSize(window->glfw_window, &width, &height);
	return width;
}

void atl_window_set_jobject(ATLWindow *window, JNIEnv *env, jobject window_obj)
{
	window->window_jobj = (*env)->NewWeakGlobalRef(env, window_obj);
}

jobject atl_window_get_jobject(ATLWindow *window)
{
	return window->window_jobj;
}

/* --- android.view.ViewRootImpl natives (may be called from any thread) --- */

static gboolean invalidate_cb(gpointer data)
{
	((ATLWindow *)data)->needs_redraw = true;
	return G_SOURCE_REMOVE;
}

JNIEXPORT void JNICALL Java_android_view_ViewRootImpl_nativeInvalidate(JNIEnv *env, jclass class, jlong window_ptr)
{
	g_idle_add_full(G_PRIORITY_HIGH_IDLE + 20, invalidate_cb, _PTR(window_ptr), NULL);
}

static gboolean request_layout_cb(gpointer data)
{
	ATLWindow *window = data;
	window->layout_width = window->layout_height = 0;
	window->needs_redraw = true;
	return G_SOURCE_REMOVE;
}

JNIEXPORT void JNICALL Java_android_view_ViewRootImpl_nativeRequestLayout(JNIEnv *env, jclass class, jlong window_ptr)
{
	g_idle_add_full(G_PRIORITY_HIGH_IDLE + 10, request_layout_cb, _PTR(window_ptr), NULL);
}
