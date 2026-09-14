/*
 * libnativewindow.so for an app's own native code: the AHardwareBuffer half,
 * forwarded to the device's Android library through libhybris.
 * See hybris_ndk.h.
 *
 * A gralloc buffer is the currency of the whole camera path on a real device -
 * the HAL writes into one, the GPU samples it, ATL's zero-copy preview binds it
 * as an EGLImage - and ATL has no gralloc of its own, so these forward
 * rather than emulate. The ANativeWindow_* entry points stay ATL's own
 * (src/libandroid/native_window.c): an app's window is an ATL surface, not an
 * Android one.
 *
 * The two JNI-shaped calls, AHardwareBuffer_{from,to}HardwareBuffer, cannot be
 * forwarded: Java's android.hardware.HardwareBuffer under ATL is ATL's own
 * object, and the Android implementation would read a pointer of the wrong
 * type out of it. AHardwareBuffer_fromHardwareBuffer is answered here instead,
 * out of ATL's side of the object: the buffer allocates a real gralloc handle
 * on first ask (atl_hardware_buffer_native, in the main library) and hands
 * back the same one every time, so an app that binds it as an EGLImage,
 * renders into it and then presents it is talking about one buffer throughout.
 * Google Camera's viewfinder is exactly that app.
 */

#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <jni.h>

#include <third_party/android-headers/android_compat.h>

#include <android/hardware_buffer.h>
#include <android/native_window.h>

#include "hybris_ndk.h"

#define SONAME "libnativewindow.so"

ATL_HYBRIS_FORWARD(SONAME, int, AHardwareBuffer_allocate,
                   (const AHardwareBuffer_Desc *desc, AHardwareBuffer **outBuffer),
                   (desc, outBuffer), -ENOSYS)

ATL_HYBRIS_FORWARD_VOID(SONAME, AHardwareBuffer_acquire, (AHardwareBuffer * buffer), (buffer))

ATL_HYBRIS_FORWARD_VOID(SONAME, AHardwareBuffer_release, (AHardwareBuffer * buffer), (buffer))

ATL_HYBRIS_FORWARD_VOID(SONAME, AHardwareBuffer_describe,
                        (const AHardwareBuffer *buffer, AHardwareBuffer_Desc *outDesc),
                        (buffer, outDesc))

ATL_HYBRIS_FORWARD(SONAME, int, AHardwareBuffer_lock,
                   (AHardwareBuffer * buffer, uint64_t usage, int32_t fence, const ARect *rect,
                    void **outVirtualAddress),
                   (buffer, usage, fence, rect, outVirtualAddress), -ENOSYS)

ATL_HYBRIS_FORWARD(SONAME, int, AHardwareBuffer_lockPlanes,
                   (AHardwareBuffer * buffer, uint64_t usage, int32_t fence, const ARect *rect,
                    AHardwareBuffer_Planes *outPlanes),
                   (buffer, usage, fence, rect, outPlanes), -ENOSYS)

ATL_HYBRIS_FORWARD(SONAME, int, AHardwareBuffer_unlock,
                   (AHardwareBuffer * buffer, int32_t *fence), (buffer, fence), -ENOSYS)

ATL_HYBRIS_FORWARD(SONAME, int, AHardwareBuffer_isSupported,
                   (const AHardwareBuffer_Desc *desc), (desc), 0)

/* an Android hint that a window should allocate its buffers now; ATL's windows
 * allocate on demand, so there is nothing to do and nothing to report */
void ANativeWindow_tryAllocateBuffers(ANativeWindow *window)
{
}

static void warn_java_bridge(const char *name)
{
	static bool logged;

	if (logged)
		return;
	logged = true;
	fprintf(stderr, "atl-ndk: %s is not supported: an ATL HardwareBuffer is a plain "
	                "allocation, not a gralloc handle\n", name);
}

/* ATL's own side of android.hardware.HardwareBuffer: the jlong the Java object
 * carries is a struct atl_hardware_buffer *, and the main library turns that
 * into a gralloc handle. Resolved by name because this library is loaded for
 * an app's native code and does not link against the main one. */
AHardwareBuffer *AHardwareBuffer_fromHardwareBuffer(JNIEnv *env, jobject hardwareBufferObj)
{
	static void *(*to_gralloc)(void *);
	static bool resolved;
	jclass class;
	jfieldID field;
	jlong ptr;

	if (!env || !hardwareBufferObj)
		return NULL;
	if (!resolved) {
		resolved = true;
		to_gralloc = dlsym(RTLD_DEFAULT, "atl_hardware_buffer_native");
		if (!to_gralloc) {
			/* ART loads a JNI library into its own scope, so the global
			 * one need not have it; the library is certainly loaded */
			void *main_lib = dlopen("libtranslation_layer_main.so", RTLD_LAZY | RTLD_NOLOAD);

			if (main_lib)
				to_gralloc = dlsym(main_lib, "atl_hardware_buffer_native");
		}
	}
	if (!to_gralloc) {
		warn_java_bridge("AHardwareBuffer_fromHardwareBuffer");
		return NULL;
	}

	class = (*env)->GetObjectClass(env, hardwareBufferObj);
	field = (*env)->GetFieldID(env, class, "nativePtr", "J");
	if (!field) {
		(*env)->ExceptionClear(env);
		warn_java_bridge("AHardwareBuffer_fromHardwareBuffer");
		return NULL;
	}
	ptr = (*env)->GetLongField(env, hardwareBufferObj, field);
	if (!ptr)
		return NULL;
	return to_gralloc((void *)(intptr_t)ptr);
}

jobject AHardwareBuffer_toHardwareBuffer(JNIEnv *env, AHardwareBuffer *hardwareBuffer)
{
	warn_java_bridge("AHardwareBuffer_toHardwareBuffer");
	return NULL;
}
