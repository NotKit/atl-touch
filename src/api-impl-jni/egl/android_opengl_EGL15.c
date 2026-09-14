/*
 * android.opengl.EGL15's fence syncs. Like EGL14 next door, the Java side boxes
 * the handles, so everything here is raw jlong.
 *
 * EGL 1.5 renamed the KHR_fence_sync entry points and widened their attribute
 * lists from EGLint to EGLAttrib, and a device can have either spelling: the
 * host's libEGL headers need not declare the 1.5 ones at all (they are only
 * resolved through eglGetProcAddress here), and hybris EGL on a Halium device
 * answers with whatever the vendor driver has. Both are tried, 1.5 first.
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../defines.h"
#include "../util.h"

#include "../generated_headers/android_opengl_EGL15.h"

/* EGL 1.5 spellings, in case the host headers predate them */
typedef void *ATLEGLSync;
typedef intptr_t ATLEGLAttrib;

static struct {
	bool tried;
	/* EGL 1.5 */
	ATLEGLSync (*createSync)(EGLDisplay, EGLenum, const ATLEGLAttrib *);
	EGLBoolean (*destroySync)(EGLDisplay, ATLEGLSync);
	EGLint (*clientWaitSync)(EGLDisplay, ATLEGLSync, EGLint, EGLTime);
	EGLBoolean (*waitSync)(EGLDisplay, ATLEGLSync, EGLint);
	EGLBoolean (*getSyncAttrib)(EGLDisplay, ATLEGLSync, EGLint, ATLEGLAttrib *);
	/* EGL_KHR_fence_sync, whose attribute lists are EGLint */
	ATLEGLSync (*createSyncKHR)(EGLDisplay, EGLenum, const EGLint *);
	EGLBoolean (*destroySyncKHR)(EGLDisplay, ATLEGLSync);
	EGLint (*clientWaitSyncKHR)(EGLDisplay, ATLEGLSync, EGLint, EGLTime);
	EGLBoolean (*waitSyncKHR)(EGLDisplay, ATLEGLSync, EGLint);
	EGLBoolean (*getSyncAttribKHR)(EGLDisplay, ATLEGLSync, EGLint, EGLint *);
} sync_api;

static void sync_api_load(void)
{
	if (sync_api.tried)
		return;
	sync_api.tried = true;

#define PROC(field, name) sync_api.field = (void *)eglGetProcAddress(name)
	PROC(createSync, "eglCreateSync");
	PROC(destroySync, "eglDestroySync");
	PROC(clientWaitSync, "eglClientWaitSync");
	PROC(waitSync, "eglWaitSync");
	PROC(getSyncAttrib, "eglGetSyncAttrib");
	PROC(createSyncKHR, "eglCreateSyncKHR");
	PROC(destroySyncKHR, "eglDestroySyncKHR");
	PROC(clientWaitSyncKHR, "eglClientWaitSyncKHR");
	PROC(waitSyncKHR, "eglWaitSyncKHR");
	PROC(getSyncAttribKHR, "eglGetSyncAttribKHR");
#undef PROC

	if (!sync_api.createSync && !sync_api.createSyncKHR)
		fprintf(stderr, "EGL15: this EGL has no fence syncs, neither 1.5 nor "
		                "EGL_KHR_fence_sync; every sync will be EGL_NO_SYNC\n");
}

JNIEXPORT jlong JNICALL Java_android_opengl_EGL15_native_1eglCreateSync(JNIEnv *env, jclass this,
                                                                        jlong dpy, jint type,
                                                                        jlongArray attrib_list)
{
	jsize count = attrib_list ? (*env)->GetArrayLength(env, attrib_list) : 0;
	jlong *values = NULL;
	void *sync = NULL;

	sync_api_load();
	if (count)
		values = (*env)->GetLongArrayElements(env, attrib_list, NULL);

	if (sync_api.createSync) {
		ATLEGLAttrib stack[16];
		ATLEGLAttrib *attribs = NULL;

		/* an EGLAttrib list is terminated by EGL_NONE; the Java array
		 * already carries it, and a NULL list means "no attributes" */
		if (count > 0 && count <= (jsize)(sizeof(stack) / sizeof(stack[0]))) {
			for (jsize i = 0; i < count; i++)
				stack[i] = (ATLEGLAttrib)values[i];
			attribs = stack;
		}
		sync = sync_api.createSync(_PTR(dpy), (EGLenum)type, attribs);
	} else if (sync_api.createSyncKHR) {
		EGLint stack[16];
		EGLint *attribs = NULL;

		if (count > 0 && count <= (jsize)(sizeof(stack) / sizeof(stack[0]))) {
			for (jsize i = 0; i < count; i++)
				stack[i] = (EGLint)values[i];
			attribs = stack;
		}
		sync = sync_api.createSyncKHR(_PTR(dpy), (EGLenum)type, attribs);
	}

	if (values)
		(*env)->ReleaseLongArrayElements(env, attrib_list, values, JNI_ABORT);
	return _INTPTR(sync);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL15_native_1eglDestroySync(JNIEnv *env, jclass this,
                                                                            jlong dpy, jlong sync)
{
	sync_api_load();
	if (sync_api.destroySync)
		return sync_api.destroySync(_PTR(dpy), _PTR(sync));
	if (sync_api.destroySyncKHR)
		return sync_api.destroySyncKHR(_PTR(dpy), _PTR(sync));
	return JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_android_opengl_EGL15_native_1eglClientWaitSync(JNIEnv *env, jclass this,
                                                                           jlong dpy, jlong sync,
                                                                           jint flags, jlong timeout)
{
	sync_api_load();
	if (sync_api.clientWaitSync)
		return sync_api.clientWaitSync(_PTR(dpy), _PTR(sync), flags, (EGLTime)timeout);
	if (sync_api.clientWaitSyncKHR)
		return sync_api.clientWaitSyncKHR(_PTR(dpy), _PTR(sync), flags, (EGLTime)timeout);
	return EGL_FALSE;
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL15_native_1eglWaitSync(JNIEnv *env, jclass this,
                                                                         jlong dpy, jlong sync,
                                                                         jint flags)
{
	sync_api_load();
	if (sync_api.waitSync)
		return sync_api.waitSync(_PTR(dpy), _PTR(sync), flags);
	if (sync_api.waitSyncKHR)
		return sync_api.waitSyncKHR(_PTR(dpy), _PTR(sync), flags);
	return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_android_opengl_EGL15_native_1eglGetSyncAttrib(JNIEnv *env, jclass this,
                                                                              jlong dpy, jlong sync,
                                                                              jint attribute,
                                                                              jlongArray value)
{
	jlong out = 0;
	EGLBoolean ok = EGL_FALSE;

	sync_api_load();
	if (sync_api.getSyncAttrib) {
		ATLEGLAttrib attrib = 0;

		ok = sync_api.getSyncAttrib(_PTR(dpy), _PTR(sync), attribute, &attrib);
		out = attrib;
	} else if (sync_api.getSyncAttribKHR) {
		EGLint attrib = 0;

		ok = sync_api.getSyncAttribKHR(_PTR(dpy), _PTR(sync), attribute, &attrib);
		out = attrib;
	}
	if (ok)
		(*env)->SetLongArrayRegion(env, value, 0, 1, &out);
	return ok;
}
