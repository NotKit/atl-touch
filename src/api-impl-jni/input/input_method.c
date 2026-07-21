#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jni.h>

#include "input_method.h"
#include "../ATLWindow.h"

/* Fake keyboard for development on a desktop, where there is no soft keyboard
 * at all: ATL_IM_DEBUG_INSET=<px> reserves that much of the window while an
 * editor is focused, so the adjustResize path can be exercised. */
static int debug_inset;

static bool debug_im_init(void)
{
	const char *px = getenv("ATL_IM_DEBUG_INSET");
	debug_inset = px ? atoi(px) : 0;
	return debug_inset > 0;
}

static void debug_im_show(int input_type)
{
	atl_windows_set_ime_inset(debug_inset);
}

static void debug_im_hide(void)
{
	atl_windows_set_ime_inset(0);
}

static const struct atl_im_backend atl_im_backend_debug = {
	.name = "debug",
	.init = debug_im_init,
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

JNIEXPORT jlong JNICALL Java_android_view_inputmethod_InputMethodManager_nativeInit(JNIEnv *env, jclass class)
{
	im_select();
	return active != NULL;
}

JNIEXPORT jboolean JNICALL Java_android_view_inputmethod_InputMethodManager_nativeShowSoftInput(JNIEnv *env, jobject this, jlong im_context, jlong widget, jobject input_connection, jint input_type)
{
	if (!active)
		return JNI_FALSE;
	active->show(input_type);
	return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_android_view_inputmethod_InputMethodManager_nativeHideSoftInput(JNIEnv *env, jclass class, jlong im_context)
{
	if (active)
		active->hide();
}
