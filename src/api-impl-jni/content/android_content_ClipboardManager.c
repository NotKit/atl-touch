#include "../defines.h"
#include "../ATLWindow.h"

#include "../generated_headers/android_content_ClipboardManager.h"

extern ATLWindow *atl_window;

JNIEXPORT void JNICALL Java_android_content_ClipboardManager_native_1set_1clipboard(JNIEnv *env, jclass class, jstring text_jstring)
{
	if (!atl_window)
		return;
	const char *text = (*env)->GetStringUTFChars(env, text_jstring, NULL);
	atl_window_set_clipboard(atl_window, text);
	(*env)->ReleaseStringUTFChars(env, text_jstring, text);
}

JNIEXPORT jstring JNICALL Java_android_content_ClipboardManager_native_1get_1clipboard(JNIEnv *env, jclass class)
{
	if (!atl_window)
		return NULL;
	const char *text = atl_window_get_clipboard(atl_window);
	return text ? (*env)->NewStringUTF(env, text) : NULL;
}
