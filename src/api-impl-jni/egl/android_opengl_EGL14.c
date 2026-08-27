/*
 * android.opengl.EGL14. The Java side boxes handles into EGLDisplay/EGLContext/
 * EGLSurface, so everything here speaks raw jlong handles.
 *
 * Calls go through the bionic_egl* shims where those exist (libandroid/egl.c),
 * so EGL14 and the NDK see the same display, the same pbuffer-config fallback
 * and the same ANativeWindow bookkeeping.
 */

#include <EGL/egl.h>
#include <stdbool.h>

#include "../defines.h"
#include "../util.h"

#include "../../libandroid/native_window.h"

#include "surface_texture_target.h"

#include "../generated_headers/android_opengl_EGL14.h"
#include "../generated_headers/android_opengl_EGLExt.h"

static EGLint *get_attrib_list(JNIEnv *env, jintArray array)
{
	if (!array)
		return NULL;
	return (EGLint *)(*env)->GetIntArrayElements(env, array, NULL);
}

static void release_attrib_list(JNIEnv *env, jintArray array, EGLint *elements)
{
	if (array)
		(*env)->ReleaseIntArrayElements(env, array, (jint *)elements, JNI_ABORT);
}

JNIEXPORT jlong JNICALL Java_android_opengl_EGL14_native_1eglGetDisplay(JNIEnv *env, jclass this, jint display_id)
{
	return _INTPTR(bionic_eglGetDisplay((EGLNativeDisplayType)(intptr_t)display_id));
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglInitialize(JNIEnv *env, jclass this, jlong dpy, jintArray version_ref)
{
	EGLint version[2] = {0, 0};
	EGLBoolean ret = eglInitialize(_PTR(dpy), &version[0], &version[1]);
	if (ret)
		(*env)->SetIntArrayRegion(env, version_ref, 0, 2, version);
	return ret;
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglTerminate(JNIEnv *env, jclass this, jlong dpy)
{
	return bionic_eglTerminate(_PTR(dpy));
}

JNIEXPORT jstring JNICALL Java_android_opengl_EGL14_native_1eglQueryString(JNIEnv *env, jclass this, jlong dpy, jint name)
{
	const char *string = eglQueryString(_PTR(dpy), name);
	return string ? _JSTRING(string) : NULL;
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglChooseConfig(JNIEnv *env, jclass this, jlong dpy, jintArray attrib_list_ref, jlongArray configs_ref, jint config_size, jintArray num_config_ref)
{
	EGLint *attrib_list = get_attrib_list(env, attrib_list_ref);
	EGLConfig *configs = configs_ref ? calloc(config_size, sizeof(EGLConfig)) : NULL;
	EGLint num_config = 0;

	EGLBoolean ret = bionic_eglChooseConfig(_PTR(dpy), attrib_list, configs, config_size, &num_config);
	release_attrib_list(env, attrib_list_ref, attrib_list);

	if (ret) {
		if (configs) {
			for (EGLint i = 0; i < num_config && i < config_size; i++) {
				jlong handle = _INTPTR(configs[i]);
				(*env)->SetLongArrayRegion(env, configs_ref, i, 1, &handle);
			}
		}
		(*env)->SetIntArrayRegion(env, num_config_ref, 0, 1, &num_config);
	}
	free(configs);
	return ret;
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglGetConfigAttrib(JNIEnv *env, jclass this, jlong dpy, jlong config, jint attribute, jintArray value_ref)
{
	EGLint value = 0;
	EGLBoolean ret = eglGetConfigAttrib(_PTR(dpy), _PTR(config), attribute, &value);
	if (ret)
		(*env)->SetIntArrayRegion(env, value_ref, 0, 1, &value);
	return ret;
}

JNIEXPORT jlong JNICALL Java_android_opengl_EGL14_native_1eglCreateContext(JNIEnv *env, jclass this, jlong dpy, jlong config, jlong share_context, jintArray attrib_list_ref)
{
	EGLint *attrib_list = get_attrib_list(env, attrib_list_ref);
	EGLContext context = eglCreateContext(_PTR(dpy), _PTR(config), _PTR(share_context), attrib_list);
	release_attrib_list(env, attrib_list_ref, attrib_list);
	return _INTPTR(context);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglDestroyContext(JNIEnv *env, jclass this, jlong dpy, jlong ctx)
{
	return eglDestroyContext(_PTR(dpy), _PTR(ctx));
}

JNIEXPORT jlong JNICALL Java_android_opengl_EGL14_native_1eglCreateWindowSurface(JNIEnv *env, jclass this, jlong dpy, jlong config, jobject surface, jintArray attrib_list_ref)
{
	struct ANativeWindow *native_window = ANativeWindow_fromSurface(env, surface);
	EGLint *attrib_list = get_attrib_list(env, attrib_list_ref);
	EGLSurface egl_surface = bionic_eglCreateWindowSurface(_PTR(dpy), _PTR(config), native_window, attrib_list);
	release_attrib_list(env, attrib_list_ref, attrib_list);
	if (native_window)
		ANativeWindow_release(native_window);
	return _INTPTR(egl_surface);
}

JNIEXPORT jlong JNICALL Java_android_opengl_EGL14_native_1eglCreateWindowSurfaceTexture(JNIEnv *env, jclass this, jlong dpy, jlong config, jobject surface_texture, jintArray attrib_list_ref)
{
	EGLint *attrib_list = get_attrib_list(env, attrib_list_ref);
	EGLSurface egl_surface = atl_egl_surface_texture_create(env, _PTR(dpy), _PTR(config), surface_texture, attrib_list);
	release_attrib_list(env, attrib_list_ref, attrib_list);
	return _INTPTR(egl_surface);
}

JNIEXPORT jlong JNICALL Java_android_opengl_EGL14_native_1eglCreatePbufferSurface(JNIEnv *env, jclass this, jlong dpy, jlong config, jintArray attrib_list_ref)
{
	EGLint *attrib_list = get_attrib_list(env, attrib_list_ref);
	EGLSurface egl_surface = bionic_eglCreatePbufferSurface(_PTR(dpy), _PTR(config), attrib_list);
	release_attrib_list(env, attrib_list_ref, attrib_list);
	return _INTPTR(egl_surface);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglDestroySurface(JNIEnv *env, jclass this, jlong dpy, jlong surface)
{
	atl_egl_surface_texture_release(_PTR(surface));
	return bionic_eglDestroySurface(_PTR(dpy), _PTR(surface));
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglQuerySurface(JNIEnv *env, jclass this, jlong dpy, jlong surface, jint attribute, jintArray value_ref)
{
	EGLint value = 0;
	EGLBoolean ret = bionic_eglQuerySurface(_PTR(dpy), _PTR(surface), attribute, &value);
	if (ret)
		(*env)->SetIntArrayRegion(env, value_ref, 0, 1, &value);
	return ret;
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglSurfaceAttrib(JNIEnv *env, jclass this, jlong dpy, jlong surface, jint attribute, jint value)
{
	return eglSurfaceAttrib(_PTR(dpy), _PTR(surface), attribute, value);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglQueryContext(JNIEnv *env, jclass this, jlong dpy, jlong ctx, jint attribute, jintArray value_ref)
{
	EGLint value = 0;
	EGLBoolean ret = eglQueryContext(_PTR(dpy), _PTR(ctx), attribute, &value);
	if (ret)
		(*env)->SetIntArrayRegion(env, value_ref, 0, 1, &value);
	return ret;
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglMakeCurrent(JNIEnv *env, jclass this, jlong dpy, jlong draw, jlong read, jlong ctx)
{
	return bionic_eglMakeCurrent(_PTR(dpy), _PTR(draw), _PTR(read), _PTR(ctx));
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglSwapBuffers(JNIEnv *env, jclass this, jlong dpy, jlong surface)
{
	if (atl_egl_surface_texture_swap(_PTR(dpy), _PTR(surface)))
		return JNI_TRUE;
	return bionic_eglSwapBuffers(_PTR(dpy), _PTR(surface));
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_native_1eglSwapInterval(JNIEnv *env, jclass this, jlong dpy, jint interval)
{
	return eglSwapInterval(_PTR(dpy), interval);
}

JNIEXPORT jlong JNICALL Java_android_opengl_EGL14_native_1eglGetCurrentDisplay(JNIEnv *env, jclass this)
{
	return _INTPTR(eglGetCurrentDisplay());
}

JNIEXPORT jlong JNICALL Java_android_opengl_EGL14_native_1eglGetCurrentContext(JNIEnv *env, jclass this)
{
	return _INTPTR(eglGetCurrentContext());
}

JNIEXPORT jlong JNICALL Java_android_opengl_EGL14_native_1eglGetCurrentSurface(JNIEnv *env, jclass this, jint readdraw)
{
	return _INTPTR(bionic_eglGetCurrentSurface(readdraw));
}

JNIEXPORT jint JNICALL Java_android_opengl_EGL14_eglGetError(JNIEnv *env, jclass this)
{
	return eglGetError();
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_eglBindAPI(JNIEnv *env, jclass this, jint api)
{
	return eglBindAPI(api);
}

JNIEXPORT jint JNICALL Java_android_opengl_EGL14_eglQueryAPI(JNIEnv *env, jclass this)
{
	return eglQueryAPI();
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_eglWaitClient(JNIEnv *env, jclass this)
{
	return eglWaitClient();
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_eglWaitGL(JNIEnv *env, jclass this)
{
	return eglWaitGL();
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_eglWaitNative(JNIEnv *env, jclass this, jint engine)
{
	return eglWaitNative(engine);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL14_eglReleaseThread(JNIEnv *env, jclass this)
{
	return eglReleaseThread();
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGLExt_native_1eglPresentationTimeANDROID(JNIEnv *env, jclass this, jlong dpy, jlong surface, jlong time)
{
	return bionic_eglPresentationTimeANDROID(_PTR(dpy), _PTR(surface), time);
}
