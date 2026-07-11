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

struct ATLWindow {
	GLFWwindow *glfw_window;
	jobject view_root; // global ref, NULL until attached
	jmethodID perform_layout;
	jmethodID perform_draw;
	jmethodID dispatch_touch_event;
	jmethodID dispatch_key_event;
	jmethodID dispatch_character;
	jmethodID dispatch_commit_text;
	jmethodID dispatch_composing_text;
	jmethodID dispatch_finish_composing;
	jobject window_jobj;    // weak ref to the java android.view.Window
	bool needs_redraw;
	bool pointer_down;
	double pointer_x, pointer_y;
	int layout_width, layout_height;
	unsigned int gl_texture;
	unsigned int gl_program;
	int gl_attr_pos, gl_attr_uv;
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

/* --- wayland touch input ----------------------------------------------
 * GLFW has no touch API; its wayland backend only listens to wl_pointer,
 * and Lomiri/Mir does not emulate a pointer for finger input. Bind our own
 * wl_touch from the seat and feed the existing dispatch path (first touch
 * point only for now). Events arrive through GLFW's wl_display, dispatched
 * from glfwPollEvents(). */

static ATLWindow *atl_touch_window;
static int32_t atl_touch_id = -1;

static ATLWindow *atl_window_from_wl_surface(struct wl_surface *surface)
{
	for (ATLWindow *w = windows; w; w = w->next)
		if (glfwGetWaylandWindow(w->glfw_window) == surface)
			return w;
	return NULL;
}

static void atl_wl_touch_down(void *data, struct wl_touch *touch, uint32_t serial,
                              uint32_t time, struct wl_surface *surface, int32_t id,
                              wl_fixed_t x, wl_fixed_t y)
{
	if (atl_touch_id != -1)
		return;
	ATLWindow *window = atl_window_from_wl_surface(surface);
	if (!window)
		return;
	atl_touch_id = id;
	atl_touch_window = window;
	window->pointer_x = wl_fixed_to_double(x);
	window->pointer_y = wl_fixed_to_double(y);
	window->pointer_down = true;
	dispatch_pointer_event(window, ACTION_DOWN);
}

static void atl_wl_touch_up(void *data, struct wl_touch *touch, uint32_t serial,
                            uint32_t time, int32_t id)
{
	if (id != atl_touch_id || !atl_touch_window)
		return;
	dispatch_pointer_event(atl_touch_window, ACTION_UP);
	atl_touch_window->pointer_down = false;
	atl_touch_window = NULL;
	atl_touch_id = -1;
}

static void atl_wl_touch_motion(void *data, struct wl_touch *touch, uint32_t time,
                                int32_t id, wl_fixed_t x, wl_fixed_t y)
{
	if (id != atl_touch_id || !atl_touch_window)
		return;
	atl_touch_window->pointer_x = wl_fixed_to_double(x);
	atl_touch_window->pointer_y = wl_fixed_to_double(y);
	dispatch_pointer_event(atl_touch_window, ACTION_MOVE);
}

static void atl_wl_touch_frame(void *data, struct wl_touch *touch)
{
}

static void atl_wl_touch_cancel(void *data, struct wl_touch *touch)
{
	if (!atl_touch_window)
		return;
	dispatch_pointer_event(atl_touch_window, ACTION_CANCEL);
	atl_touch_window->pointer_down = false;
	atl_touch_window = NULL;
	atl_touch_id = -1;
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

static void atl_wl_registry_global(void *data, struct wl_registry *registry, uint32_t name,
                                   const char *interface, uint32_t version)
{
	static struct wl_seat *seat;
	if (!strcmp(interface, wl_seat_interface.name) && !seat) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface,
		                        version < 5 ? version : 5);
		wl_seat_add_listener(seat, &atl_wl_seat_listener, NULL);
	}
}

static void atl_wl_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener atl_wl_registry_listener = {
	.global = atl_wl_registry_global,
	.global_remove = atl_wl_registry_global_remove,
};

static void atl_touch_init(void)
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

/* --- IME event injection (used by input method backends) --- */

void atl_windows_ime_commit_text(const char *utf8)
{
	ATLWindow *window = windows;
	if (!window || !window->view_root || !utf8)
		return;
	JNIEnv *env = get_jni_env();
	jstring str = (*env)->NewStringUTF(env, utf8);
	(*env)->CallBooleanMethod(env, window->view_root, window->dispatch_commit_text, str);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	(*env)->DeleteLocalRef(env, str);
}

void atl_windows_ime_set_composing(const char *utf8)
{
	ATLWindow *window = windows;
	if (!window || !window->view_root || !utf8)
		return;
	JNIEnv *env = get_jni_env();
	jstring str = (*env)->NewStringUTF(env, utf8);
	(*env)->CallBooleanMethod(env, window->view_root, window->dispatch_composing_text, str);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	(*env)->DeleteLocalRef(env, str);
}

void atl_windows_ime_finish_composing(void)
{
	ATLWindow *window = windows;
	if (!window || !window->view_root)
		return;
	JNIEnv *env = get_jni_env();
	(*env)->CallVoidMethod(env, window->view_root, window->dispatch_finish_composing);
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
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	(*env)->DeleteLocalRef(env, key_event);
}

static int ime_inset = 0;

void atl_windows_set_ime_inset(int inset)
{
	if (inset < 0)
		inset = 0;
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
		/* keep the layout clear of the soft keyboard (adjustResize) */
		int layout_height = height > ime_inset ? height - ime_inset : height;
		jlong t = debug_render() ? atl_uptime_millis() : 0;
		(*env)->CallVoidMethod(env, window->view_root, window->perform_layout, width, layout_height);
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
	if (!window->gl_program)
		window->gl_program = atl_gl_make_blit_program(&window->gl_attr_pos, &window->gl_attr_uv);
	if (!window->gl_texture) {
		glGenTextures(1, &window->gl_texture);
		glBindTexture(GL_TEXTURE_2D, window->gl_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixel_width, pixel_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
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
	glfwSetKeyCallback(window->glfw_window, on_key);
	glfwSetCharCallback(window->glfw_window, on_char);
	glfwSetFramebufferSizeCallback(window->glfw_window, on_framebuffer_size);
	glfwSetWindowCloseCallback(window->glfw_window, on_window_close);
	if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
		atl_touch_init();
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
	window->dispatch_commit_text = (*env)->GetMethodID(env, view_root_class, "dispatchCommitText", "(Ljava/lang/String;)Z");
	window->dispatch_composing_text = (*env)->GetMethodID(env, view_root_class, "dispatchComposingText", "(Ljava/lang/String;)Z");
	window->dispatch_finish_composing = (*env)->GetMethodID(env, view_root_class, "dispatchFinishComposing", "()V");
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
