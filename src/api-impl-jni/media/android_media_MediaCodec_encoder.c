/*
 * The encoder half of android.media.MediaCodec: createInputSurface() and the
 * access units that come back out of dequeueOutputBuffer().
 *
 * The decoder half is android_media_MediaCodec.c; the two share nothing but
 * the Java class, because an encoder here is a GStreamer pipeline fed by a
 * Surface (video_encoder.h) rather than a codec_backend instance.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"
#include "../util.h"

#include "video_encoder.h"

#include "../generated_headers/android_media_MediaCodec.h"

/* android.media.MediaCodec.BUFFER_FLAG_* */
#define BUFFER_FLAG_KEY_FRAME      0x1
#define BUFFER_FLAG_END_OF_STREAM  0x4

JNIEXPORT jlong JNICALL Java_android_media_MediaCodec_native_1encoder_1create(JNIEnv *env, jclass class,
                                                                              jint width, jint height,
                                                                              jint fps, jint bit_rate)
{
	return _INTPTR(atl_video_encoder_new(width, height, fps, bit_rate, NULL));
}

JNIEXPORT void JNICALL Java_android_media_MediaCodec_native_1encoder_1inputSurface(JNIEnv *env, jclass class,
                                                                                   jlong ptr, jobject surface)
{
	atl_video_encoder_attach_surface(env, _PTR(ptr), surface);
}

JNIEXPORT jboolean JNICALL Java_android_media_MediaCodec_native_1encoder_1start(JNIEnv *env, jclass class, jlong ptr)
{
	return atl_video_encoder_start(_PTR(ptr)) ? JNI_TRUE : JNI_FALSE;
}

/* 0 with the BufferInfo filled in (size 0 plus the EOS flag at the end of the
 * stream), or -1 when nothing has been encoded yet */
JNIEXPORT jint JNICALL Java_android_media_MediaCodec_native_1encoder_1dequeue(JNIEnv *env, jclass class,
                                                                              jlong ptr, jobject buffer,
                                                                              jobject buffer_info, jlong timeout_us)
{
	struct atl_video_encoder *encoder = _PTR(ptr);
	jarray array_ref;
	jbyte *array;
	uint8_t *scratch, *data;
	int64_t pts_us = 0;
	bool keyframe = false;
	int capacity, size;

	if (!encoder || !buffer || !buffer_info)
		return -1;

	/* pull into scratch first: the pull blocks, and blocking inside a critical
	 * array region is how a heap ByteBuffer wedges the GC */
	capacity = get_nio_buffer_size(env, buffer);
	scratch = capacity > 0 ? malloc(capacity) : NULL;
	if (!scratch)
		return -1;
	size = atl_video_encoder_pull(encoder, scratch, capacity, &pts_us, &keyframe, timeout_us);

	if (size == ATL_ENCODER_AGAIN) {
		free(scratch);
		return -1;
	}
	if (size > 0) {
		data = get_nio_buffer(env, buffer, &array_ref, &array);
		if (data)
			memcpy(data, scratch, size);
		release_nio_buffer(env, array_ref, array);
	}
	free(scratch);

	_SET_INT_FIELD(buffer_info, "offset", 0);
	if (size == ATL_ENCODER_EOS) {
		_SET_INT_FIELD(buffer_info, "size", 0);
		_SET_INT_FIELD(buffer_info, "flags", BUFFER_FLAG_END_OF_STREAM);
		_SET_LONG_FIELD(buffer_info, "presentationTimeUs", 0);
		return 0;
	}
	_SET_INT_FIELD(buffer_info, "size", size);
	_SET_INT_FIELD(buffer_info, "flags", keyframe ? BUFFER_FLAG_KEY_FRAME : 0);
	_SET_LONG_FIELD(buffer_info, "presentationTimeUs", (jlong)pts_us);
	return 0;
}

JNIEXPORT jbyteArray JNICALL Java_android_media_MediaCodec_native_1encoder_1csd(JNIEnv *env, jclass class, jlong ptr)
{
	size_t size = 0;
	const uint8_t *csd = atl_video_encoder_get_csd(_PTR(ptr), &size);
	jbyteArray array;

	if (!csd || !size)
		return NULL;
	array = (*env)->NewByteArray(env, size);
	(*env)->SetByteArrayRegion(env, array, 0, size, (const jbyte *)csd);
	return array;
}

JNIEXPORT void JNICALL Java_android_media_MediaCodec_native_1encoder_1signalEndOfInputStream(JNIEnv *env, jclass class,
                                                                                             jlong ptr)
{
	atl_video_encoder_signal_eos(_PTR(ptr));
}

JNIEXPORT void JNICALL Java_android_media_MediaCodec_native_1encoder_1finish(JNIEnv *env, jclass class, jlong ptr)
{
	atl_video_encoder_finish(_PTR(ptr));
}

JNIEXPORT void JNICALL Java_android_media_MediaCodec_native_1encoder_1release(JNIEnv *env, jclass class, jlong ptr)
{
	atl_video_encoder_unref(_PTR(ptr));
}
