/*
 * android.media.ImageWriter and the reprocessing input queue, see
 * image_writer.h.
 *
 * Two ways in: dequeueInputImage() hands the app an empty NV21 buffer with
 * plane descriptors to fill, and queueInputImage() takes an Image that came
 * from an ImageReader and copies its pixels in. Both end up as the same
 * queued frame, which the next reprocess capture takes and pushes through the
 * session's outputs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "../defines.h"

#include "camera_backend.h"
#include "camera_streams.h"
#include "image_reader.h"
#include "image_writer.h"

#include "../generated_headers/android_media_ImageWriter.h"

/* android.graphics.ImageFormat */
#define FORMAT_YUV_420_888 0x23
#define FORMAT_PRIVATE     0x22

#define MAX_PLANES 3

struct atl_image_writer_frame {
	struct atl_image_writer_frame *next;
	struct atl_image_writer *writer; /* set while the app holds the image */

	uint8_t *data;
	size_t capacity;
	size_t size;
	int width;
	int height;
	int64_t timestamp;
};

struct atl_image_writer {
	gint refcount;

	GMutex lock;
	bool closed;

	/* a real session's input: no frames of our own, the camera takes the
	 * Image's HAL buffer and hands it back when it is done */
	bool real;
	struct atl_image_writer_sink sink;
	int outstanding;  /* buffers the camera holds */
	JavaVM *jvm;
	jweak self;       /* weak: the app owns the ImageWriter's lifetime */

	int width;
	int height;
	int format;
	int max_images;

	struct atl_image_writer_frame *queue_head;
	struct atl_image_writer_frame *queue_tail;
	struct atl_image_writer_frame *free_list;
	int queued;
	int dequeued; /* handed to the app, not queued back yet */

	uint64_t written;
	uint64_t consumed;
};

static JNIEnv *writer_env(struct atl_image_writer *writer);

static void frame_free(struct atl_image_writer_frame *frame)
{
	if (!frame)
		return;
	free(frame->data);
	free(frame);
}

static bool frame_reserve(struct atl_image_writer_frame *frame, size_t size)
{
	if (frame->capacity >= size)
		return true;
	free(frame->data);
	frame->data = malloc(size);
	frame->capacity = frame->data ? size : 0;
	return frame->data != NULL;
}

struct atl_image_writer *atl_image_writer_new(int width, int height, int format, int max_images)
{
	struct atl_image_writer *writer;

	if (width < 2 || height < 2 || max_images < 1)
		return NULL;
	if (format != FORMAT_YUV_420_888 && format != FORMAT_PRIVATE) {
		fprintf(stderr, "ImageWriter: unsupported input format 0x%x\n", format);
		return NULL;
	}

	writer = calloc(1, sizeof(*writer));
	writer->refcount = 1;
	writer->width = width & ~1;
	writer->height = height & ~1;
	writer->format = format;
	writer->max_images = max_images;
	g_mutex_init(&writer->lock);
	return writer;
}

struct atl_image_writer *atl_image_writer_new_input(int width, int height, int format,
                                                    int max_images,
                                                    const struct atl_image_writer_sink *sink)
{
	struct atl_image_writer *writer;

	if (width < 2 || height < 2 || max_images < 1 || !sink || !sink->queue)
		return NULL;

	writer = calloc(1, sizeof(*writer));
	writer->refcount = 1;
	writer->real = true;
	writer->sink = *sink;
	/* a raw input's width is odd-sized on no sensor, but rounding it here
	 * would describe a buffer the camera never sent */
	writer->width = width;
	writer->height = height;
	writer->format = format;
	writer->max_images = max_images;
	g_mutex_init(&writer->lock);
	return writer;
}

bool atl_image_writer_is_real(struct atl_image_writer *writer)
{
	return writer && writer->real;
}

void atl_image_writer_clear_sink(struct atl_image_writer *writer)
{
	struct atl_image_writer_sink sink;

	if (!writer)
		return;
	g_mutex_lock(&writer->lock);
	sink = writer->sink;
	memset(&writer->sink, 0, sizeof(writer->sink));
	g_mutex_unlock(&writer->lock);
	if (sink.unref && sink.user)
		sink.unref(sink.user);
}

void atl_image_writer_ref(struct atl_image_writer *writer)
{
	if (writer)
		g_atomic_int_inc(&writer->refcount);
}

void atl_image_writer_unref(struct atl_image_writer *writer)
{
	struct atl_image_writer_frame *frame;

	if (!writer || !g_atomic_int_dec_and_test(&writer->refcount))
		return;

	while ((frame = writer->queue_head)) {
		writer->queue_head = frame->next;
		frame_free(frame);
	}
	while ((frame = writer->free_list)) {
		writer->free_list = frame->next;
		frame_free(frame);
	}
	if (writer->sink.unref && writer->sink.user)
		writer->sink.unref(writer->sink.user);
	if (writer->self) {
		JNIEnv *env = writer_env(writer);

		if (env)
			(*env)->DeleteWeakGlobalRef(env, writer->self);
	}
	g_mutex_clear(&writer->lock);
	free(writer);
}

static JNIEnv *writer_env(struct atl_image_writer *writer)
{
	JNIEnv *env;

	if (!writer->jvm)
		return NULL;
	if ((*writer->jvm)->GetEnv(writer->jvm, (void **)&env, JNI_VERSION_1_6) == JNI_OK)
		return env;

	JavaVMAttachArgs args = {JNI_VERSION_1_6, "atl-image-writer", NULL};
	if ((*writer->jvm)->AttachCurrentThreadAsDaemon(writer->jvm, (void **)&env, &args) != JNI_OK) {
		fprintf(stderr, "ImageWriter: failed to attach the release thread\n");
		return NULL;
	}
	return env;
}

/* onImageReleased is app code: it runs on the dispatch thread, never on the
 * camera's binder thread */
static gboolean deliver_image_released(gpointer user)
{
	struct atl_image_writer *writer = user;
	JNIEnv *env = writer_env(writer);
	jweak weak;
	jobject self;

	g_mutex_lock(&writer->lock);
	weak = writer->self;
	g_mutex_unlock(&writer->lock);
	if (env && weak && (self = (*env)->NewLocalRef(env, weak))) {
		jclass class = (*env)->GetObjectClass(env, self);
		jmethodID method = (*env)->GetMethodID(env, class, "dispatchImageReleased", "()V");

		if (method)
			(*env)->CallVoidMethod(env, self, method);
		else
			(*env)->ExceptionClear(env);
		(*env)->DeleteLocalRef(env, class);
		(*env)->DeleteLocalRef(env, self);
	}
	atl_image_writer_unref(writer);
	return G_SOURCE_REMOVE;
}

void atl_image_writer_released(struct atl_image_writer *writer)
{
	bool notify;

	if (!writer)
		return;
	g_mutex_lock(&writer->lock);
	if (writer->outstanding > 0)
		writer->outstanding--;
	notify = writer->self != NULL;
	g_mutex_unlock(&writer->lock);
	if (!notify)
		return;
	g_atomic_int_inc(&writer->refcount);
	g_idle_add(deliver_image_released, writer);
}

void atl_image_writer_attach_surface(JNIEnv *env, struct atl_image_writer *writer, jobject surface)
{
	if (writer && surface)
		_SET_LONG_FIELD(surface, "imageWriterPtr", _INTPTR(writer));
}

struct atl_image_writer *atl_image_writer_from_surface(JNIEnv *env, jobject surface)
{
	struct atl_image_writer *writer;

	if (!surface)
		return NULL;
	writer = _PTR(_GET_LONG_FIELD(surface, "imageWriterPtr"));
	if (!writer)
		return NULL;
	g_atomic_int_inc(&writer->refcount);
	return writer;
}

void atl_image_writer_get_size(struct atl_image_writer *writer, int *width, int *height)
{
	*width = writer->width;
	*height = writer->height;
}

int atl_image_writer_get_format(struct atl_image_writer *writer)
{
	return writer->format;
}

struct atl_image_writer_frame *atl_image_writer_take(struct atl_image_writer *writer)
{
	struct atl_image_writer_frame *frame;

	if (!writer)
		return NULL;

	g_mutex_lock(&writer->lock);
	frame = writer->queue_head;
	if (frame) {
		writer->queue_head = frame->next;
		if (!writer->queue_head)
			writer->queue_tail = NULL;
		writer->queued--;
		writer->consumed++;
		frame->next = NULL;
	}
	g_mutex_unlock(&writer->lock);
	return frame;
}

void atl_image_writer_release(struct atl_image_writer *writer, struct atl_image_writer_frame *frame)
{
	if (!frame)
		return;
	if (!writer) {
		frame_free(frame);
		return;
	}
	g_mutex_lock(&writer->lock);
	if (writer->closed) {
		g_mutex_unlock(&writer->lock);
		frame_free(frame);
		return;
	}
	frame->next = writer->free_list;
	writer->free_list = frame;
	g_mutex_unlock(&writer->lock);
}

const uint8_t *atl_image_writer_frame_data(const struct atl_image_writer_frame *frame,
                                           int *width, int *height, int64_t *timestamp)
{
	if (!frame)
		return NULL;
	*width = frame->width;
	*height = frame->height;
	*timestamp = frame->timestamp;
	return frame->data;
}

/* call with the lock held: a recycled frame, or a fresh one */
static struct atl_image_writer_frame *frame_get_locked(struct atl_image_writer *writer)
{
	struct atl_image_writer_frame *frame = writer->free_list;

	if (frame) {
		writer->free_list = frame->next;
		frame->next = NULL;
		return frame;
	}
	return calloc(1, sizeof(*frame));
}

/* call with the lock held */
static void queue_locked(struct atl_image_writer *writer, struct atl_image_writer_frame *frame)
{
	frame->next = NULL;
	if (writer->queue_tail)
		writer->queue_tail->next = frame;
	else
		writer->queue_head = frame;
	writer->queue_tail = frame;
	writer->queued++;
	writer->written++;
}

JNIEXPORT jlong JNICALL Java_android_media_ImageWriter_native_1fromSurface(JNIEnv *env, jclass class,
                                                                            jobject surface, jobject self)
{
	struct atl_image_writer *writer = atl_image_writer_from_surface(env, surface);

	/* a real input tells the app when the camera gave a buffer back, which
	 * needs the ImageWriter itself. The queue outlives any one of them, so
	 * the newest is the one that hears: a stale weak ref is a dead object and
	 * the app that made the new writer would never be told. */
	if (writer && self) {
		jweak previous;

		(*env)->GetJavaVM(env, &writer->jvm);
		g_mutex_lock(&writer->lock);
		previous = writer->self;
		writer->self = (*env)->NewWeakGlobalRef(env, self);
		g_mutex_unlock(&writer->lock);
		if (previous)
			(*env)->DeleteWeakGlobalRef(env, previous);
	}
	return _INTPTR(writer);
}

/* {width, height, format, maxImages, real} */
JNIEXPORT jintArray JNICALL Java_android_media_ImageWriter_native_1info(JNIEnv *env, jclass class, jlong ptr)
{
	struct atl_image_writer *writer = _PTR(ptr);
	jintArray array;
	jint info[5];

	if (!writer)
		return NULL;
	info[0] = writer->width;
	info[1] = writer->height;
	info[2] = writer->format;
	info[3] = writer->max_images;
	info[4] = writer->real;

	array = (*env)->NewIntArray(env, 5);
	(*env)->SetIntArrayRegion(env, array, 0, 5, info);
	return array;
}

/* an empty buffer for the app to fill; 0 when maxImages are already out */
JNIEXPORT jlong JNICALL Java_android_media_ImageWriter_native_1dequeue(JNIEnv *env, jclass class, jlong ptr)
{
	struct atl_image_writer *writer = _PTR(ptr);
	struct atl_image_writer_frame *frame;
	size_t size;

	if (!writer)
		return 0;
	if (writer->real) {
		/* the camera fills its own buffers; the app hands one of those back */
		fprintf(stderr, "ImageWriter: a real reprocessing input has no buffers to hand out\n");
		return 0;
	}
	size = (size_t)writer->width * writer->height * 3 / 2;

	g_mutex_lock(&writer->lock);
	if (writer->closed || writer->queued + writer->dequeued >= writer->max_images) {
		g_mutex_unlock(&writer->lock);
		return 0;
	}
	frame = frame_get_locked(writer);
	if (!frame || !frame_reserve(frame, size)) {
		g_mutex_unlock(&writer->lock);
		frame_free(frame);
		return 0;
	}
	memset(frame->data, 0, size);
	memset(frame->data + (size_t)writer->width * writer->height, 128,
	       size - (size_t)writer->width * writer->height); /* neutral chroma */
	frame->size = size;
	frame->width = writer->width;
	frame->height = writer->height;
	frame->timestamp = 0;
	frame->writer = writer;
	writer->dequeued++;
	g_mutex_unlock(&writer->lock);
	return _INTPTR(frame);
}

/* the app filled a dequeued image: it goes on the queue */
JNIEXPORT jboolean JNICALL Java_android_media_ImageWriter_native_1queueOwn(JNIEnv *env, jclass class, jlong ptr,
                                                                           jlong frame_ptr, jlong timestamp)
{
	struct atl_image_writer *writer = _PTR(ptr);
	struct atl_image_writer_frame *frame = _PTR(frame_ptr);

	if (!writer || !frame)
		return JNI_FALSE;
	if (writer->real) {
		frame_free(frame);
		return JNI_FALSE;
	}

	g_mutex_lock(&writer->lock);
	writer->dequeued--;
	if (writer->closed) {
		g_mutex_unlock(&writer->lock);
		frame_free(frame);
		return JNI_FALSE;
	}
	frame->timestamp = timestamp;
	frame->writer = NULL;
	queue_locked(writer, frame);
	g_mutex_unlock(&writer->lock);
	return JNI_TRUE;
}

/*
 * The real input: the Image's own gralloc buffer goes to the camera, with no
 * copy, and stays out until the camera releases it. The app's Image is spent
 * either way - AOSP takes ownership of a queued image.
 */
static bool queue_to_sink(struct atl_image_writer *writer, struct atl_image *image)
{
	struct atl_camera_buffer *buffer;
	struct atl_image_writer_sink sink;
	bool queued;

	g_mutex_lock(&writer->lock);
	sink = writer->sink;
	if (writer->closed || !sink.queue || writer->outstanding >= writer->max_images) {
		g_mutex_unlock(&writer->lock);
		return false;
	}
	/* under the lock, so clear_sink cannot drop the last reference between
	 * reading the sink and calling into it */
	if (sink.ref)
		sink.ref(sink.user);
	writer->outstanding++;
	g_mutex_unlock(&writer->lock);

	buffer = atl_image_take_buffer(image);
	queued = buffer && sink.queue(sink.user, buffer);
	if (!queued) {
		atl_camera_buffer_release(buffer);
		g_mutex_lock(&writer->lock);
		writer->outstanding--;
		g_mutex_unlock(&writer->lock);
	} else {
		g_mutex_lock(&writer->lock);
		writer->written++;
		writer->consumed++;
		g_mutex_unlock(&writer->lock);
	}
	if (sink.unref)
		sink.unref(sink.user);
	return queued;
}

/* an Image from somewhere else (an ImageReader's PRIVATE output): its pixels
 * are copied, so the app can close the source image straight away */
JNIEXPORT jboolean JNICALL Java_android_media_ImageWriter_native_1queueForeign(JNIEnv *env, jclass class, jlong ptr,
                                                                               jlong image_ptr)
{
	struct atl_image_writer *writer = _PTR(ptr);
	struct atl_image_writer_frame *frame;
	const uint8_t *data;
	size_t size;
	int width, height, format;
	int64_t timestamp;

	if (!writer)
		return JNI_FALSE;
	if (writer->real)
		return queue_to_sink(writer, _PTR(image_ptr)) ? JNI_TRUE : JNI_FALSE;
	if (!atl_image_get_data(_PTR(image_ptr), &data, &size, &width, &height, &format, &timestamp))
		return JNI_FALSE;
	if (format != FORMAT_YUV_420_888 && format != FORMAT_PRIVATE) {
		fprintf(stderr, "ImageWriter: an image of format 0x%x cannot be reprocessed\n", format);
		return JNI_FALSE;
	}

	g_mutex_lock(&writer->lock);
	if (writer->closed || writer->queued + writer->dequeued >= writer->max_images) {
		g_mutex_unlock(&writer->lock);
		return JNI_FALSE;
	}
	frame = frame_get_locked(writer);
	if (!frame || !frame_reserve(frame, size)) {
		g_mutex_unlock(&writer->lock);
		frame_free(frame);
		return JNI_FALSE;
	}
	memcpy(frame->data, data, size);
	frame->size = size;
	frame->width = width;
	frame->height = height;
	frame->timestamp = timestamp;
	frame->writer = NULL;
	queue_locked(writer, frame);
	g_mutex_unlock(&writer->lock);
	return JNI_TRUE;
}

/* {rowStride, pixelStride, offset, size} of a dequeued image's plane */
JNIEXPORT jintArray JNICALL Java_android_media_ImageWriter_native_1planeInfo(JNIEnv *env, jclass class,
                                                                             jlong frame_ptr, jint plane)
{
	struct atl_image_writer_frame *frame = _PTR(frame_ptr);
	jintArray array;
	jint info[4];
	int luma;

	if (!frame || plane < 0 || plane >= MAX_PLANES)
		return NULL;
	luma = frame->width * frame->height;

	switch (plane) {
	case 0:
		info[0] = frame->width;
		info[1] = 1;
		info[2] = 0;
		info[3] = luma;
		break;
	case 1: /* NV21 chroma is V,U: U is the odd byte */
		info[0] = frame->width;
		info[1] = 2;
		info[2] = luma + 1;
		info[3] = luma / 2 - 1;
		break;
	default:
		info[0] = frame->width;
		info[1] = 2;
		info[2] = luma;
		info[3] = luma / 2;
		break;
	}
	array = (*env)->NewIntArray(env, 4);
	(*env)->SetIntArrayRegion(env, array, 0, 4, info);
	return array;
}

JNIEXPORT jobject JNICALL Java_android_media_ImageWriter_native_1planeBuffer(JNIEnv *env, jclass class,
                                                                             jlong frame_ptr, jint offset, jint size)
{
	struct atl_image_writer_frame *frame = _PTR(frame_ptr);

	if (!frame || offset < 0 || size < 0 || (size_t)(offset + size) > frame->size)
		return NULL;
	return (*env)->NewDirectByteBuffer(env, frame->data + offset, size);
}

/* a dequeued image the app abandoned rather than queued */
JNIEXPORT void JNICALL Java_android_media_ImageWriter_native_1cancel(JNIEnv *env, jclass class, jlong ptr,
                                                                     jlong frame_ptr)
{
	struct atl_image_writer *writer = _PTR(ptr);
	struct atl_image_writer_frame *frame = _PTR(frame_ptr);

	if (!frame)
		return;
	if (!writer) {
		frame_free(frame);
		return;
	}
	g_mutex_lock(&writer->lock);
	writer->dequeued--;
	g_mutex_unlock(&writer->lock);
	atl_image_writer_release(writer, frame);
}

JNIEXPORT void JNICALL Java_android_media_ImageWriter_native_1close(JNIEnv *env, jclass class, jlong ptr)
{
	struct atl_image_writer *writer = _PTR(ptr);

	if (!writer)
		return;
	g_mutex_lock(&writer->lock);
	writer->closed = true; /* the app was the only producer */
	fprintf(stderr, "ImageWriter: %dx%d format 0x%x closed after %" G_GUINT64_FORMAT
	                " images, %" G_GUINT64_FORMAT " reprocessed\n",
	        writer->width, writer->height, writer->format, writer->written, writer->consumed);
	g_mutex_unlock(&writer->lock);
	atl_image_writer_unref(writer);
}
