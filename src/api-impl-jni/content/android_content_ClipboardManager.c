#include "../defines.h"
#include "../ATLWindow.h"
#include "content_hub_clipboard.h"

#include <glib.h>
#include <string.h>

#include "../generated_headers/android_content_ClipboardManager.h"

extern ATLWindow *atl_window;

/* The last text we copied, and whether content-hub took it. The Wayland
 * selection is only shared with other Wayland clients, so on Ubuntu Touch the
 * hub is the system-wide clipboard and wins — unless it turned our own copy
 * down and that copy is still the selection. */
static char *own_copy;
static bool own_copy_published;

JNIEXPORT void JNICALL Java_android_content_ClipboardManager_native_1set_1clipboard(JNIEnv *env, jclass class, jstring text_jstring)
{
	if (!text_jstring)
		return;
	const char *text = (*env)->GetStringUTFChars(env, text_jstring, NULL);

	if (atl_window)
		atl_window_set_clipboard(atl_window, text);
	own_copy_published = atl_content_hub_clipboard_set(text);
	g_free(own_copy);
	own_copy = g_strdup(text);

	(*env)->ReleaseStringUTFChars(env, text_jstring, text);
}

JNIEXPORT jstring JNICALL Java_android_content_ClipboardManager_native_1get_1clipboard(JNIEnv *env, jclass class)
{
	/* owned by GLFW, valid until the next clipboard call */
	const char *wayland = atl_window ? atl_window_get_clipboard(atl_window) : NULL;
	bool ours = own_copy && wayland && !strcmp(wayland, own_copy);

	if (!ours || own_copy_published) {
		char *text = atl_content_hub_clipboard_get();
		if (text) {
			jstring text_jstring = (*env)->NewStringUTF(env, text);
			g_free(text);
			return text_jstring;
		}
	}

	return wayland ? (*env)->NewStringUTF(env, wayland) : NULL;
}
