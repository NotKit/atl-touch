#include "../defines.h"
#include "../ATLWindow.h"
#include "../util.h"
#include "content_hub_clipboard.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "../generated_headers/android_content_ClipboardManager.h"

extern ATLWindow *atl_window;

JNIEXPORT void JNICALL Java_android_content_ClipboardManager_native_1set_1clipboard(JNIEnv *env, jclass class, jstring text_jstring)
{
	if (!text_jstring)
		return;
	char *text = jstring_to_utf8(env, text_jstring);
	if (!text)
		return;

	if (atl_clipboard_debug())
		fprintf(stderr, "[atl-clip] app copied %zu bytes\n", strlen(text));
	if (atl_window)
		atl_window_set_clipboard(atl_window, text);
	atl_content_hub_clipboard_set(text);

	g_free(text);
}

JNIEXPORT jstring JNICALL Java_android_content_ClipboardManager_native_1get_1clipboard(JNIEnv *env, jclass class)
{
	char *text = atl_content_hub_clipboard_get();
	if (text) {
		jstring result = utf8_to_jstring(env, text);
		g_free(text);
		return result;
	}

	const char *wayland = atl_window ? atl_window_get_clipboard(atl_window) : NULL;
	return utf8_to_jstring(env, wayland);
}
