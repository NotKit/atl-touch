#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <string.h>
#include <wayland-client.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_EGL
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#include <glib.h>

#include "defines.h"
#include "util.h"

#include "ATLWindow.h"
#include "graphics/ATLCanvas.h"

#include "generated_headers/android_view_ViewRootImpl.h"

#include "viewporter-client-protocol.h"
#include "widgets/atl_surface_layer.h"
#include "../libandroid/native_window.h"

struct ATLWindow {
	GLFWwindow *glfw_window;
	jobject view_root; // global ref, NULL until attached
	jmethodID perform_layout;
	jmethodID set_ime_inset;
	jmethodID dispatch_configuration_changed;
	jmethodID perform_draw;
	jmethodID dispatch_touch_event;
	jmethodID dispatch_generic_motion_event;
	jmethodID dispatch_key_event;
	jmethodID dispatch_character;
	jmethodID dispatch_commit_text;
	jmethodID dispatch_composing_text;
	jmethodID dispatch_finish_composing;
	jmethodID dispatch_ime_set_selection;
	jmethodID dispatch_ime_get_selection;
	jmethodID dispatch_im_initiated_hide;
	jmethodID dispatch_window_focus_changed;
	jobject window_jobj;    // weak ref to the java android.view.Window
	bool needs_redraw;
	/* redraw everything: set by native-initiated invalidations (resize, show,
	 * WebView frames, ...) that have no ViewRootImpl.mDirty damage rect */
	bool full_redraw;
	bool pointer_down;
	double pointer_x, pointer_y;
	int layout_width, layout_height;
	/* persistent surface (raster or GPU): pixels outside a frame's damage rect
	 * keep their previous contents, which partial redraws rely on */
	void *canvas;
	int canvas_width, canvas_height;
	int tex_width, tex_height;
	void *gpu_context;   // GrDirectContext for this window's GL context
	bool canvas_is_gpu;
	bool gpu_failed;     // context/surface creation failed: stay on raster
	jfieldID dirty_field; // ViewRootImpl.mDirty (android.graphics.Rect)
	jfieldID rect_left, rect_top, rect_right, rect_bottom;
	unsigned int gl_texture;
	unsigned int gl_program;
	int gl_attr_pos, gl_attr_uv;
	/* the chrome sub-surface ATL renders into once this window has a
	 * SurfaceView (doc/SurfaceViewCompositing.md); the toplevel then only keeps
	 * a full-size opaque buffer for input and window management */
	void *chrome_egl_window;   /* struct wl_egl_window *: identity == generation */
	EGLSurface chrome_surface;
	/* GLFW's own EGL objects, read off the current context rather than through
	 * glfwGetEGLContext(): those only answer for GLFW_EGL_CONTEXT_API, and ATL
	 * lets GLFW pick the creation API (GLFW_NATIVE_CONTEXT_API on Wayland) */
	EGLContext glfw_context;
	EGLSurface glfw_surface;
	int toplevel_width, toplevel_height; /* size of the last buffer it was given */
	bool toplevel_resync;      /* its last commit went out at the previous size */
	struct ATLWindow *next;
};

static struct ATLWindow *windows = NULL;

/* --- ES2-compatible blit ---------------------------------------------
 * The fixed-function path (glBegin/glMatrixMode/...) does not exist on
 * OpenGL ES, which is all that hybris EGL offers on Ubuntu Touch. This
 * shader path works on both GLES 2.0 and desktop GL 2.1 contexts. */

static unsigned int atl_gl_compile(unsigned int type, const char *src)
{
	unsigned int shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);
	int ok = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512] = {0};
		glGetShaderInfoLog(shader, sizeof(log) - 1, NULL, log);
		fprintf(stderr, "ATLWindow: shader compile failed: %s\n", log);
	}
	return shader;
}

static unsigned int atl_gl_make_blit_program(int *attr_pos, int *attr_uv)
{
	static const char *vs_src =
		"attribute vec2 pos;\n"
		"attribute vec2 uv;\n"
		"varying vec2 v_uv;\n"
		"void main() { gl_Position = vec4(pos, 0.0, 1.0); v_uv = uv; }\n";
	static const char *fs_src =
		"#ifdef GL_ES\n"
		"precision mediump float;\n"
		"#endif\n"
		"varying vec2 v_uv;\n"
		"uniform sampler2D tex;\n"
		"void main() { gl_FragColor = texture2D(tex, v_uv); }\n";

	unsigned int program = glCreateProgram();
	glAttachShader(program, atl_gl_compile(GL_VERTEX_SHADER, vs_src));
	glAttachShader(program, atl_gl_compile(GL_FRAGMENT_SHADER, fs_src));
	glLinkProgram(program);
	int ok = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512] = {0};
		glGetProgramInfoLog(program, sizeof(log) - 1, NULL, log);
		fprintf(stderr, "ATLWindow: program link failed: %s\n", log);
	}
	*attr_pos = glGetAttribLocation(program, "pos");
	*attr_uv = glGetAttribLocation(program, "uv");
	glUseProgram(program);
	glUniform1i(glGetUniformLocation(program, "tex"), 0);
	return program;
}

extern void activity_close_all(void); // app/android_app_Activity.c

/* --- input dispatch --- */

#define ACTION_DOWN   0
#define ACTION_UP     1
#define ACTION_MOVE   2
#define ACTION_CANCEL 3
#define ACTION_POINTER_DOWN 5
#define ACTION_POINTER_UP   6
#define ACTION_SCROLL 8
/* ACTION_POINTER_DOWN/UP carry the index of the pointer that went down or up in
 * the high byte of the action (MotionEvent.ACTION_POINTER_INDEX_SHIFT). */
#define ACTION_POINTER_INDEX_SHIFT 8

#define ATL_MAX_CONTACTS 8

/* one live contact: `id` is what web content sees as Touch.identifier, `wl_id`
 * is the compositor's own numbering (which does not have to be dense) */
struct atl_contact {
	int32_t id;
	int32_t wl_id;
	double x, y;
};

#define SOURCE_TOUCHSCREEN 0x1002
#define SOURCE_MOUSE       0x2002

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

/* Build one MotionEvent carrying every live contact and hand it to
 * ViewRootImpl.dispatchTouchEvent. `action` is already packed with the pointer
 * index for ACTION_POINTER_DOWN/UP; the array must still hold ALL contacts,
 * including the one that is lifting, because the consumer reads the lifting
 * pointer out of the array by action index. */
static void dispatch_touch_contacts(ATLWindow *window, int action, int count, const struct atl_contact *c)
{
	if (!window->view_root || count <= 0 || count > ATL_MAX_CONTACTS)
		return;
	JNIEnv *env = get_jni_env();
	/* A pending exception (e.g. thrown by an app callback under a nested native
	 * frame that couldn't handle it) makes every JNI call below misbehave —
	 * object creation returns NULL and the dispatch call aborts the runtime
	 * ("with unexpected pending exception"). Surface and clear it instead. */
	atl_report_pending_exception(env);
	/* GLFW reports the cursor in window (logical) coordinates, but the scene is
	 * laid out and rendered in framebuffer pixels. On a scaled/HiDPI output the
	 * two differ by the content scale, so convert before dispatching or touches
	 * land in the wrong place and miss their targets. */
	int fb_w = 0, fb_h = 0, win_w = 0, win_h = 0;
	glfwGetFramebufferSize(window->glfw_window, &fb_w, &fb_h);
	glfwGetWindowSize(window->glfw_window, &win_w, &win_h);
	float scale_x = win_w > 0 ? (float)fb_w / win_w : 1.0f;
	float scale_y = win_h > 0 ? (float)fb_h / win_h : 1.0f;
	jint idv[ATL_MAX_CONTACTS];
	jfloat values[4 * ATL_MAX_CONTACTS];
	for (int i = 0; i < count; i++) {
		float px = (float)c[i].x * scale_x;
		float py = (float)c[i].y * scale_y;
		idv[i] = c[i].id;
		values[4 * i + 0] = px;
		values[4 * i + 1] = py;
		values[4 * i + 2] = px;
		values[4 * i + 3] = py;
	}
	if (getenv("ATL_DEBUG_INPUT")) {
		fprintf(stderr, "ATLWindow: pointer action=0x%x count=%d scale=(%.2f,%.2f) fb=%dx%d win=%dx%d",
		        action, count, scale_x, scale_y, fb_w, fb_h, win_w, win_h);
		for (int i = 0; i < count; i++)
			fprintf(stderr, " [%d id=%d scene=(%.1f,%.1f)]", i, idv[i], values[4 * i], values[4 * i + 1]);
		fprintf(stderr, "\n");
	}
	jintArray ids = (*env)->NewIntArray(env, count);
	jfloatArray coords = (*env)->NewFloatArray(env, 4 * count);
	(*env)->SetIntArrayRegion(env, ids, 0, count, idv);
	(*env)->SetFloatArrayRegion(env, coords, 0, 4 * count, values);
	jobject motion_event = (*env)->NewObject(env, handle_cache.motion_event.class, handle_cache.motion_event.constructor,
	                                         SOURCE_TOUCHSCREEN, action, atl_uptime_millis(), ids, coords);
	jboolean handled = (*env)->CallBooleanMethod(env, window->view_root, window->dispatch_touch_event, motion_event);
	atl_report_pending_exception(env);
	if (getenv("ATL_DEBUG_INPUT"))
		fprintf(stderr, "ATLWindow: dispatchTouchEvent handled=%d\n", handled);
	(*env)->DeleteLocalRef(env, motion_event);
	(*env)->DeleteLocalRef(env, ids);
	(*env)->DeleteLocalRef(env, coords);
}

/* the GLFW mouse path: one contact, at the cursor. Pointer id 0 so that a
 * second contact can be id 1 - APZ only ever matches identifiers against each
 * other, so the absolute value does not matter to it. */
static void dispatch_pointer_event(ATLWindow *window, int action)
{
	struct atl_contact c = { .id = 0, .wl_id = -1, .x = window->pointer_x, .y = window->pointer_y };
	dispatch_touch_contacts(window, action, 1, &c);
}

static void on_cursor_pos(GLFWwindow *glfw_window, double x, double y)
{
	ATLWindow *window = glfwGetWindowUserPointer(glfw_window);
	window->pointer_x = x;
	window->pointer_y = y;
	if (window->pointer_down)
		dispatch_pointer_event(window, ACTION_MOVE);
}

/* Wheel/two-finger scroll. Android delivers this as a SOURCE_MOUSE
 * ACTION_SCROLL event on the generic-motion path, not the touch path, with the
 * deltas on the HSCROLL/VSCROLL axes. Nothing dispatched it before, so a wheel
 * did nothing in any app. */
static void on_scroll(GLFWwindow *glfw_window, double dx, double dy)
{
	ATLWindow *window = glfwGetWindowUserPointer(glfw_window);
	if (!window->view_root || !window->dispatch_generic_motion_event)
		return;
	JNIEnv *env = get_jni_env();
	atl_report_pending_exception(env);
	int fb_w = 0, fb_h = 0, win_w = 0, win_h = 0;
	glfwGetFramebufferSize(window->glfw_window, &fb_w, &fb_h);
	glfwGetWindowSize(window->glfw_window, &win_w, &win_h);
	float scale_x = win_w > 0 ? (float)fb_w / win_w : 1.0f;
	float scale_y = win_h > 0 ? (float)fb_h / win_h : 1.0f;
	float px = (float)window->pointer_x * scale_x;
	float py = (float)window->pointer_y * scale_y;
	if (getenv("ATL_DEBUG_INPUT"))
		fprintf(stderr, "ATLWindow: scroll d=(%.2f,%.2f) at (%.1f,%.1f)\n", dx, dy, px, py);
	jobject motion_event = (*env)->NewObject(env, handle_cache.motion_event.class,
	                                         handle_cache.motion_event.constructor_scroll,
	                                         SOURCE_MOUSE, ACTION_SCROLL, atl_uptime_millis(),
	                                         px, py, px, py, (jfloat)dx, (jfloat)dy);
	jboolean handled = (*env)->CallBooleanMethod(env, window->view_root,
	                                             window->dispatch_generic_motion_event, motion_event);
	atl_report_pending_exception(env);
	if (getenv("ATL_DEBUG_INPUT"))
		fprintf(stderr, "ATLWindow: dispatchGenericMotionEvent handled=%d\n", handled);
	(*env)->DeleteLocalRef(env, motion_event);
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

/* --- touch input, one or more contacts ---------------------------------
 * GLFW has no touch API; its wayland backend only listens to wl_pointer,
 * and Lomiri/Mir does not emulate a pointer for finger input. So we bind our
 * own wl_touch from the seat.
 *
 * The contact table below and the ACTION_POINTER_DOWN/UP packing are shared:
 * the wl_touch callbacks are thin converters over atl_touch_{add,move,release}
 * and so are the ATL_DEBUG_PINCH and ATL_DEBUG_TAP drivers, which are the only
 * things that can drive contacts on a host with no touch device (see
 * debug_pinch and debug_tap).
 */

static ATLWindow *atl_touch_window;
static struct atl_contact atl_touches[ATL_MAX_CONTACTS];
static int atl_touch_count;

static ATLWindow *atl_window_from_wl_surface(struct wl_surface *surface)
{
	for (ATLWindow *w = windows; w; w = w->next)
		if (glfwGetWaylandWindow(w->glfw_window) == surface)
			return w;
	return NULL;
}

static int atl_touch_slot(int32_t wl_id)
{
	for (int i = 0; i < atl_touch_count; i++)
		if (atl_touches[i].wl_id == wl_id)
			return i;
	return -1;
}

/* Touch.identifier stays put for the life of a contact, so pick the lowest
 * free one rather than the slot index: slots shift when an earlier finger
 * lifts, and APZ tracks its cached touches by identifier. */
static int32_t atl_touch_alloc_id(void)
{
	for (int32_t cand = 0; cand < ATL_MAX_CONTACTS; cand++) {
		bool used = false;
		for (int i = 0; i < atl_touch_count; i++)
			if (atl_touches[i].id == cand)
				used = true;
		if (!used)
			return cand;
	}
	return -1;
}

/* contact 0 mirrored into the GLFW pointer state, so the mouse path and the
 * touch path do not disagree about where the pointer is */
static void atl_touch_mirror_pointer(void)
{
	if (!atl_touch_window)
		return;
	atl_touch_window->pointer_down = atl_touch_count > 0;
	if (atl_touch_count > 0) {
		atl_touch_window->pointer_x = atl_touches[0].x;
		atl_touch_window->pointer_y = atl_touches[0].y;
	}
}

static void atl_touch_add(ATLWindow *window, int32_t wl_id, double x, double y)
{
	if (atl_touch_count && window != atl_touch_window)
		return;
	if (atl_touch_count >= ATL_MAX_CONTACTS || atl_touch_slot(wl_id) >= 0)
		return;
	atl_touch_window = window;
	int slot = atl_touch_count;
	atl_touches[slot].id = atl_touch_alloc_id();
	atl_touches[slot].wl_id = wl_id;
	atl_touches[slot].x = x;
	atl_touches[slot].y = y;
	atl_touch_count++;
	atl_touch_mirror_pointer();
	int action = slot == 0 ? ACTION_DOWN
	                       : (ACTION_POINTER_DOWN | (slot << ACTION_POINTER_INDEX_SHIFT));
	dispatch_touch_contacts(window, action, atl_touch_count, atl_touches);
}

static void atl_touch_move(int32_t wl_id, double x, double y)
{
	int slot = atl_touch_slot(wl_id);
	if (slot < 0 || !atl_touch_window)
		return;
	atl_touches[slot].x = x;
	atl_touches[slot].y = y;
	atl_touch_mirror_pointer();
	dispatch_touch_contacts(atl_touch_window, ACTION_MOVE, atl_touch_count, atl_touches);
}

static void atl_touch_release(int32_t wl_id)
{
	int slot = atl_touch_slot(wl_id);
	if (slot < 0 || !atl_touch_window)
		return;
	ATLWindow *window = atl_touch_window;
	/* dispatch BEFORE compacting: ACTION_POINTER_UP names the lifting pointer
	 * by index into the array, so it has to still be in there. */
	int action = atl_touch_count == 1 ? ACTION_UP
	                                  : (ACTION_POINTER_UP | (slot << ACTION_POINTER_INDEX_SHIFT));
	dispatch_touch_contacts(window, action, atl_touch_count, atl_touches);
	for (int i = slot; i + 1 < atl_touch_count; i++)
		atl_touches[i] = atl_touches[i + 1];
	atl_touch_count--;
	atl_touch_mirror_pointer();
	if (!atl_touch_count) {
		window->pointer_down = false;
		atl_touch_window = NULL;
	}
}

static void atl_touch_cancel_all(void)
{
	if (!atl_touch_window || !atl_touch_count)
		return;
	dispatch_touch_contacts(atl_touch_window, ACTION_CANCEL, atl_touch_count, atl_touches);
	atl_touch_window->pointer_down = false;
	atl_touch_window = NULL;
	atl_touch_count = 0;
}

static void atl_wl_touch_down(void *data, struct wl_touch *touch, uint32_t serial,
                              uint32_t time, struct wl_surface *surface, int32_t id,
                              wl_fixed_t x, wl_fixed_t y)
{
	ATLWindow *window = atl_window_from_wl_surface(surface);
	if (!window)
		return;
	atl_touch_add(window, id, wl_fixed_to_double(x), wl_fixed_to_double(y));
}

static void atl_wl_touch_up(void *data, struct wl_touch *touch, uint32_t serial,
                            uint32_t time, int32_t id)
{
	atl_touch_release(id);
}

static void atl_wl_touch_motion(void *data, struct wl_touch *touch, uint32_t time,
                                int32_t id, wl_fixed_t x, wl_fixed_t y)
{
	atl_touch_move(id, wl_fixed_to_double(x), wl_fixed_to_double(y));
}

static void atl_wl_touch_frame(void *data, struct wl_touch *touch)
{
}

static void atl_wl_touch_cancel(void *data, struct wl_touch *touch)
{
	atl_touch_cancel_all();
}

static const struct wl_touch_listener atl_wl_touch_listener = {
	.down = atl_wl_touch_down,
	.up = atl_wl_touch_up,
	.motion = atl_wl_touch_motion,
	.frame = atl_wl_touch_frame,
	.cancel = atl_wl_touch_cancel,
};

static void atl_wl_seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
	static struct wl_touch *touch;
	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !touch) {
		touch = wl_seat_get_touch(seat);
		wl_touch_add_listener(touch, &atl_wl_touch_listener, NULL);
	}
}

static void atl_wl_seat_name(void *data, struct wl_seat *seat, const char *name)
{
}

static const struct wl_seat_listener atl_wl_seat_listener = {
	.capabilities = atl_wl_seat_capabilities,
	.name = atl_wl_seat_name,
};

static struct wl_compositor *wayland_compositor;
static struct wl_subcompositor *wayland_subcompositor;
static struct wp_viewporter *wayland_viewporter;

static void atl_wl_registry_global(void *data, struct wl_registry *registry, uint32_t name,
                                   const char *interface, uint32_t version)
{
	static struct wl_seat *seat;
	if (!strcmp(interface, wl_seat_interface.name) && !seat) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface,
		                        version < 5 ? version : 5);
		wl_seat_add_listener(seat, &atl_wl_seat_listener, NULL);
	} else if (!strcmp(interface, wl_compositor_interface.name) && !wayland_compositor) {
		wayland_compositor = wl_registry_bind(registry, name, &wl_compositor_interface,
		                                      version < 6 ? version : 6);
	} else if (!strcmp(interface, wl_subcompositor_interface.name) && !wayland_subcompositor) {
		wayland_subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
	} else if (!strcmp(interface, wp_viewporter_interface.name) && !wayland_viewporter) {
		wayland_viewporter = wl_registry_bind(registry, name, &wp_viewporter_interface, 1);
	}
}

struct wl_compositor *atl_wayland_compositor(void) { return wayland_compositor; }
struct wl_subcompositor *atl_wayland_subcompositor(void) { return wayland_subcompositor; }
struct wp_viewporter *atl_wayland_viewporter(void) { return wayland_viewporter; }

static void atl_wl_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener atl_wl_registry_listener = {
	.global = atl_wl_registry_global,
	.global_remove = atl_wl_registry_global_remove,
};

static void atl_wayland_init(void)
{
	static bool done;
	if (done)
		return;
	done = true;
	struct wl_display *display = glfwGetWaylandDisplay();
	if (!display)
		return;
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &atl_wl_registry_listener, NULL);
	wl_display_roundtrip(display); /* deliver globals (binds the seat) */
	wl_display_roundtrip(display); /* deliver seat capabilities */
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
	if (getenv("ATL_DEBUG_INPUT"))
		fprintf(stderr, "ATLWindow: key key=%d action=%d mods=%d root=%p\n",
		        key, action, mods, (void *)window->view_root);
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
	atl_report_pending_exception(env);
	(*env)->DeleteLocalRef(env, key_event);
}

/* Text entry: GLFW delivers the final Unicode codepoint here after applying the
 * OS keyboard layout (Cyrillic, dead keys, AltGr, etc.), so this — not the key
 * callback's keycode mapping — is what produces typed characters. Control keys
 * (backspace, arrows, enter) do not generate char events and stay on on_key. */
static void on_char(GLFWwindow *glfw_window, unsigned int codepoint)
{
	ATLWindow *window = glfwGetWindowUserPointer(glfw_window);
	if (getenv("ATL_DEBUG_INPUT"))
		fprintf(stderr, "ATLWindow: char U+%04X root=%p\n", codepoint,
		        (void *)window->view_root);
	if (!window->view_root || !window->dispatch_character)
		return;
	JNIEnv *env = get_jni_env();
	(*env)->CallBooleanMethod(env, window->view_root, window->dispatch_character, (jint)codepoint);
	atl_report_pending_exception(env);
}

static bool debug_chrome(void);

static void on_framebuffer_size(GLFWwindow *glfw_window, int width, int height)
{
	ATLWindow *window = glfwGetWindowUserPointer(glfw_window);
	/* the only place a compositor-driven resize is visible in ATL's own log:
	 * everything downstream just reads glfwGetFramebufferSize() */
	if (debug_chrome())
		fprintf(stderr, "ATLWindow: framebuffer size now %dx%d (was %dx%d)\n",
		        width, height, window->layout_width, window->layout_height);
	window->needs_redraw = true;
	window->full_redraw = true;
}

static void atl_window_notify_focus(ATLWindow *window, bool has_focus);

static void on_window_focus(GLFWwindow *glfw_window, int focused)
{
	ATLWindow *window = glfwGetWindowUserPointer(glfw_window);
	if (window == windows) /* the window IME events are routed to, see atl_window_hide */
		atl_window_notify_focus(window, focused != 0);
}

static void on_window_close(GLFWwindow *glfw_window)
{
	fprintf(stderr, "ATLWindow: window close event from compositor -> closing all activities and exiting\n");
	activity_close_all();
	exit(0);
}

/* --- IME event injection (used by input method backends) --- */

void atl_windows_ime_commit_text(const char *utf8, int replace_start, int replace_length, int cursor_pos)
{
	ATLWindow *window = windows;
	if (!window || !window->view_root || !utf8)
		return;
	JNIEnv *env = get_jni_env();
	jstring str = utf8_to_jstring(env, utf8);
	(*env)->CallBooleanMethod(env, window->view_root, window->dispatch_commit_text, str,
	                          replace_start, replace_length, cursor_pos);
	atl_report_pending_exception(env);
	(*env)->DeleteLocalRef(env, str);
}

void atl_windows_ime_set_composing(const char *utf8, int replace_start, int replace_length, int cursor_pos)
{
	ATLWindow *window = windows;
	if (!window || !window->view_root || !utf8)
		return;
	JNIEnv *env = get_jni_env();
	jstring str = utf8_to_jstring(env, utf8);
	(*env)->CallBooleanMethod(env, window->view_root, window->dispatch_composing_text, str,
	                          replace_start, replace_length, cursor_pos);
	atl_report_pending_exception(env);
	(*env)->DeleteLocalRef(env, str);
}

void atl_windows_ime_finish_composing(void)
{
	ATLWindow *window = windows;
	if (!window || !window->view_root)
		return;
	JNIEnv *env = get_jni_env();
	(*env)->CallVoidMethod(env, window->view_root, window->dispatch_finish_composing);
	atl_report_pending_exception(env);
}

void atl_windows_ime_set_selection(int start, int length)
{
	ATLWindow *window = windows;
	if (!window || !window->view_root)
		return;
	JNIEnv *env = get_jni_env();
	(*env)->CallVoidMethod(env, window->view_root, window->dispatch_ime_set_selection, start, length);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
}

char *atl_windows_ime_get_selection(void)
{
	ATLWindow *window = windows;
	if (!window || !window->view_root)
		return NULL;
	JNIEnv *env = get_jni_env();
	jstring str = (*env)->CallObjectMethod(env, window->view_root, window->dispatch_ime_get_selection);
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		return NULL;
	}
	if (!str)
		return NULL;
	char *utf8 = jstring_to_utf8(env, str);
	(*env)->DeleteLocalRef(env, str);
	return utf8;
}

void atl_windows_ime_initiated_hide(void)
{
	ATLWindow *window = windows;
	if (!window || !window->view_root)
		return;
	JNIEnv *env = get_jni_env();
	(*env)->CallVoidMethod(env, window->view_root, window->dispatch_im_initiated_hide);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
}

/* The compositor moved keyboard focus, or the window was hidden. Both must
 * reach the input method: a context left active goes on receiving the commits
 * meant for whatever has focus now. */
static void atl_window_notify_focus(ATLWindow *window, bool has_focus)
{
	if (!window || !window->view_root)
		return;
	JNIEnv *env = get_jni_env();
	(*env)->CallVoidMethod(env, window->view_root, window->dispatch_window_focus_changed,
	                       has_focus ? JNI_TRUE : JNI_FALSE);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
}

void atl_windows_ime_key(int action, int keycode)
{
	ATLWindow *window = windows;
	if (!window || !window->view_root)
		return;
	JNIEnv *env = get_jni_env();
	jobject key_event = (*env)->NewObject(env, handle_cache.key_event.class, handle_cache.key_event.constructor,
	                                      (jlong)0, (jlong)0, action, keycode, 0, 0);
	(*env)->CallBooleanMethod(env, window->view_root, window->dispatch_key_event, key_event);
	atl_report_pending_exception(env);
	(*env)->DeleteLocalRef(env, key_event);
}

static int ime_inset = 0;

/* keyboard geometry is shell-specific and hard to observe on a device, so make
 * the inset traceable: ATL_DEBUG_IME=1 */
bool atl_debug_ime(void)
{
	static int cached = -1;
	if (cached < 0)
		cached = getenv("ATL_DEBUG_IME") != NULL;
	return cached;
}

void atl_windows_set_ime_inset(int inset)
{
	if (inset < 0)
		inset = 0;
	if (atl_debug_ime())
		fprintf(stderr, "atl_ime: inset %d -> %d\n", ime_inset, inset);
	if (ime_inset == inset)
		return;
	ime_inset = inset;
	for (ATLWindow *w = windows; w; w = w->next) {
		w->layout_width = 0; /* force relayout with the new inset */
		w->needs_redraw = true;
	}
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

/* ATL_NO_DAMAGE=1 disables damage-region rendering (full redraw+upload every
 * frame), as a fallback if partial redraw ever leaves artifacts */
static bool damage_enabled(void)
{
	static int cached = -1;
	if (cached < 0)
		cached = getenv("ATL_NO_DAMAGE") == NULL;
	return cached;
}

/* ATL_NO_GPU=1 disables Ganesh rendering (CPU raster + texture upload instead);
 * also the automatic fallback when a GPU context can't be created */
static bool gpu_enabled(void)
{
	static int cached = -1;
	if (cached < 0)
		cached = getenv("ATL_NO_GPU") == NULL;
	return cached;
}

static bool debug_damage(void)
{
	static int cached = -1;
	if (cached < 0)
		cached = getenv("ATL_DEBUG_DAMAGE") != NULL;
	return cached;
}

/* ATL_DEBUG_CHROME=1 traces the chrome sub-surface's presents */
static bool debug_chrome(void)
{
	static int cached = -1;
	if (cached < 0)
		cached = getenv("ATL_DEBUG_CHROME") != NULL;
	return cached;
}

/*
 * ATL_DEBUG_RESIZE=<seconds>[:<w>x<h>] - once, <seconds> after the first tick,
 * ask the compositor to resize this window (default 600x800).
 *
 * A phone shell configures a window once, when it maps, and never again, so the
 * resize path of the chrome/toplevel split had no way of being exercised on the
 * compositor it was written for. xdg_toplevel has no "please resize me", but a
 * size *limit* is a request the compositor answers with a fresh configure -
 * measured on Mir 1.8.3, where set_max_size alone is enough - and GLFW sends
 * both limits from glfwSetWindowSizeLimits(). Diagnostic only: nothing in ATL
 * sets this by itself.
 */
static bool debug_resize(int *w, int *h, double *after)
{
	static int cached = -1;
	static int rw = 600, rh = 800;
	static double rt;

	if (cached < 0) {
		const char *s = getenv("ATL_DEBUG_RESIZE");
		char *end = NULL;
		cached = 0;
		if (s && *s) {
			double t = strtod(s, &end);
			if (end != s && t >= 0) {
				cached = 1;
				rt = t;
				if (end && *end == ':') {
					int pw = 0, ph = 0;
					if (sscanf(end + 1, "%dx%d", &pw, &ph) == 2 && pw > 0 && ph > 0) {
						rw = pw;
						rh = ph;
					}
				}
			}
		}
	}
	if (!cached)
		return false;
	*w = rw;
	*h = rh;
	*after = rt;
	return true;
}

/* ATL_DEBUG_PINCH=<seconds>[:cx,cy,span0,span1,steps] - once, <seconds> after
 * the first tick, walk two contacts apart (or together) about (cx,cy) from
 * span0 to span1 pixels, one step every 8 ticks.
 *
 * This is an injector, not an input source, and it exists because a second
 * real contact cannot be produced on a headless host: the compositor is
 * started with no input devices, the only injection protocol available is
 * zwlr_virtual_pointer_v1 (a pointer, not a touchscreen), and there is no
 * virtual-touch protocol in wayland-protocols or wlroots at all. It drives the
 * same atl_touch_add/move/release the wl_touch callbacks drive, so everything
 * above the compositor socket - the contact table, the ACTION_POINTER_DOWN/UP
 * packing, the MotionEvent arrays - is the real code path. Diagnostic only.
 */
static bool debug_pinch(double *after, int *cx, int *cy, int *span0, int *span1, int *steps)
{
	static int cached = -1;
	static int pcx = 360, pcy = 800, ps0 = 120, ps1 = 600, pst = 10;
	static double pt;

	if (cached < 0) {
		const char *s = getenv("ATL_DEBUG_PINCH");
		char *end = NULL;
		cached = 0;
		if (s && *s) {
			double t = strtod(s, &end);
			if (end != s && t >= 0) {
				cached = 1;
				pt = t;
				if (end && *end == ':') {
					int a, b, c, d, e;
					if (sscanf(end + 1, "%d,%d,%d,%d,%d", &a, &b, &c, &d, &e) == 5 && e > 1) {
						pcx = a; pcy = b; ps0 = c; ps1 = d; pst = e;
					}
				}
			}
		}
	}
	if (!cached)
		return false;
	*after = pt;
	*cx = pcx;
	*cy = pcy;
	*span0 = ps0;
	*span1 = ps1;
	*steps = pst;
	return true;
}

/* one step of the injected two-contact sequence, driven off the 4 ms tick so
 * layout and draw keep running between the events (a real gesture is spread
 * over frames too, and APZ's pinch detector needs the moves to be separate). */
static void debug_pinch_step(ATLWindow *window, int step, int cx, int cy, int span0, int span1, int steps)
{
	double half0 = span0 / 2.0;
	if (step == 0) {
		atl_touch_add(window, 100, cx - half0, cy);
	} else if (step == 1) {
		atl_touch_add(window, 101, cx + half0, cy);
	} else if (step < 2 + steps) {
		int k = step - 2;
		double span = span0 + (span1 - span0) * (double)k / (steps - 1);
		atl_touch_move(100, cx - span / 2.0, cy);
		atl_touch_move(101, cx + span / 2.0, cy);
	} else if (step == 2 + steps) {
		atl_touch_release(101);
	} else {
		atl_touch_release(100);
	}
	fprintf(stderr, "ATLWindow: ATL_DEBUG_PINCH step=%d contacts=%d", step, atl_touch_count);
	for (int i = 0; i < atl_touch_count; i++)
		fprintf(stderr, " [id=%d %.0f,%.0f]", atl_touches[i].id, atl_touches[i].x, atl_touches[i].y);
	fprintf(stderr, "\n");
}

/* ATL_DEBUG_TAP=<seconds>:<x>,<y>[-<x>,<y>][;<seconds>:<x>,<y>...] - a schedule
 * of single contacts. At <seconds> after the first tick put a finger down at
 * (x,y) and lift it again, or, when a second point is given, drag from the
 * first to the second. The entries fire in order, so one run can walk several
 * screens.
 *
 * The same injector argument as debug_pinch above, with one contact instead of
 * two: on a headless host there is no touch device to tap with either, and the
 * one injection protocol that exists, zwlr_virtual_pointer_v1, is a wlroots
 * extension - Mir/Lomiri does not offer it, so on the phone this is the only
 * way to drive a tap from inside. Coordinates are window pixels, the space
 * dispatch_touch_contacts scales into scene=. Diagnostic only.
 */
#define ATL_DEBUG_TAP_MAX 16
#define ATL_DEBUG_TAP_DRAG_STEPS 10
#define ATL_DEBUG_TAP_WL_ID 200 /* not the pinch's 100/101 */

struct atl_debug_tap {
	double after;
	int x, y, x1, y1;
	int steps; /* moves from (x,y) to (x1,y1); 0 for a plain tap */
};

static int debug_tap(const struct atl_debug_tap **out)
{
	static struct atl_debug_tap taps[ATL_DEBUG_TAP_MAX];
	static int count = -1;

	if (count < 0) {
		const char *s = getenv("ATL_DEBUG_TAP");
		count = 0;
		while (s && *s && count < ATL_DEBUG_TAP_MAX) {
			char *end = NULL;
			double t = strtod(s, &end);
			int x, y, x1, y1, steps = 0;

			if (end == s || *end != ':' || t < 0)
				break;
			if (sscanf(end + 1, "%d,%d-%d,%d", &x, &y, &x1, &y1) == 4) {
				steps = ATL_DEBUG_TAP_DRAG_STEPS;
			} else if (sscanf(end + 1, "%d,%d", &x, &y) == 2) {
				x1 = x;
				y1 = y;
			} else {
				break;
			}
			taps[count].after = t;
			taps[count].x = x;
			taps[count].y = y;
			taps[count].x1 = x1;
			taps[count].y1 = y1;
			taps[count].steps = steps;
			count++;
			s = strchr(end, ';');
			if (s)
				s++;
		}
	}
	*out = taps;
	return count;
}

/* one step of one scheduled contact, paced off the tick like the pinch: down,
 * then the drag moves (a plain tap has none), then the lift. Spreading it over
 * frames is what makes it a gesture rather than two events in one frame. */
static void debug_tap_step(ATLWindow *window, const struct atl_debug_tap *tap, int step)
{
	if (step == 0) {
		atl_touch_add(window, ATL_DEBUG_TAP_WL_ID, tap->x, tap->y);
	} else if (step <= tap->steps) {
		double k = (double)step / tap->steps;
		atl_touch_move(ATL_DEBUG_TAP_WL_ID, tap->x + (tap->x1 - tap->x) * k,
		               tap->y + (tap->y1 - tap->y) * k);
	} else {
		atl_touch_release(ATL_DEBUG_TAP_WL_ID);
	}
	fprintf(stderr, "ATLWindow: ATL_DEBUG_TAP step=%d contacts=%d", step, atl_touch_count);
	for (int i = 0; i < atl_touch_count; i++)
		fprintf(stderr, " [id=%d %.0f,%.0f]", atl_touches[i].id, atl_touches[i].x, atl_touches[i].y);
	fprintf(stderr, "\n");
}

/* the EGLConfig GLFW's context was created with, so the chrome's window surface
 * is context-compatible without guessing at attributes */
static bool atl_window_egl_config(EGLDisplay display, EGLContext context, EGLConfig *out)
{
	EGLint config_id = 0, count = 0;
	EGLint attrs[] = { EGL_CONFIG_ID, 0, EGL_NONE };

	if (!eglQueryContext(display, context, EGL_CONFIG_ID, &config_id))
		return false;
	attrs[1] = config_id;
	return eglChooseConfig(display, attrs, out, 1, &count) && count == 1;
}

static EGLint atl_egl_attr(EGLDisplay display, EGLConfig config, EGLint attr)
{
	EGLint value = 0;

	eglGetConfigAttrib(display, config, attr, &value);
	return value;
}

/*
 * The chrome's buffer carries the SurfaceView's hole, so it needs an alpha
 * channel - and GLFW's own config has none on the phone: on hybris/Adreno the
 * config GLFW settles on is 8/8/8/0, the hole lands as opaque black and the
 * app's own frames underneath are never seen. Find the same config with 8 alpha
 * bits instead of guessing at attributes, so everything else the context cares
 * about (colour depth, depth/stencil, samples, renderable type) still matches.
 */
static bool atl_window_egl_config_alpha(EGLDisplay display, EGLConfig base, EGLConfig *out)
{
	static const EGLint match[] = {
		EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_DEPTH_SIZE,
		EGL_STENCIL_SIZE, EGL_SAMPLES, EGL_RENDERABLE_TYPE, EGL_COLOR_BUFFER_TYPE,
	};
	EGLConfig *configs;
	EGLint count = 0;
	bool found = false;

	if (atl_egl_attr(display, base, EGL_ALPHA_SIZE) >= 8)
		return false;                                /* nothing to fix */
	if (!eglGetConfigs(display, NULL, 0, &count) || count <= 0)
		return false;
	configs = calloc(count, sizeof(*configs));
	if (!configs)
		return false;
	if (eglGetConfigs(display, configs, count, &count)) {
		for (EGLint i = 0; i < count && !found; i++) {
			if (atl_egl_attr(display, configs[i], EGL_ALPHA_SIZE) < 8)
				continue;
			if (!(atl_egl_attr(display, configs[i], EGL_SURFACE_TYPE) & EGL_WINDOW_BIT))
				continue;
			found = true;
			for (size_t a = 0; a < sizeof(match) / sizeof(match[0]); a++)
				if (atl_egl_attr(display, configs[i], match[a]) !=
				    atl_egl_attr(display, base, match[a]))
					found = false;
			if (found)
				*out = configs[i];
		}
	}
	free(configs);
	return found;
}

/* the EGLSurface must go before the wl_egl_window under it does */
static void atl_window_drop_chrome_surface(ATLWindow *window, EGLDisplay display)
{
	if (window->chrome_surface) {
		if (eglGetCurrentSurface(EGL_DRAW) == window->chrome_surface)
			eglMakeCurrent(display, window->glfw_surface, window->glfw_surface, window->glfw_context);
		eglDestroySurface(display, window->chrome_surface);
		/* the next frame goes somewhere else - a new chrome, or the toplevel,
		 * whose buffer is the opaque black clear present_chrome left in it - so
		 * it cannot be a damage-rect frame over what was there before */
		window->full_redraw = true;
	}
	window->chrome_surface = NULL;
	window->chrome_egl_window = NULL;
}

/*
 * Point the GL context at the chrome sub-surface, creating it (and its
 * EGLSurface) the first time and after every recreation. Returns false when
 * this window draws into its toplevel as before - which is every app without a
 * SurfaceView, and every non-Wayland platform.
 */
static bool atl_window_bind_chrome(ATLWindow *window, int width, int height)
{
	EGLDisplay display;
	EGLContext context;
	struct wl_egl_window *egl_window;

	if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)
		return false;
	display = glfwGetEGLDisplay();
	context = window->glfw_context;
	if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT)
		return false;
	if (atl_surface_chrome_is_stale(window))
		atl_window_drop_chrome_surface(window, display);
	egl_window = atl_surface_chrome_ensure(window, width, height, atl_window_scale(window));
	if (!egl_window) {
		atl_window_drop_chrome_surface(window, display);
		return false;
	}

	if (window->chrome_egl_window != (void *)egl_window) {
		EGLConfig config, with_alpha;
		EGLSurface surface;
		EGLint alpha = 0;

		atl_window_drop_chrome_surface(window, display);
		if (!atl_window_egl_config(display, context, &config)) {
			fprintf(stderr, "ATLWindow: no EGLConfig for the chrome surface (0x%x)\n", eglGetError());
			atl_surface_chrome_fallback(window);
			return false;
		}
		if (atl_window_egl_config_alpha(display, config, &with_alpha)) {
			/* the driver may refuse a surface whose config is not the context's;
			 * eglMakeCurrent is where it says so, so ask here and keep the
			 * context's own config when it does */
			surface = eglCreateWindowSurface(display, with_alpha, (EGLNativeWindowType)egl_window, NULL);
			if (surface != EGL_NO_SURFACE &&
			    eglMakeCurrent(display, surface, surface, context)) {
				config = with_alpha;
			} else {
				fprintf(stderr, "ATLWindow: the chrome's 8-bit-alpha EGLConfig is not usable "
				                "with this context (0x%x); the hole stays opaque\n", eglGetError());
				if (surface != EGL_NO_SURFACE)
					eglDestroySurface(display, surface);
				surface = EGL_NO_SURFACE;
			}
		} else {
			surface = EGL_NO_SURFACE;
		}
		if (surface == EGL_NO_SURFACE)
			surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)egl_window, NULL);
		if (surface == EGL_NO_SURFACE) {
			fprintf(stderr, "ATLWindow: eglCreateWindowSurface for the chrome failed (0x%x)\n", eglGetError());
			atl_surface_chrome_fallback(window);
			return false;
		}
		eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &alpha);
		/* unconditional: this one line is what lets a log say for itself which
		 * of the two states produced it, which no run before 2026-08-14 could */
		fprintf(stderr, "ATLWindow: chrome EGLConfig has %d alpha bits "
		                "(transparent-framebuffer hint %s, opaque region %s)%s\n",
		        alpha, atl_surface_chrome_alpha_enabled() ? "on" : "off",
		        atl_surface_chrome_alpha_enabled() && atl_surface_opaque_region_enabled()
		                ? "declared by ATL" : "left to GLFW",
		        alpha < 8 ? "; a SurfaceView's hole will not be transparent" : "");
		window->chrome_surface = surface;
		window->chrome_egl_window = egl_window;
		/* a fresh buffer has no history, and the toplevel underneath keeps the
		 * last scene it was given until it is cleared below */
		window->full_redraw = true;
	}

	if (!eglMakeCurrent(display, window->chrome_surface, window->chrome_surface, context)) {
		fprintf(stderr, "ATLWindow: eglMakeCurrent on the chrome surface failed (0x%x)\n", eglGetError());
		atl_surface_chrome_fallback(window);
		return false;
	}
	return true;
}

/*
 * Present a frame that was drawn into the chrome sub-surface, and give the
 * toplevel a buffer when it needs one: at the size it was just configured to,
 * and whenever a sub-surface has parent-double-buffered state (a position, or a
 * newly created sub-surface's place-on-top) waiting for a parent commit.
 * Opaque black, because the whole toplevel is declared opaque - by GLFW when
 * the framebuffer has no alpha, by ATL when it has - and a surface must not
 * declare an opaque region it does not fill.
 */
static void atl_window_present_chrome(ATLWindow *window, int width, int height)
{
	EGLDisplay display = glfwGetEGLDisplay();
	bool commit_parent;

	if (!eglSwapBuffers(display, window->chrome_surface))
		fprintf(stderr, "ATLWindow: eglSwapBuffers on the chrome surface failed (0x%x)\n", eglGetError());
	/* GLFW believes its own surface is current on this thread and swaps it
	 * without checking, so hand it back before anything calls into GLFW */
	eglMakeCurrent(display, window->glfw_surface, window->glfw_surface, window->glfw_context);

	commit_parent = atl_surface_layers_take_parent_commit(window);
	if (window->toplevel_width != width || window->toplevel_height != height) {
		/* Only the toplevel's *first* buffer can be stale. It is dequeued
		 * during GL setup, before any configure has arrived, so this swap
		 * attaches it at the created size. Every later size change is dequeued
		 * after the configure - the driver reallocates at whatever size the
		 * wl_egl_window has then - and that attach already agrees, so a resync
		 * there costs a clear and a swap for a buffer identical to the one just
		 * sent (measured: firefox-atl testapps/evidence/device-resize-attaches.txt). */
		if (!window->toplevel_width && !window->toplevel_height)
			window->toplevel_resync = true;
		window->toplevel_width = width;
		window->toplevel_height = height;
		commit_parent = true;
	}
	if (commit_parent) {
		glViewport(0, 0, width, height);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		/* the toplevel commits rarely in this mode, so its opaque region has to
		 * go out with the commit that does happen */
		atl_surface_layers_before_swap(window, glfwGetWaylandWindow(window->glfw_window),
		                               width, height, atl_window_scale(window));
		glfwSwapBuffers(window->glfw_window);
	}
	if (debug_chrome())
		fprintf(stderr, "ATLWindow: chrome present %dx%d, parent commit %s\n",
		        width, height, commit_parent ? "yes" : "no");
}

/*
 * Give the toplevel one more buffer after its first size change.
 *
 * A swap attaches the buffer that was dequeued for it, and the toplevel's first
 * one is dequeued while the context is being set up, before any configure. So
 * the commit that reacts to the *first* configure carries the created size's
 * buffer, and in chrome mode nothing else ever commits the toplevel: that stale
 * buffer would stay up for the life of the window. Measured on the phone before
 * this existed, a 960x540 buffer sat under a 1080x2349 opaque region for the
 * whole run (doc/SurfaceViewCompositing.md).
 *
 * Costs one opaque-black clear and one swap, once per window, on a surface the
 * chrome covers completely. A later resize does not need it: its buffer is
 * dequeued after the configure and comes back at the new size by itself.
 */
static void atl_window_resync_toplevel(ATLWindow *window)
{
	int width, height;

	window->toplevel_resync = false;
	if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)
		return;
	glfwGetFramebufferSize(window->glfw_window, &width, &height);
	/* another configure landed in between: the next present handles it */
	if (width < 1 || height < 1 ||
	    width != window->toplevel_width || height != window->toplevel_height)
		return;
	glfwMakeContextCurrent(window->glfw_window);
	glViewport(0, 0, width, height);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	atl_surface_layers_before_swap(window, glfwGetWaylandWindow(window->glfw_window),
	                               width, height, atl_window_scale(window));
	glfwSwapBuffers(window->glfw_window);
	if (debug_chrome())
		fprintf(stderr, "ATLWindow: toplevel resync commit %dx%d\n", width, height);
}

/* Every publisher of the window size goes through Display.setWindowSize():
 * besides the two statics it updates Configuration, which was otherwise
 * snapshotted once by Context's static initialiser and then frozen. */
void atl_display_set_window_size(JNIEnv *env, int width, int height)
{
	static jclass display_class = NULL;
	static jmethodID set_window_size = NULL;
	if (!display_class) {
		jclass local_display_class = (*env)->FindClass(env, "android/view/Display");
		if (!local_display_class)
			return;
		display_class = (*env)->NewGlobalRef(env, local_display_class);
		(*env)->DeleteLocalRef(env, local_display_class);
		set_window_size = (*env)->GetStaticMethodID(env, display_class, "setWindowSize", "(II)V");
	}
	if (!set_window_size)
		return;
	(*env)->CallStaticVoidMethod(env, display_class, set_window_size, width, height);
	atl_report_pending_exception(env);
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
	bool full = window->full_redraw || !damage_enabled();
	jlong layout_ms = 0;
	if (width != window->layout_width || height != window->layout_height) {
		window->layout_width = width;
		window->layout_height = height;
		full = true;
		/* Display's statics back getMetrics()/getSize(), which is where an app
		 * gets the size it lays itself out for; leaving them at the size the
		 * window had before the compositor resized it is how a layout ends up
		 * wider than the surface it is drawn into. Display.setWindowSize also
		 * carries the new size into Configuration, which the app's resource
		 * lookups are keyed on, so do it before the layout pass below. */
		atl_display_set_window_size(env, width, height);
		if (window->dispatch_configuration_changed)
			(*env)->CallVoidMethod(env, window->view_root, window->dispatch_configuration_changed);
		atl_report_pending_exception(env);
		/* the window keeps its full size; the view root keeps the content clear
		 * of the soft keyboard itself, per the activity's softInputMode */
		(*env)->CallVoidMethod(env, window->view_root, window->set_ime_inset, ime_inset);
		jlong t = debug_render() ? atl_uptime_millis() : 0;
		(*env)->CallVoidMethod(env, window->view_root, window->perform_layout, width, height);
		atl_report_pending_exception(env);
		if (debug_render())
			layout_ms = atl_uptime_millis() - t;
	}

	/* Skia GPU work (surface creation, draws, flush) needs the window's GL
	 * context current on this thread; harmless for the raster path, which
	 * previously only made it current for the upload */
	glfwMakeContextCurrent(window->glfw_window);
	/* GLFW re-binds its own draw surface here every frame, so this is where the
	 * two handles the chrome path has to restore are valid */
	if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
		window->glfw_context = eglGetCurrentContext();
		window->glfw_surface = eglGetCurrentSurface(EGL_DRAW);
	}

	if (gpu_enabled() && !window->gpu_failed && !window->gpu_context) {
		window->gpu_context = atl_gpu_context_create((void *(*)(const char *))glfwGetProcAddress);
		if (!window->gpu_context) {
			window->gpu_failed = true;
			fprintf(stderr, "ATLWindow: GPU context creation failed, using CPU raster\n");
		} else {
			fprintf(stderr, "ATLWindow: GPU rendering (Ganesh) on %s, %s\n",
			        glGetString(GL_RENDERER), glGetString(GL_VERSION));
		}
	}

	if (!window->canvas || window->canvas_width != width || window->canvas_height != height) {
		if (window->canvas)
			atl_canvas_free(window->canvas);
		window->canvas = NULL;
		window->canvas_is_gpu = false;
		if (window->gpu_context && !window->gpu_failed) {
			window->canvas = atl_canvas_new_gpu(window->gpu_context, width, height);
			if (window->canvas)
				window->canvas_is_gpu = true;
			else {
				window->gpu_failed = true;
				fprintf(stderr, "ATLWindow: GPU surface creation failed, using CPU raster\n");
			}
		}
		if (!window->canvas)
			window->canvas = atl_canvas_new_raster(width, height);
		window->canvas_width = width;
		window->canvas_height = height;
		full = true;
	}

	/* everything ATL draws goes to the chrome sub-surface once this window has a
	 * SurfaceView, so that it composites above the app's own content; bound here
	 * rather than at present time so a chrome that was just (re)created gets its
	 * first buffer even on a frame with no damage */
	bool chrome = atl_window_bind_chrome(window, width, height);
	/* not just when it bound one: dropping the chrome moves this frame to the
	 * toplevel, and that needs the whole window as much as a new chrome does */
	full = full || window->full_redraw;

	/* foreign GL use on this context (WebView texture binds) invalidates the
	 * state Ganesh caches between frames */
	if (window->canvas_is_gpu)
		atl_gpu_context_reset(window->gpu_context);

	/* this frame's damage: everything, or what the view hierarchy accumulated
	 * in ViewRootImpl.mDirty since the last frame */
	int dl = 0, dt = 0, dr = width, db = height;
	if (!full) {
		jobject dirty = (*env)->GetObjectField(env, window->view_root, window->dirty_field);
		dl = (*env)->GetIntField(env, dirty, window->rect_left);
		dt = (*env)->GetIntField(env, dirty, window->rect_top);
		dr = (*env)->GetIntField(env, dirty, window->rect_right);
		db = (*env)->GetIntField(env, dirty, window->rect_bottom);
		(*env)->DeleteLocalRef(env, dirty);
		if (dl < 0) dl = 0;
		if (dt < 0) dt = 0;
		if (dr > width) dr = width;
		if (db > height) db = height;
		if (dl >= dr || dt >= db) {
			/* nothing visible changed */
			window->needs_redraw = false;
			return;
		}
	}
	if (debug_damage())
		fprintf(stderr, "ATLWindow: damage %d,%d-%d,%d%s of %dx%d\n", dl, dt, dr, db,
		        full ? " (full)" : "", width, height);

	jlong t_draw = debug_render() ? atl_uptime_millis() : 0;
	void *canvas = window->canvas;
	atl_canvas_begin_frame(canvas, dl, dt, dr, db);
	(*env)->CallVoidMethod(env, window->view_root, window->perform_draw, _INTPTR(canvas), width, height);
	atl_report_pending_exception(env);
	atl_canvas_end_frame(canvas);
	jlong draw_ms = debug_render() ? atl_uptime_millis() - t_draw : 0;

	if (window->canvas_is_gpu) {
		atl_canvas_gpu_present(window->gpu_context, canvas, width, height);
		if (chrome) {
			atl_window_present_chrome(window, width, height);
		} else {
			if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
				atl_surface_layers_before_swap(window, glfwGetWaylandWindow(window->glfw_window),
				                               width, height, atl_window_scale(window));
			}
			glfwSwapBuffers(window->glfw_window);
		}
		window->needs_redraw = false;
		window->full_redraw = false;
		if (debug_render() && (layout_ms > 50 || draw_ms > 50))
			fprintf(stderr, "ATLWindow: slow frame: layout %lldms, draw %lldms (%dx%d, damage %d,%d-%d,%d)\n",
			        (long long)layout_ms, (long long)draw_ms, width, height, dl, dt, dr, db);
		return;
	}

	int pixel_width, pixel_height, stride;
	const void *pixels = atl_canvas_get_pixels(canvas, &pixel_width, &pixel_height, &stride);

	/* no glfwMakeContextCurrent() here: it is already current from the top of
	 * this function, and GLFW re-binds its own surface unconditionally, which
	 * would send this blit to the toplevel instead of the chrome sub-surface */
	if (!window->gl_program)
		window->gl_program = atl_gl_make_blit_program(&window->gl_attr_pos, &window->gl_attr_uv);
	if (!window->gl_texture) {
		glGenTextures(1, &window->gl_texture);
		glBindTexture(GL_TEXTURE_2D, window->gl_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		window->tex_width = window->tex_height = 0;
	} else {
		glBindTexture(GL_TEXTURE_2D, window->gl_texture);
	}
	/* OpenGL ES 2.0 has no GL_UNPACK_ROW_LENGTH; skia raster surfaces are
	 * tightly packed in practice, repack in the (unexpected) padded case */
	void *packed = NULL;
	if (stride != pixel_width * 4) {
		packed = malloc((size_t)pixel_width * 4 * pixel_height);
		for (int y = 0; y < pixel_height; y++)
			memcpy((char *)packed + (size_t)y * pixel_width * 4,
			       (const char *)pixels + (size_t)y * stride,
			       (size_t)pixel_width * 4);
		pixels = packed;
		stride = pixel_width * 4;
	}
	if (window->tex_width != pixel_width || window->tex_height != pixel_height) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixel_width, pixel_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		window->tex_width = pixel_width;
		window->tex_height = pixel_height;
	} else {
		/* upload only the damaged rows (full width keeps the data contiguous
		 * without GL_UNPACK_ROW_LENGTH, which ES 2.0 lacks) */
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, dt, pixel_width, db - dt, GL_RGBA, GL_UNSIGNED_BYTE,
		                (const char *)pixels + (size_t)dt * stride);
	}
	free(packed);

	glViewport(0, 0, width, height);
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(window->gl_program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, window->gl_texture);
	static const float verts[] = {
		/* pos      uv */
		-1, -1,     0, 1,
		 1, -1,     1, 1,
		-1,  1,     0, 0,
		 1,  1,     1, 0,
	};
	glVertexAttribPointer(window->gl_attr_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts);
	glVertexAttribPointer(window->gl_attr_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts + 2);
	glEnableVertexAttribArray(window->gl_attr_pos);
	glEnableVertexAttribArray(window->gl_attr_uv);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	if (chrome) {
		atl_window_present_chrome(window, width, height);
	} else {
		if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
			atl_surface_layers_before_swap(window, glfwGetWaylandWindow(window->glfw_window),
			                               width, height, atl_window_scale(window));
		}
		glfwSwapBuffers(window->glfw_window);
	}

	window->needs_redraw = false;
	window->full_redraw = false;

	/* Only a handful of frames should ever be slow (the first layout, genuine
	 * resizes); a steady stream of these means something is scheduling
	 * needless relayouts/redraws. */
	if (debug_render() && (layout_ms > 50 || draw_ms > 50))
		fprintf(stderr, "ATLWindow: slow frame: layout %lldms, draw %lldms (%dx%d, damage %d,%d-%d,%d)\n",
		        (long long)layout_ms, (long long)draw_ms, width, height, dl, dt, dr, db);
}

/* --- event pump on the GLib main loop --- */

static gboolean atl_windows_tick(gpointer user_data)
{
	int rw, rh;
	double after;
	int pcx, pcy, ps0, ps1, psteps;
	const struct atl_debug_tap *taps;
	int ntaps;

	glfwPollEvents();
	if (windows && debug_resize(&rw, &rh, &after)) {
		static jlong t0;
		static bool fired;
		jlong now = atl_uptime_millis();
		if (!t0)
			t0 = now;
		if (!fired && (double)(now - t0) >= after * 1000.0) {
			fired = true;
			fprintf(stderr, "ATLWindow: ATL_DEBUG_RESIZE: asking the compositor for %dx%d\n", rw, rh);
			glfwSetWindowSizeLimits(windows->glfw_window, rw, rh, rw, rh);
			/* xdg_surface state is double-buffered and in chrome mode the
			 * toplevel can go a whole run without a commit, so the limits
			 * would otherwise sit pending forever and no configure would come */
			if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
				wl_surface_commit(glfwGetWaylandWindow(windows->glfw_window));
		}
	}
	if (windows && debug_pinch(&after, &pcx, &pcy, &ps0, &ps1, &psteps)) {
		static jlong t0;
		static int step = -1, ticks;
		jlong now = atl_uptime_millis();
		if (!t0)
			t0 = now;
		if (step < 3 + psteps && (double)(now - t0) >= after * 1000.0 && ticks++ % 8 == 0) {
			ATLWindow *target = NULL;
			for (ATLWindow *w = windows; w; w = w->next)
				if (w->view_root)
					target = w;
			if (!target) {
				ticks = 0; /* nothing to aim at yet; wait for a view root */
			} else {
				step++;
				debug_pinch_step(target, step, pcx, pcy, ps0, ps1, psteps);
			}
		}
	}
	if (windows && (ntaps = debug_tap(&taps))) {
		static jlong t0;
		static int index, step = -1, ticks;
		jlong now = atl_uptime_millis();
		if (!t0)
			t0 = now;
		if (index < ntaps && (double)(now - t0) >= taps[index].after * 1000.0 && ticks++ % 8 == 0) {
			ATLWindow *target = NULL;
			for (ATLWindow *w = windows; w; w = w->next)
				if (w->view_root)
					target = w;
			if (!target) {
				ticks = 0; /* nothing to aim at yet; wait for a view root */
			} else {
				debug_tap_step(target, &taps[index], ++step);
				if (step > taps[index].steps) { /* lifted; on to the next entry */
					index++;
					step = -1;
					ticks = 0;
				}
			}
		}
	}
	for (ATLWindow *window = windows; window; window = window->next) {
		if (window->needs_redraw)
			atl_window_render(window);
		/* not inside the render path: an idle window still owes the toplevel
		 * this commit, and a frame with no damage never reaches a present */
		if (window->toplevel_resync)
			atl_window_resync_toplevel(window);
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

	/* ATL_FORCE_FULLSCREEN (doc/Envs.md): open at the monitor's size instead of
	 * at the launcher's default. A phone shell gives every app the whole screen
	 * anyway, and starting at the size we will keep means the layout, the canvas
	 * and the surface never disagree -- opening small and being resized leaves a
	 * window whose first frame was laid out for one size and drawn into a buffer
	 * of another.
	 *
	 * Size the window rather than making it a fullscreen toplevel: a client-driven
	 * state change leaves qtmir's previousState stale, and restoring the app from
	 * minimised then aborts Lomiri. The shell gives us the whole screen unasked. */
	bool size_to_monitor = false;
	if (getenv("ATL_FORCE_FULLSCREEN")) {
		GLFWmonitor *monitor = glfwGetPrimaryMonitor();
		int area_x, area_y, area_width, area_height;
		/* the work area, not the video mode: a phone panel's mode is its
		 * unrotated one (2856x1280 on a portrait screen), while the work area
		 * is the output as the compositor presents it, rotation included */
		if (monitor)
			glfwGetMonitorWorkarea(monitor, &area_x, &area_y, &area_width, &area_height);
		if (monitor && area_width > 0 && area_height > 0) {
			width = area_width;
			height = area_height;
			size_to_monitor = true;
			fprintf(stderr, "ATLWindow: sized to the primary monitor's work area, %dx%d\n", width, height);
		} else {
			fprintf(stderr, "atl_window_new: ATL_FORCE_FULLSCREEN set but no monitor work area, using %dx%d\n", width, height);
		}
	}

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
	/*
	 * Ask GLFW for a framebuffer with an alpha channel, which is the only way to
	 * get one where EGL_EXT_present_opaque is missing: GLFW then "ignore[s] any
	 * config with an alpha channel to ensure the buffer is opaque" (glfw
	 * src/egl_context.c). That is the phone - hybris EGL has no such extension -
	 * so without this ATL's context comes up 8/8/8/0, the hole punched into the
	 * chrome sub-surface for a SurfaceView lands as opaque black, and every
	 * pixel the app draws is lost. GLFW stops declaring the toplevel's opaque
	 * region once the hint is set, so ATL declares it itself
	 * (atl_surface_layers_before_swap). ATL_SURFACE_CHROME_ALPHA=0 is the way
	 * back. Wayland only: on X11 the hint picks an ARGB visual, which needs a
	 * compositing WM, and there are no sub-surfaces there anyway.
	 */
	if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND && atl_surface_chrome_alpha_enabled())
		glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
	if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
		/* once per process: a log that does not say which of the two states it
		 * was taken in cannot be used to attribute anything to either */
		static bool said;
		if (!said) {
			said = true;
			fprintf(stderr, "ATLWindow: transparent-framebuffer hint %s, opaque region %s\n",
			        atl_surface_chrome_alpha_enabled() ? "on" : "off",
			        atl_surface_chrome_alpha_enabled() && atl_surface_opaque_region_enabled()
			                ? "declared by ATL" : "left to GLFW");
		}
	}
#ifdef GLFW_WAYLAND_APP_ID
	/* Lomiri/Mir associates windows with their launcher entry through the
	 * wayland app_id */
	if (getenv("APP_ID"))
		glfwWindowHintString(GLFW_WAYLAND_APP_ID, getenv("APP_ID"));
#endif
	window->glfw_window = glfwCreateWindow(width, height, "android-translation-layer", NULL, NULL);
	if (!window->glfw_window) {
		/* mobile GPUs (e.g. hybris EGL on Ubuntu Touch) only do OpenGL ES */
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
		window->glfw_window = glfwCreateWindow(width, height, "android-translation-layer", NULL, NULL);
	}
	if (!window->glfw_window) {
		const char *desc = NULL;
		glfwGetError(&desc);
		fprintf(stderr, "atl_window_new: glfwCreateWindow failed: %s\n", desc ? desc : "(no detail)");
		exit(1);
	}
	glfwSetWindowUserPointer(window->glfw_window, window);
	glfwSetCursorPosCallback(window->glfw_window, on_cursor_pos);
	glfwSetMouseButtonCallback(window->glfw_window, on_mouse_button);
	glfwSetScrollCallback(window->glfw_window, on_scroll);
	glfwSetKeyCallback(window->glfw_window, on_key);
	glfwSetCharCallback(window->glfw_window, on_char);
	glfwSetFramebufferSizeCallback(window->glfw_window, on_framebuffer_size);
	glfwSetWindowCloseCallback(window->glfw_window, on_window_close);
	glfwSetWindowFocusCallback(window->glfw_window, on_window_focus);
	if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
		atl_wayland_init();
	/* a SurfaceView's wl_egl_window only works on the display GLFW made its
	 * Wayland connection on, so that is the one an app's eglGetDisplay gets */
	bionic_egl_set_primary_display(glfwGetEGLDisplay());
	glfwMakeContextCurrent(window->glfw_window);
	glfwSwapInterval(0); // frame pacing comes from the render tick, don't block on vsync

	/* On Wayland a surface is not mapped until its first buffer is committed,
	 * so hold off committing anything here: the compositor keeps showing its
	 * own splash (e.g. Lomiri's QML splash) until the app draws its first real
	 * frame, instead of us flashing a blank white window at the pre-configure
	 * size for the few seconds before a ViewRootImpl attaches.
	 *
	 * On X11 the window maps as soon as it is created regardless of drawing, so
	 * commit one clear frame there to avoid showing uninitialised garbage until
	 * the first real frame arrives. */
	if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND) {
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glfwSwapBuffers(window->glfw_window);
	}
	glfwPollEvents();

	/* Wait for the compositor's first configure before handing this window to
	 * the caller. It is the configure, not our request, that sets the size, and
	 * the launcher publishes the size to Display -- and the app caches it, in
	 * Configuration and in its own fields -- before a single frame is drawn.
	 * Only when we sized ourselves to the monitor: elsewhere the size we asked
	 * for is the size we get, and this would be 300ms of start-up spent waiting
	 * for an event that never comes. */
	if (size_to_monitor) {
		for (int i = 0; i < 30; i++) {
			int configured_width, configured_height;
			glfwGetFramebufferSize(window->glfw_window, &configured_width, &configured_height);
			if (configured_width != width || configured_height != height)
				break;
			glfwWaitEventsTimeout(0.01);
		}
	}

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
	for (ATLWindow *window = windows; window; window = window->next) {
		window->needs_redraw = true;
		window->full_redraw = true; // no java-side damage rect for this
	}
}

void atl_window_set_view_root(ATLWindow *window, JNIEnv *env, jobject view_root)
{
	if (window->view_root)
		(*env)->DeleteGlobalRef(env, window->view_root);
	window->view_root = (*env)->NewGlobalRef(env, view_root);
	jclass view_root_class = (*env)->GetObjectClass(env, view_root);
	window->perform_layout = (*env)->GetMethodID(env, view_root_class, "performLayout", "(II)V");
	window->set_ime_inset = (*env)->GetMethodID(env, view_root_class, "setImeInset", "(I)V");
	window->dispatch_configuration_changed = (*env)->GetMethodID(env, view_root_class, "dispatchConfigurationChanged", "()V");
	window->perform_draw = (*env)->GetMethodID(env, view_root_class, "performDraw", "(JII)V");
	window->dispatch_touch_event = (*env)->GetMethodID(env, view_root_class, "dispatchTouchEvent", "(Landroid/view/MotionEvent;)Z");
	window->dispatch_generic_motion_event = (*env)->GetMethodID(env, view_root_class, "dispatchGenericMotionEvent", "(Landroid/view/MotionEvent;)Z");
	window->dispatch_key_event = (*env)->GetMethodID(env, view_root_class, "dispatchKeyEvent", "(Landroid/view/KeyEvent;)Z");
	window->dispatch_character = (*env)->GetMethodID(env, view_root_class, "dispatchCharacter", "(I)Z");
	window->dispatch_commit_text = (*env)->GetMethodID(env, view_root_class, "dispatchCommitText", "(Ljava/lang/String;III)Z");
	window->dispatch_composing_text = (*env)->GetMethodID(env, view_root_class, "dispatchComposingText", "(Ljava/lang/String;III)Z");
	window->dispatch_finish_composing = (*env)->GetMethodID(env, view_root_class, "dispatchFinishComposing", "()V");
	window->dispatch_ime_set_selection = (*env)->GetMethodID(env, view_root_class, "dispatchImeSetSelection", "(II)V");
	window->dispatch_ime_get_selection = (*env)->GetMethodID(env, view_root_class, "dispatchImeGetSelection", "()Ljava/lang/String;");
	window->dispatch_im_initiated_hide = (*env)->GetMethodID(env, view_root_class, "dispatchImInitiatedHide", "()V");
	window->dispatch_window_focus_changed = (*env)->GetMethodID(env, view_root_class, "dispatchWindowFocusChanged", "(Z)V");
	window->dirty_field = (*env)->GetFieldID(env, view_root_class, "mDirty", "Landroid/graphics/Rect;");
	jclass rect_class = (*env)->FindClass(env, "android/graphics/Rect");
	window->rect_left = (*env)->GetFieldID(env, rect_class, "left", "I");
	window->rect_top = (*env)->GetFieldID(env, rect_class, "top", "I");
	window->rect_right = (*env)->GetFieldID(env, rect_class, "right", "I");
	window->rect_bottom = (*env)->GetFieldID(env, rect_class, "bottom", "I");
	(*env)->SetLongField(env, view_root, (*env)->GetFieldID(env, view_root_class, "scene", "J"), _INTPTR(window));
	window->layout_width = window->layout_height = 0; // force a layout pass
	window->needs_redraw = true;
	window->full_redraw = true;
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
	window->full_redraw = true; // a fresh surface needs a full commit
}

void atl_window_hide(ATLWindow *window)
{
	glfwHideWindow(window->glfw_window);
	/* An unmapped surface gets no focus-lost callback from the compositor, so
	 * release the input method here instead — but only for the window IME
	 * events are actually routed to (the atl_windows_ime_* helpers all address
	 * the list head), or hiding a second window would release the first's
	 * context. */
	if (window == windows)
		atl_window_notify_focus(window, false);
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

/* --- what the subsurface layers need from their parent toplevel --- */

struct wl_surface *atl_window_wl_surface(ATLWindow *window)
{
	if (!window || glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)
		return NULL;
	return glfwGetWaylandWindow(window->glfw_window);
}

/* framebuffer pixels per logical (wayland surface-local) pixel */
double atl_window_scale(ATLWindow *window)
{
	int fb_w = 0, fb_h = 0, win_w = 0, win_h = 0;

	if (!window)
		return 1;
	glfwGetFramebufferSize(window->glfw_window, &fb_w, &fb_h);
	glfwGetWindowSize(window->glfw_window, &win_w, &win_h);
	return win_w > 0 ? (double)fb_w / win_w : 1;
}

void atl_window_invalidate(ATLWindow *window)
{
	if (window) {
		window->needs_redraw = true;
		window->full_redraw = true;
	}
}

int atl_window_get_width(ATLWindow *window)
{
	int width, height;
	glfwGetFramebufferSize(window->glfw_window, &width, &height);
	return width;
}

int atl_window_get_height(ATLWindow *window)
{
	int width, height;
	glfwGetFramebufferSize(window->glfw_window, &width, &height);
	return height;
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
