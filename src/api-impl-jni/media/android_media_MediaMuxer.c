/*
 * android.media.MediaMuxer, see video_muxer.h. The samples arrive in the
 * app's own ByteBuffers, which are heap buffers as often as direct ones, so
 * they go through the nio helpers rather than GetDirectBufferAddress.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../defines.h"
#include "../util.h"

#include "video_muxer.h"

#include "../generated_headers/android_media_MediaMuxer.h"

JNIEXPORT jlong JNICALL Java_android_media_MediaMuxer_native_1create(JNIEnv *env, jclass class, jstring path_str)
{
	struct atl_video_muxer *muxer;
	const char *path;

	if (!path_str)
		return 0;
	path = _CSTRING(path_str);
	muxer = atl_video_muxer_new(path);
	(*env)->ReleaseStringUTFChars(env, path_str, path);
	return _INTPTR(muxer);
}

JNIEXPORT jboolean JNICALL Java_android_media_MediaMuxer_native_1addTrack(JNIEnv *env, jclass class, jlong ptr,
                                                                          jint width, jint height, jint fps,
                                                                          jobject csd_buffer)
{
	struct atl_video_muxer *muxer = _PTR(ptr);
	jarray array_ref;
	jbyte *array;
	const uint8_t *csd = NULL;
	size_t csd_size = 0;
	bool ok;

	if (!muxer)
		return JNI_FALSE;
	if (csd_buffer) {
		csd_size = get_nio_buffer_size(env, csd_buffer);
		csd = get_nio_buffer(env, csd_buffer, &array_ref, &array);
	}
	ok = atl_video_muxer_add_track(muxer, width, height, fps, csd, csd_size);
	if (csd_buffer)
		release_nio_buffer(env, array_ref, array);
	return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_android_media_MediaMuxer_native_1setOrientation(JNIEnv *env, jclass class, jlong ptr,
                                                                            jint degrees)
{
	atl_video_muxer_set_orientation(_PTR(ptr), degrees);
}

JNIEXPORT jboolean JNICALL Java_android_media_MediaMuxer_native_1start(JNIEnv *env, jclass class, jlong ptr)
{
	return atl_video_muxer_start(_PTR(ptr)) ? JNI_TRUE : JNI_FALSE;
}

/* the buffer arrives positioned on the sample, the way AOSP's own muxer
 * windows it */
JNIEXPORT void JNICALL Java_android_media_MediaMuxer_native_1write(JNIEnv *env, jclass class, jlong ptr,
                                                                   jobject buffer, jint size,
                                                                   jlong pts_us, jboolean keyframe)
{
	struct atl_video_muxer *muxer = _PTR(ptr);
	jarray array_ref;
	jbyte *array;
	const uint8_t *data;
	int available;

	if (!muxer || !buffer || size <= 0)
		return;
	available = get_nio_buffer_size(env, buffer);
	if (size > available) {
		fprintf(stderr, "MediaMuxer: a %d byte sample does not fit in the %d bytes left\n",
		        size, available);
		return;
	}
	data = get_nio_buffer(env, buffer, &array_ref, &array);
	if (data)
		atl_video_muxer_write(muxer, data, size, pts_us, keyframe == JNI_TRUE);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_media_MediaMuxer_native_1stop(JNIEnv *env, jclass class, jlong ptr)
{
	atl_video_muxer_stop(_PTR(ptr));
}

JNIEXPORT void JNICALL Java_android_media_MediaMuxer_native_1release(JNIEnv *env, jclass class, jlong ptr)
{
	atl_video_muxer_free(_PTR(ptr));
}
