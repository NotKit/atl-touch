/*
 * android.media.ImageReader and its Images, see image_reader.h.
 *
 * The reader never converts anything: the stream that feeds it is in its
 * format already, real (a HAL stream) or emulated (camera_streams.c). What is
 * here is the queue, the callback accounting and the Image the app reads.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "../defines.h"

#include "camera_streams.h"
#include "hardware_buffer.h"
#include "image_reader.h"

#include "../generated_headers/android_media_ImageReader.h"

/* android.hardware.HardwareBuffer */
#define HB_FORMAT_BLOB          0x21
#define HB_FORMAT_YCBCR_420_888 0x23

struct atl_image {
	struct atl_image *next;          /* queue link */
	struct atl_image_reader *reader; /* held while the app has the image */
	struct atl_camera_buffer *buffer;
	struct atl_hardware_buffer *hw;  /* created on demand, at most once */
};

struct atl_image_reader {
	gint refcount;
	JavaVM *jvm;
	jweak self; /* weak: the app owns the ImageReader's lifetime */

	GMutex lock;

	bool closed;
	bool listener_enabled;
	bool idle_queued;

	int width;
	int height;
	int format;
	int max_images;
	uint64_t usage;

	struct atl_image *queue_head;
	struct atl_image *queue_tail;
	int queued;
	int promised; /* callbacks dispatched that no acquire has answered yet */
	int acquired;

	uint64_t submitted;
	uint64_t dropped;
};

static jmethodID dispatch_image_available;

static void post_image_available_locked(struct atl_image_reader *reader);

/* ATL_DEBUG_IMAGEREADER: the queue accounting behind every callback decision */
static bool reader_debug(void)
{
	static int on = -1;

	if (on < 0)
		on = g_getenv("ATL_DEBUG_IMAGEREADER") != NULL;
	return on;
}

/* call with the lock held */
static void reader_debug_locked(struct atl_image_reader *reader, const char *what)
{
	if (!reader_debug())
		return;
	fprintf(stderr, "ImageReader %dx%d fmt 0x%x: %s - queued %d promised %d acquired %d"
	        " max %d, %" G_GUINT64_FORMAT " submitted %" G_GUINT64_FORMAT " dropped\n",
	        reader->width, reader->height, reader->format, what, reader->queued, reader->promised,
	        reader->acquired, reader->max_images, reader->submitted, reader->dropped);
}

/* the image and its buffer go, a HardwareBuffer the app kept stops pointing at
 * the recycled pixels first */
static void image_free(struct atl_image *image)
{
	if (!image)
		return;
	if (image->hw) {
		atl_hardware_buffer_detach(image->hw);
		atl_hardware_buffer_unref(image->hw);
	}
	atl_camera_buffer_release(image->buffer);
	free(image);
}

static void reader_unref(struct atl_image_reader *reader)
{
	struct atl_image *image;

	if (!g_atomic_int_dec_and_test(&reader->refcount))
		return;

	while ((image = reader->queue_head)) {
		reader->queue_head = image->next;
		image_free(image);
	}
	g_mutex_clear(&reader->lock);
	free(reader);
}

void atl_image_reader_ref(struct atl_image_reader *reader)
{
	if (reader)
		g_atomic_int_inc(&reader->refcount);
}

void atl_image_reader_unref(struct atl_image_reader *reader)
{
	if (reader)
		reader_unref(reader);
}

struct atl_image_reader *atl_image_reader_from_surface(JNIEnv *env, jobject surface)
{
	struct atl_image_reader *reader;

	if (!surface)
		return NULL;
	reader = _PTR(_GET_LONG_FIELD(surface, "imageReaderPtr"));
	if (!reader)
		return NULL;
	g_atomic_int_inc(&reader->refcount);
	return reader;
}

void atl_image_reader_get_stream(struct atl_image_reader *reader, struct atl_camera_stream *stream)
{
	memset(stream, 0, sizeof(*stream));
	stream->width = reader->width;
	stream->height = reader->height;
	stream->format = reader->format;
	stream->max_buffers = reader->max_images;
	/* readable on the CPU whatever the app said about usage: the planes of
	 * anything but an opaque image are read there, and an opaque one is what
	 * an app posts to its viewfinder, which ATL composites on the CPU */
	stream->usage = reader->usage | ATL_CAMERA_USAGE_CPU_READ_OFTEN;
}

int atl_image_reader_get_format(struct atl_image_reader *reader)
{
	return reader->format;
}

static JNIEnv *get_env(struct atl_image_reader *reader)
{
	JNIEnv *env;

	if ((*reader->jvm)->GetEnv(reader->jvm, (void **)&env, JNI_VERSION_1_6) == JNI_OK)
		return env;

	JavaVMAttachArgs args = {JNI_VERSION_1_6, "atl-image-reader", NULL};
	if ((*reader->jvm)->AttachCurrentThreadAsDaemon(reader->jvm, (void **)&env, &args) != JNI_OK) {
		fprintf(stderr, "ImageReader: failed to attach the delivery thread\n");
		return NULL;
	}
	return env;
}

/* onImageAvailable is app code: it runs on the dispatch thread, never on the
 * producer's */
static gboolean deliver_image_available(gpointer user)
{
	struct atl_image_reader *reader = user;
	JNIEnv *env = get_env(reader);

	if (!env)
		return G_SOURCE_REMOVE;

	g_mutex_lock(&reader->lock);
	reader->idle_queued = false;
	jobject self = NULL;
	/*
	 * One callback per image, and never more callbacks than there are images
	 * to take: an app counts them and acquires once for each. Promising more
	 * hands it a null from an acquire it was promised; promising fewer - which
	 * is what coalescing several submits into one idle callback did - leaves
	 * it acquiring the newest image against an older result forever, because
	 * acquireLatestImage recycles the ones it was never told about.
	 */
	reader_debug_locked(reader, "dispatch");
	if (!reader->closed && reader->listener_enabled && reader->queued > reader->promised) {
		reader->promised++;
		self = (*env)->NewLocalRef(env, reader->self);
	}
	g_mutex_unlock(&reader->lock);

	if (self) {
		(*env)->CallVoidMethod(env, self, dispatch_image_available);
		if ((*env)->ExceptionCheck(env)) {
			(*env)->ExceptionDescribe(env);
			(*env)->ExceptionClear(env);
		}
		(*env)->DeleteLocalRef(env, self); /* nothing frees local refs outside a JNI call */
	}

	g_mutex_lock(&reader->lock);
	post_image_available_locked(reader); /* the images this one did not cover */
	g_mutex_unlock(&reader->lock);
	return G_SOURCE_REMOVE;
}

/*
 * The callbacks are dispatched from a thread of their own rather than the UI
 * main loop, which is what Android does and what the accounting above needs to
 * mean anything: a stalled main loop used to queue a dozen frames before the
 * first callback went out, and an app that drains with acquireLatestImage then
 * took an image a dozen frames ahead of the capture results it had been given.
 * It is still never the producer's thread, so a slow listener cannot stall the
 * camera.
 */
static GMainContext *dispatch_context;

static gpointer dispatch_thread(gpointer loop)
{
	g_main_context_push_thread_default(dispatch_context);
	g_main_loop_run(loop);
	return NULL;
}

static GMainContext *dispatch_context_get(void)
{
	static gsize once = 0;

	if (g_once_init_enter(&once)) {
		dispatch_context = g_main_context_new();
		g_thread_unref(g_thread_new("atl-image-reader", dispatch_thread,
		                            g_main_loop_new(dispatch_context, FALSE)));
		g_once_init_leave(&once, 1);
	}
	return dispatch_context;
}

/* call with the lock held */
static void post_image_available_locked(struct atl_image_reader *reader)
{
	GSource *source;

	if (!reader->listener_enabled || reader->idle_queued || reader->closed)
		return;
	if (reader->queued <= reader->promised)
		return;
	reader_debug_locked(reader, "post");
	reader->idle_queued = true;
	g_atomic_int_inc(&reader->refcount);

	source = g_idle_source_new();
	g_source_set_priority(source, G_PRIORITY_DEFAULT);
	g_source_set_callback(source, deliver_image_available, reader, (GDestroyNotify)reader_unref);
	g_source_attach(source, dispatch_context_get());
	g_source_unref(source);
}

/* call with the lock held: a promise is only good while its image is queued */
static void promises_settle_locked(struct atl_image_reader *reader)
{
	if (reader->promised > reader->queued)
		reader->promised = reader->queued;
}

void atl_image_reader_submit(struct atl_image_reader *reader, struct atl_camera_buffer *buffer)
{
	struct atl_image *image;

	if (!reader || !buffer)
		return;

	g_mutex_lock(&reader->lock);
	reader->submitted++;
	if (reader->closed || reader->queued + reader->acquired >= reader->max_images) {
		reader->dropped++;
		reader_debug_locked(reader, "submit dropped, no budget");
		g_mutex_unlock(&reader->lock);
		atl_camera_buffer_release(buffer);
		return;
	}
	image = calloc(1, sizeof(*image));
	image->buffer = buffer;
	if (reader->queue_tail)
		reader->queue_tail->next = image;
	else
		reader->queue_head = image;
	reader->queue_tail = image;
	reader->queued++;
	post_image_available_locked(reader);
	g_mutex_unlock(&reader->lock);
}

bool atl_image_get_data(struct atl_image *image, const uint8_t **data, size_t *size,
                        int *width, int *height, int *format, int64_t *timestamp)
{
	struct atl_camera_buffer *buffer = image ? image->buffer : NULL;

	if (!buffer || !buffer->size || !buffer->planes[0].data)
		return false;
	*data = buffer->planes[0].data;
	*size = buffer->size;
	*width = buffer->width;
	*height = buffer->height;
	*format = buffer->format;
	*timestamp = buffer->timestamp;
	return true;
}

struct atl_camera_buffer *atl_image_take_buffer(struct atl_image *image)
{
	struct atl_camera_buffer *buffer;

	if (!image || !image->buffer)
		return NULL;
	/* a HardwareBuffer the app made of this image stops pointing at the
	 * pixels: they belong to whoever took the buffer from here on */
	if (image->hw)
		atl_hardware_buffer_detach(image->hw);
	buffer = image->buffer;
	image->buffer = NULL;
	return buffer;
}

/* call with the lock held */
static struct atl_image *dequeue_locked(struct atl_image_reader *reader)
{
	struct atl_image *image = reader->queue_head;

	if (!image)
		return NULL;
	reader->queue_head = image->next;
	if (!reader->queue_head)
		reader->queue_tail = NULL;
	reader->queued--;
	image->next = NULL;
	return image;
}

JNIEXPORT jlong JNICALL Java_android_media_ImageReader_native_1create(JNIEnv *env, jclass class, jint width,
                                                                      jint height, jint format, jint max_images,
                                                                      jlong usage, jobject self)
{
	struct atl_image_reader *reader;

	if (format != ATL_CAMERA_FORMAT_YUV_420_888 && format != ATL_CAMERA_FORMAT_JPEG &&
	    format != ATL_CAMERA_FORMAT_PRIVATE && format != ATL_CAMERA_FORMAT_RAW10 &&
	    format != ATL_CAMERA_FORMAT_RAW_SENSOR && format != ATL_CAMERA_FORMAT_RAW12) {
		fprintf(stderr, "ImageReader: unsupported format 0x%x\n", format);
		return 0;
	}
	if (!dispatch_image_available) {
		jclass reader_class = (*env)->GetObjectClass(env, self);

		dispatch_image_available = _METHOD(reader_class, "dispatchImageAvailable", "()V");
		(*env)->DeleteLocalRef(env, reader_class);
		if (!dispatch_image_available) {
			fprintf(stderr, "ImageReader: ImageReader.dispatchImageAvailable not found\n");
			(*env)->ExceptionClear(env);
			return 0;
		}
	}

	reader = calloc(1, sizeof(*reader));
	reader->refcount = 1;
	reader->width = width;
	reader->height = height;
	reader->format = format;
	reader->max_images = max_images;
	reader->usage = (uint64_t)usage;
	g_mutex_init(&reader->lock);
	(*env)->GetJavaVM(env, &reader->jvm);
	reader->self = (*env)->NewWeakGlobalRef(env, self);
	return _INTPTR(reader);
}

JNIEXPORT void JNICALL Java_android_media_ImageReader_native_1attachSurface(JNIEnv *env, jclass class, jlong ptr,
                                                                            jobject surface)
{
	if (ptr && surface)
		_SET_LONG_FIELD(surface, "imageReaderPtr", ptr);
}

JNIEXPORT void JNICALL Java_android_media_ImageReader_native_1setListenerEnabled(JNIEnv *env, jclass class, jlong ptr,
                                                                                  jboolean enabled)
{
	struct atl_image_reader *reader = _PTR(ptr);

	if (!reader)
		return;
	g_mutex_lock(&reader->lock);
	reader->listener_enabled = enabled;
	/* an image queued before the listener was set still deserves a callback */
	if (enabled && reader->queued)
		post_image_available_locked(reader);
	g_mutex_unlock(&reader->lock);
}

JNIEXPORT jboolean JNICALL Java_android_media_ImageReader_native_1hasQueuedImage(JNIEnv *env, jclass class,
                                                                                 jlong ptr)
{
	struct atl_image_reader *reader = _PTR(ptr);
	bool queued;

	if (!reader)
		return JNI_FALSE;
	g_mutex_lock(&reader->lock);
	queued = !reader->closed && reader->queued > 0;
	/* nothing to take after all: give the promise back, or no further image
	 * would ever be announced */
	reader_debug_locked(reader, queued ? "claim" : "claim found nothing");
	if (!queued && reader->promised > 0)
		reader->promised--;
	g_mutex_unlock(&reader->lock);
	return queued ? JNI_TRUE : JNI_FALSE;
}

/* 0: nothing queued; -1: the app is already holding maxImages */
JNIEXPORT jlong JNICALL Java_android_media_ImageReader_native_1acquire(JNIEnv *env, jclass class, jlong ptr,
                                                                       jboolean latest)
{
	struct atl_image_reader *reader = _PTR(ptr);
	struct atl_image *image = NULL, *skipped = NULL;

	if (!reader)
		return 0;

	g_mutex_lock(&reader->lock);
	if (reader->acquired >= reader->max_images) {
		g_mutex_unlock(&reader->lock);
		return -1;
	}
	if (latest) {
		/* the older images go back to the producer, outside the lock */
		while (reader->queued > 1) {
			struct atl_image *old = dequeue_locked(reader);

			old->next = skipped;
			skipped = old;
		}
	}
	image = dequeue_locked(reader);
	if (image) {
		reader->acquired++;
		image->reader = reader;
		g_atomic_int_inc(&reader->refcount);
		if (reader->promised > 0)
			reader->promised--; /* this acquire is the answer to one callback */
	}
	promises_settle_locked(reader);
	post_image_available_locked(reader); /* a latest-acquire may have freed the queue */
	g_mutex_unlock(&reader->lock);

	while (skipped) {
		struct atl_image *old = skipped;

		skipped = old->next;
		image_free(old);
	}
	return _INTPTR(image);
}

JNIEXPORT void JNICALL Java_android_media_ImageReader_native_1discardFreeBuffers(JNIEnv *env, jclass class, jlong ptr)
{
	/* the buffers are the producer's: there is nothing of the reader's to
	 * give back */
	(void)env;
	(void)class;
	(void)ptr;
}

JNIEXPORT void JNICALL Java_android_media_ImageReader_native_1close(JNIEnv *env, jclass class, jlong ptr)
{
	struct atl_image_reader *reader = _PTR(ptr);
	struct atl_image *queued = NULL;

	if (!reader)
		return;

	g_mutex_lock(&reader->lock);
	reader->closed = true;
	reader->listener_enabled = false;
	if (reader->self)
		(*env)->DeleteWeakGlobalRef(env, reader->self);
	reader->self = NULL;
	queued = reader->queue_head;
	reader->queue_head = reader->queue_tail = NULL;
	reader->queued = 0;
	fprintf(stderr, "ImageReader: %dx%d format 0x%x closed after %" G_GUINT64_FORMAT
	                " frames, %" G_GUINT64_FORMAT " dropped\n",
	        reader->width, reader->height, reader->format, reader->submitted, reader->dropped);
	g_mutex_unlock(&reader->lock);

	while (queued) {
		struct atl_image *image = queued;

		queued = image->next;
		image_free(image);
	}
	reader_unref(reader);
}

/* {width, height, format, planeCount, timestamp} */
JNIEXPORT jlongArray JNICALL Java_android_media_ImageReader_native_1imageInfo(JNIEnv *env, jclass class, jlong ptr)
{
	struct atl_image *image = _PTR(ptr);
	jlongArray array;
	jlong info[5];

	if (!image || !image->buffer)
		return NULL;
	info[0] = image->buffer->width;
	info[1] = image->buffer->height;
	info[2] = image->buffer->format;
	info[3] = image->buffer->n_planes;
	info[4] = image->buffer->timestamp;

	array = (*env)->NewLongArray(env, 5);
	(*env)->SetLongArrayRegion(env, array, 0, 5, info);
	return array;
}

JNIEXPORT jintArray JNICALL Java_android_media_ImageReader_native_1planeStrides(JNIEnv *env, jclass class, jlong ptr,
                                                                                jint plane)
{
	struct atl_image *image = _PTR(ptr);
	jintArray array;
	jint strides[2];

	if (!image || !image->buffer || plane < 0 || plane >= image->buffer->n_planes)
		return NULL;
	strides[0] = image->buffer->planes[plane].row_stride;
	strides[1] = image->buffer->planes[plane].pixel_stride;

	array = (*env)->NewIntArray(env, 2);
	(*env)->SetIntArrayRegion(env, array, 0, 2, strides);
	return array;
}

JNIEXPORT jobject JNICALL Java_android_media_ImageReader_native_1planeBuffer(JNIEnv *env, jclass class, jlong ptr,
                                                                             jint plane)
{
	struct atl_image *image = _PTR(ptr);
	const struct atl_camera_plane *p;

	if (!image || !image->buffer || plane < 0 || plane >= image->buffer->n_planes)
		return NULL;
	p = &image->buffer->planes[plane];
	if (!p->data || p->len <= 0)
		return NULL;
	return (*env)->NewDirectByteBuffer(env, p->data, p->len);
}

JNIEXPORT jobject JNICALL Java_android_media_ImageReader_native_1imageHardwareBuffer(JNIEnv *env, jclass class, jlong ptr)
{
	struct atl_image *image = _PTR(ptr);
	struct atl_camera_buffer *buffer;

	if (!image || !image->buffer)
		return NULL;
	buffer = image->buffer;
	if (!image->hw) {
		uint64_t usage = image->reader ? image->reader->usage : 0;

		if (buffer->native) {
			/* a HAL buffer: the app gets the gralloc buffer the camera wrote
			 * into, as it does on Android, and samples it with no copy. Its
			 * planes, where the image has them mapped, are what ATL presents
			 * from: the image holds the buffer locked, and gralloc does not
			 * lock a buffer twice */
			struct atl_window_frame mapped = {0};
			bool have_planes = buffer->n_planes >= 3 &&
			                   buffer->format == ATL_CAMERA_FORMAT_YUV_420_888;

			for (int i = 0; have_planes && i < 3; i++)
				mapped.planes[i] = (struct atl_window_plane){
				    buffer->planes[i].data, (size_t)buffer->planes[i].row_stride,
				    (size_t)buffer->planes[i].pixel_stride,
				};
			image->hw = atl_hardware_buffer_wrap_native(buffer->native, buffer->width,
			                                            buffer->height, buffer->format, usage,
			                                            have_planes ? &mapped : NULL);
		} else if (buffer->size && buffer->planes[0].data) {
			/* only YUV has a HardwareBuffer format of its own; a JPEG and a
			 * packed raw plane are both a run of bytes */
			bool blob = buffer->format != ATL_CAMERA_FORMAT_YUV_420_888 &&
			            buffer->format != ATL_CAMERA_FORMAT_PRIVATE;
			int format = blob ? HB_FORMAT_BLOB : HB_FORMAT_YCBCR_420_888;
			int width = blob ? (int)buffer->size : buffer->width;
			int height = blob ? 1 : buffer->height;

			image->hw = atl_hardware_buffer_wrap(buffer->planes[0].data, buffer->size, width,
			                                     height, format, usage);
		}
	}
	return image->hw ? atl_hardware_buffer_to_java(env, image->hw) : NULL;
}

JNIEXPORT void JNICALL Java_android_media_ImageReader_native_1imageClose(JNIEnv *env, jclass class, jlong ptr)
{
	struct atl_image *image = _PTR(ptr);
	struct atl_image_reader *reader;

	if (!image)
		return;
	reader = image->reader;
	image->reader = NULL;
	image_free(image);
	if (!reader)
		return;
	g_mutex_lock(&reader->lock);
	reader->acquired--;
	g_mutex_unlock(&reader->lock);
	reader_unref(reader);
}
