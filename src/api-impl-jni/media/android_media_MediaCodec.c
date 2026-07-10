/*
 * android.media.MediaCodec JNI glue.
 *
 * All decoding goes through the pluggable codec backend (codec_backend.h):
 * ffmpeg on mainline, the hybris/libmediandk backend later for halium
 * devices. This file only marshals buffers between Java and the backend and
 * posts rendered video frames to the target Surface (SurfaceView draws them
 * into the Skia scene, same path as SurfaceHolder.lockCanvas frames).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"
#include "../util.h"
#include "../generated_headers/android_media_MediaCodec.h"

#include "codec_backend.h"

/* video_frame_skia.cpp */
extern void *atl_video_frame_wrap_skbitmap(uint8_t *pixels, int width, int height);

#define INFO_TRY_AGAIN_LATER -1

struct atl_media_codec {
	const struct atl_codec_backend *backend;
	struct atl_codec *codec;
	jobject surface; /* global ref, video only (may be NULL) */
};

JNIEXPORT jlong JNICALL Java_android_media_MediaCodec_native_1constructor(JNIEnv *env, jobject this, jstring codec_name_jstr)
{
	const struct atl_codec_backend *backend = atl_codec_backend_get();
	const char *codec_name = (*env)->GetStringUTFChars(env, codec_name_jstr, NULL);
	struct atl_codec *codec = backend->create(codec_name);
	(*env)->ReleaseStringUTFChars(env, codec_name_jstr, codec_name);
	if (!codec)
		return 0;

	struct atl_media_codec *mc = calloc(1, sizeof(*mc));
	mc->backend = backend;
	mc->codec = codec;
	return _INTPTR(mc);
}

JNIEXPORT void JNICALL Java_android_media_MediaCodec_native_1configure_1audio(JNIEnv *env, jobject this, jlong ptr, jobject extradata, jint sample_rate, jint nb_channels)
{
	struct atl_media_codec *mc = _PTR(ptr);
	jarray array_ref;
	jbyte *array;
	const uint8_t *data = NULL;
	size_t size = 0;

	if (extradata) {
		size = get_nio_buffer_size(env, extradata);
		data = get_nio_buffer(env, extradata, &array_ref, &array);
	}
	mc->backend->configure_audio(mc->codec, data, size, sample_rate, nb_channels);
	if (extradata)
		release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_media_MediaCodec_native_1configure_1video(JNIEnv *env, jobject this, jlong ptr, jobject csd0, jobject csd1, jobject surface_obj)
{
	struct atl_media_codec *mc = _PTR(ptr);
	jarray array_ref;
	jbyte *array;
	size_t csd0_size = csd0 ? get_nio_buffer_size(env, csd0) : 0;
	size_t csd1_size = csd1 ? get_nio_buffer_size(env, csd1) : 0;

	uint8_t *csd = NULL;
	if (csd0_size + csd1_size) {
		csd = malloc(csd0_size + csd1_size);
		if (csd0) {
			memcpy(csd, get_nio_buffer(env, csd0, &array_ref, &array), csd0_size);
			release_nio_buffer(env, array_ref, array);
		}
		if (csd1) {
			memcpy(csd + csd0_size, get_nio_buffer(env, csd1, &array_ref, &array), csd1_size);
			release_nio_buffer(env, array_ref, array);
		}
	}
	mc->backend->configure_video(mc->codec, csd, csd0_size + csd1_size);
	free(csd);

	if (surface_obj)
		mc->surface = _REF(surface_obj);
}

JNIEXPORT void JNICALL Java_android_media_MediaCodec_native_1start(JNIEnv *env, jobject this, jlong ptr)
{
	struct atl_media_codec *mc = _PTR(ptr);
	mc->backend->start(mc->codec);
}

JNIEXPORT jint JNICALL Java_android_media_MediaCodec_native_1queueInputBuffer(JNIEnv *env, jobject this, jlong ptr, jobject buffer, jlong presentationTimeUs)
{
	struct atl_media_codec *mc = _PTR(ptr);
	jarray array_ref;
	jbyte *array;
	int ret;

	if (buffer) {
		size_t size = get_nio_buffer_size(env, buffer);
		const uint8_t *data = get_nio_buffer(env, buffer, &array_ref, &array);
		ret = mc->backend->queue_input(mc->codec, data, size, presentationTimeUs);
		release_nio_buffer(env, array_ref, array);
	} else { /* end of stream */
		ret = mc->backend->queue_input(mc->codec, NULL, 0, 0);
	}
	return ret;
}

JNIEXPORT jint JNICALL Java_android_media_MediaCodec_native_1dequeueOutputBuffer(JNIEnv *env, jobject this, jlong ptr, jobject buffer, jobject buffer_info)
{
	struct atl_media_codec *mc = _PTR(ptr);
	jarray array_ref;
	jbyte *array;

	if (mc->backend->is_video(mc->codec)) {
		int status;
		struct atl_video_frame *frame = mc->backend->dequeue_video(mc->codec, &status);
		if (!frame && status == ATL_CODEC_AGAIN)
			return INFO_TRY_AGAIN_LATER;

		/* stash the frame handle in the output buffer for releaseOutputBuffer */
		uint8_t *raw_buffer = get_nio_buffer(env, buffer, &array_ref, &array);
		*((struct atl_video_frame **)raw_buffer) = frame;
		release_nio_buffer(env, array_ref, array);

		if (!frame) { /* EOS (or error, reported as EOS to stop the loop) */
			_SET_INT_FIELD(buffer_info, "flags", android_media_MediaCodec_BUFFER_FLAG_END_OF_STREAM);
			_SET_INT_FIELD(buffer_info, "offset", 0);
			_SET_INT_FIELD(buffer_info, "size", 0);
			_SET_LONG_FIELD(buffer_info, "presentationTimeUs", 0);
			return 0;
		}
		_SET_INT_FIELD(buffer_info, "flags", 0);
		_SET_INT_FIELD(buffer_info, "offset", 0);
		_SET_INT_FIELD(buffer_info, "size", sizeof(struct atl_video_frame *));
		_SET_LONG_FIELD(buffer_info, "presentationTimeUs", frame->pts_us);
		return 0;
	} else {
		int64_t pts_us = 0;
		uint8_t *out = get_nio_buffer(env, buffer, &array_ref, &array);
		int ret = mc->backend->dequeue_audio(mc->codec, out, get_nio_buffer_size(env, buffer), &pts_us);
		release_nio_buffer(env, array_ref, array);

		if (ret == ATL_CODEC_AGAIN || ret == ATL_CODEC_ERROR)
			return INFO_TRY_AGAIN_LATER;
		if (ret == ATL_CODEC_EOS) {
			_SET_INT_FIELD(buffer_info, "flags", android_media_MediaCodec_BUFFER_FLAG_END_OF_STREAM);
			_SET_INT_FIELD(buffer_info, "offset", 0);
			_SET_INT_FIELD(buffer_info, "size", 0);
			_SET_LONG_FIELD(buffer_info, "presentationTimeUs", 0);
			return 0;
		}
		_SET_INT_FIELD(buffer_info, "flags", 0);
		_SET_INT_FIELD(buffer_info, "offset", 0);
		_SET_INT_FIELD(buffer_info, "size", ret);
		_SET_LONG_FIELD(buffer_info, "presentationTimeUs", pts_us);
		return 0;
	}
}

JNIEXPORT void JNICALL Java_android_media_MediaCodec_native_1releaseOutputBuffer(JNIEnv *env, jobject this, jlong ptr, jobject buffer, jboolean render)
{
	struct atl_media_codec *mc = _PTR(ptr);
	jarray array_ref;
	jbyte *array;

	if (!mc->backend->is_video(mc->codec))
		return;

	struct atl_video_frame **raw_buffer = get_nio_buffer(env, buffer, &array_ref, &array);
	struct atl_video_frame *frame = *raw_buffer;
	*raw_buffer = NULL;
	release_nio_buffer(env, array_ref, array);

	if (!frame)
		return;
	if (!render || !mc->surface) {
		atl_video_frame_free(frame);
		return;
	}

	int width = frame->width;
	int height = frame->height;
	void *skbitmap = atl_video_frame_wrap_skbitmap(atl_video_frame_take_pixels(frame), width, height);
	if (!skbitmap)
		return;

	/* Surface.postFrame(Bitmap) -> SurfaceView.postFrame -> postInvalidate;
	 * safe to call from the app's codec thread */
	jobject bitmap = (*env)->CallStaticObjectMethod(env, handle_cache.bitmap.class,
	                                                handle_cache.bitmap.fromNative, _INTPTR(skbitmap));
	(*env)->CallVoidMethod(env, mc->surface, handle_cache.surface.postFrame, bitmap);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	(*env)->DeleteLocalRef(env, bitmap);
}

JNIEXPORT void JNICALL Java_android_media_MediaCodec_native_1release(JNIEnv *env, jobject this, jlong ptr)
{
	struct atl_media_codec *mc = _PTR(ptr);

	mc->backend->release(mc->codec);
	if (mc->surface)
		_UNREF(mc->surface);
	free(mc);
}
