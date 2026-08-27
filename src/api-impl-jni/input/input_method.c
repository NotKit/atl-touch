#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <jni.h>

#include "input_method.h"
#include "../ATLWindow.h"
#include "../util.h"

/* Fake keyboard for development on a desktop, where there is no soft keyboard
 * at all: ATL_IM_DEBUG_INSET=<px> reserves that much of the window while an
 * editor is focused, so the adjustResize path can be exercised.
 *
 * With ATL_DEBUG_IME=1 it also traces the editor state the app pushes, which is
 * the half a real backend forwards to its server. That trace is the only way to
 * see, without a keyboard attached, whether an edit reached the input method —
 * the failure it exists to catch is state that stops being sent. */
static int debug_inset;

static bool debug_im_init(void)
{
	const char *px = getenv("ATL_IM_DEBUG_INSET");
	debug_inset = px ? atoi(px) : 0;
	return debug_inset > 0;
}

static void debug_im_update(const struct atl_im_state *state, bool focus_changed)
{
	if (atl_debug_ime())
		fprintf(stderr, "atl_ime: update focus=%d focusChanged=%d cursor=%d anchor=%d text='%s'\n",
		        state->focused, focus_changed, state->cursor_position, state->anchor_position,
		        state->focused ? state->surrounding_text : "");
	if (!state->focused)
		atl_windows_set_ime_inset(0);
}

static void debug_im_reset(void)
{
	if (atl_debug_ime())
		fprintf(stderr, "atl_ime: reset\n");
}

static void debug_im_show(void)
{
	if (atl_debug_ime())
		fprintf(stderr, "atl_ime: show\n");
	atl_windows_set_ime_inset(debug_inset);
}

static void debug_im_hide(void)
{
	if (atl_debug_ime())
		fprintf(stderr, "atl_ime: hide\n");
	atl_windows_set_ime_inset(0);
}

static const struct atl_im_backend atl_im_backend_debug = {
	.name = "debug",
	.init = debug_im_init,
	.update = debug_im_update,
	.reset = debug_im_reset,
	.show = debug_im_show,
	.hide = debug_im_hide,
};

/* Probe order: the debug backend (opt-in through its env var) first, then
 * shell-specific D-Bus transports, generic wayland protocols
 * (zwp_text_input_v3, once implemented) after. NULL entries are backends that
 * weren't compiled in (weak symbols). */
static const struct atl_im_backend *const candidates[] = {
	&atl_im_backend_debug,
	&atl_im_backend_maliit,
};

static const struct atl_im_backend *active;

static void im_select(void)
{
	/* mirrors QT_IM_MODULE: force a backend by name, or "none" to disable */
	const char *force = getenv("ATL_IM_MODULE");
	if (force && !strcmp(force, "none"))
		return;

	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
		const struct atl_im_backend *backend = candidates[i];
		if (!backend) /* weak symbol, backend not compiled in */
			continue;
		if (force && strcmp(force, backend->name))
			continue;
		if (backend->init()) {
			fprintf(stderr, "input_method: using '%s' backend\n", backend->name);
			active = backend;
			return;
		}
	}
	if (force)
		fprintf(stderr, "input_method: requested backend '%s' is not available\n", force);
}

JNIEXPORT jboolean JNICALL Java_android_view_inputmethod_InputMethodManager_nativeInit(JNIEnv *env, jclass class)
{
	im_select();
	return active != NULL;
}

JNIEXPORT void JNICALL Java_android_view_inputmethod_InputMethodManager_nativeUpdate(
	JNIEnv *env, jclass class, jstring surrounding_text, jint cursor_position, jint anchor_position,
	jint input_type, jint ime_options, jboolean focused, jboolean focus_changed)
{
	if (!active)
		return;

	/* real UTF-8, not JNI's modified kind: a backend hands this straight to
	 * GLib/D-Bus, which reject the CESU-8 an emoji would come out as */
	char *text = jstring_to_utf8(env, surrounding_text);
	struct atl_im_state state = {
		.focused = focused,
		.surrounding_text = text ? text : "",
		.cursor_position = cursor_position,
		.anchor_position = anchor_position,
		.input_type = input_type,
		.ime_options = ime_options,
	};
	active->update(&state, focus_changed);
	g_free(text);
}

JNIEXPORT void JNICALL Java_android_view_inputmethod_InputMethodManager_nativeReset(JNIEnv *env, jclass class)
{
	if (active)
		active->reset();
}

JNIEXPORT void JNICALL Java_android_view_inputmethod_InputMethodManager_nativeShowSoftInput(JNIEnv *env, jclass class)
{
	if (active)
		active->show();
}

JNIEXPORT void JNICALL Java_android_view_inputmethod_InputMethodManager_nativeHideSoftInput(JNIEnv *env, jclass class)
{
	if (active)
		active->hide();
}
