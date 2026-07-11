#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jni.h>

#include "input_method.h"

/* Probe order: shell-specific D-Bus transports first, generic wayland
 * protocols (zwp_text_input_v3, once implemented) after. NULL entries are
 * backends that weren't compiled in (weak symbols). */
static const struct atl_im_backend *const candidates[] = {
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

JNIEXPORT void JNICALL Java_android_view_inputmethod_InputMethodManager_nativeHideSoftInput(JNIEnv *env, jobject this, jlong im_context)
{
	if (active)
		active->hide();
}
