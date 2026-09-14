/*
 * android.media.MediaRecorder: a recording Surface and the encoder behind it.
 *
 * The Java side owns the state machine; this is only the encoder's lifetime
 * plus the Surface field that lets a camera2 session find it (the third kind
 * of sink after a SurfaceTexture and an ImageReader).
 */

#include <stdio.h>
#include <stdlib.h>

#include "../defines.h"

#include "video_encoder.h"

#include "../generated_headers/android_media_MediaRecorder.h"

JNIEXPORT jlong JNICALL Java_android_media_MediaRecorder_native_1prepare(JNIEnv *env, jclass class,
                                                                         jint width, jint height,
                                                                         jint fps, jint bit_rate,
                                                                         jstring path_str, jobject surface)
{
	struct atl_video_encoder *encoder;
	const char *path;

	if (!path_str || !surface)
		return 0;

	path = _CSTRING(path_str);
	encoder = atl_video_encoder_new(width, height, fps, bit_rate, path);
	(*env)->ReleaseStringUTFChars(env, path_str, path);
	if (!encoder)
		return 0;

	atl_video_encoder_attach_surface(env, encoder, surface);
	return _INTPTR(encoder);
}

JNIEXPORT jboolean JNICALL Java_android_media_MediaRecorder_native_1start(JNIEnv *env, jclass class, jlong ptr)
{
	return atl_video_encoder_start(_PTR(ptr)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_android_media_MediaRecorder_native_1stop(JNIEnv *env, jclass class, jlong ptr)
{
	atl_video_encoder_finish(_PTR(ptr));
}

JNIEXPORT jlong JNICALL Java_android_media_MediaRecorder_native_1frameCount(JNIEnv *env, jclass class, jlong ptr)
{
	return (jlong)atl_video_encoder_get_frame_count(_PTR(ptr));
}

JNIEXPORT void JNICALL Java_android_media_MediaRecorder_native_1release(JNIEnv *env, jclass class, jlong ptr)
{
	atl_video_encoder_unref(_PTR(ptr));
}
