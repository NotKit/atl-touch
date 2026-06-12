#include <jni.h>

#include "../defines.h"
#include "../util.h"

#include "../ATLWindow.h"

#include "../generated_headers/android_app_Dialog.h"

JNIEXPORT jlong JNICALL Java_android_app_Dialog_nativeInit(JNIEnv *env, jobject this)
{
	ATLWindow *dialog = atl_window_new(400, 400, false, true);
	atl_window_set_dismiss_target(dialog, env, this);
	return _INTPTR(dialog);
}

JNIEXPORT void JNICALL Java_android_app_Dialog_nativeSetTitle(JNIEnv *env, jobject this, jlong ptr, jstring title)
{
	const char *nativeTitle = (*env)->GetStringUTFChars(env, title, NULL);
	atl_window_set_title((ATLWindow *)_PTR(ptr), nativeTitle);
	(*env)->ReleaseStringUTFChars(env, title, nativeTitle);
}

JNIEXPORT void JNICALL Java_android_app_Dialog_nativeSetContentView(JNIEnv *env, jobject this, jlong ptr, jlong widget_ptr)
{
	/* unused: content is attached through Window.attachViewRoot() */
}

JNIEXPORT void JNICALL Java_android_app_Dialog_nativeShow(JNIEnv *env, jobject this, jlong ptr)
{
	atl_window_show((ATLWindow *)_PTR(ptr));
}

JNIEXPORT void JNICALL Java_android_app_Dialog_nativeClose(JNIEnv *env, jobject this, jlong ptr)
{
	atl_window_hide((ATLWindow *)_PTR(ptr));
}

JNIEXPORT jboolean JNICALL Java_android_app_Dialog_nativeIsShowing(JNIEnv *env, jobject this, jlong ptr)
{
	return atl_window_is_visible((ATLWindow *)_PTR(ptr));
}
