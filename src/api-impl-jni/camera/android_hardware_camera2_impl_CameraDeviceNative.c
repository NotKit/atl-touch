/*
 * camera2 device and capture session on top of the camera backend, shaped like
 * AOSP's CameraDeviceClient: one open camera, a session of output streams -
 * one per app Surface, each in its own format and size - and requests that
 * name the streams they target. The stream engine (camera_streams.c) forwards
 * that to a backend with real streams or emulates it over a one-stream one;
 * this file turns Surfaces into stream configurations, routes each delivered
 * buffer to the consumer behind its Surface (an ImageReader queue, a
 * SurfaceTexture, a recording encoder), and posts the capture events to Java
 * on the main loop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <third_party/android-headers/camera/NdkCameraMetadataTags.h>

#include "../defines.h"

#include "../media/video_encoder.h"
#include "camera2_metadata.h"
#include "camera_backend.h"
#include "camera_frame.h"
#include "camera_streams.h"
#include "image_reader.h"
#include "image_writer.h"
#include "surface_texture.h"

#include "../generated_headers/android_hardware_camera2_impl_CameraDeviceNative.h"

/* Google Camera configures 7: a preview, a still and one raw stream per
 * physical camera */
#define MAX_OUTPUTS ATL_CAMERA_MAX_STREAMS

/* the zero-shutter-lag ring an app keeps on the reprocessing input */
#define MAX_INPUT_IMAGES 8

/* one configured output: exactly one of the three sinks */
struct atl_camera2_output {
	struct atl_surface_texture *texture;
	struct atl_image_reader *reader;
	struct atl_video_encoder *encoder;
	struct atl_camera_stream stream;
	char *physical_id;
	uint8_t *nv21; /* an encoder's frame, repacked from the buffer's planes */
	size_t nv21_capacity;
};

struct atl_camera2_device {
	gint refcount;
	const struct atl_camera_backend *backend;
	struct atl_camera *camera;
	struct atl_camera_streams *streams;
	JavaVM *jvm;

	GMutex lock;
	jweak self; /* weak: the app owns the CameraDevice's lifetime */
	bool shut_down;

	struct atl_camera2_output outputs[MAX_OUTPUTS];
	int n_outputs;
	/* the reprocessing input, when the session was created with one */
	struct atl_image_writer *input;
	/* and its stream, when the backend serves it as a HAL input of its own */
	bool has_input;
	struct atl_camera_stream_input input_config;

	/* one-shot ids handed out by the reprocessing path, which has frames of
	 * its own to number */
	int64_t reprocess_frame_number;

	/*
	 * Events posted to the main loop and not yet dispatched; atomic, because
	 * the frame thread reads it and the main loop clears it. Reported, not
	 * acted on: an app that has fallen behind the camera and one that is
	 * keeping up look identical from everywhere else.
	 */
	gint events_in_flight;
	uint64_t buffers; /* delivered to a consumer */
	uint64_t buffers_dropped;
};

enum event_type {
	EVENT_STARTED,
	EVENT_COMPLETED,
	EVENT_FAILED,
	EVENT_BUFFER_LOST,
	EVENT_ERROR,
};

struct capture_event {
	struct atl_camera2_device *device; /* holds a reference */
	enum event_type type;
	int request_id;
	int error;
	int stream;
	int64_t frame_number;
	int64_t timestamp;
	struct atl_camera_metadata *result; /* EVENT_COMPLETED; Java takes it */
};

static jmethodID dispatch_started;
static jmethodID dispatch_completed;
static jmethodID dispatch_failed;
static jmethodID dispatch_buffer_lost;
static jmethodID dispatch_error;

static bool ensure_java_refs(JNIEnv *env, jobject self)
{
	if (dispatch_started)
		return true;

	jclass class = (*env)->GetObjectClass(env, self);
	dispatch_started = _METHOD(class, "dispatchCaptureStarted", "(IJJ)V");
	dispatch_completed = _METHOD(class, "dispatchCaptureCompleted", "(IJJ)V");
	dispatch_failed = _METHOD(class, "dispatchCaptureFailed", "(IJ)V");
	dispatch_buffer_lost = _METHOD(class, "dispatchCaptureBufferLost", "(IJI)V");
	dispatch_error = _METHOD(class, "dispatchError", "(I)V");
	(*env)->DeleteLocalRef(env, class);

	if (!dispatch_started || !dispatch_completed || !dispatch_failed || !dispatch_buffer_lost ||
	    !dispatch_error) {
		fprintf(stderr, "camera2: CameraDeviceNative.dispatch* methods not found\n");
		(*env)->ExceptionClear(env);
		dispatch_started = NULL;
		return false;
	}
	return true;
}

static void device_unref(struct atl_camera2_device *device)
{
	if (!g_atomic_int_dec_and_test(&device->refcount))
		return;
	g_mutex_clear(&device->lock);
	free(device);
}

static JNIEnv *get_env(struct atl_camera2_device *device)
{
	JNIEnv *env;

	if ((*device->jvm)->GetEnv(device->jvm, (void **)&env, JNI_VERSION_1_6) == JNI_OK)
		return env;

	JavaVMAttachArgs args = {JNI_VERSION_1_6, "atl-camera2", NULL};
	if ((*device->jvm)->AttachCurrentThreadAsDaemon(device->jvm, (void **)&env, &args) != JNI_OK) {
		fprintf(stderr, "camera2: failed to attach the delivery thread\n");
		return NULL;
	}
	return env;
}

static gboolean deliver_event(gpointer user)
{
	struct capture_event *event = user;
	struct atl_camera2_device *device = event->device;
	JNIEnv *env = get_env(device);

	if (!env)
		return G_SOURCE_REMOVE;

	/* the weak ref must be read under the lock that close() clears it with */
	g_mutex_lock(&device->lock);
	jobject self = device->shut_down ? NULL : (*env)->NewLocalRef(env, device->self);
	g_mutex_unlock(&device->lock);
	if (!self)
		return G_SOURCE_REMOVE;

	switch (event->type) {
	case EVENT_STARTED:
		(*env)->CallVoidMethod(env, self, dispatch_started, event->request_id,
		                       (jlong)event->frame_number, (jlong)event->timestamp);
		break;
	case EVENT_COMPLETED:
		(*env)->CallVoidMethod(env, self, dispatch_completed, event->request_id,
		                       (jlong)event->frame_number, _INTPTR(event->result));
		event->result = NULL; /* Java owns the bag from here on */
		break;
	case EVENT_FAILED:
		(*env)->CallVoidMethod(env, self, dispatch_failed, event->request_id,
		                       (jlong)event->frame_number);
		break;
	case EVENT_BUFFER_LOST:
		(*env)->CallVoidMethod(env, self, dispatch_buffer_lost, event->request_id,
		                       (jlong)event->frame_number, event->stream);
		break;
	case EVENT_ERROR:
		(*env)->CallVoidMethod(env, self, dispatch_error, event->error);
		break;
	}
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
	}
	(*env)->DeleteLocalRef(env, self);
	return G_SOURCE_REMOVE;
}

static void free_event(gpointer user)
{
	struct capture_event *event = user;

	/* undelivered results are ours to free */
	atl_camera_metadata_free(event->result);
	g_atomic_int_add(&event->device->events_in_flight, -1);
	device_unref(event->device);
	free(event);
}

static void post_event(struct atl_camera2_device *device, struct capture_event *event)
{
	g_mutex_lock(&device->lock);
	bool shut_down = device->shut_down;
	g_mutex_unlock(&device->lock);
	if (shut_down) {
		atl_camera_metadata_free(event->result);
		free(event);
		return;
	}

	event->device = device;
	g_atomic_int_inc(&device->refcount);
	g_atomic_int_inc(&device->events_in_flight);
	/*
	 * Above the image callbacks, which are idle-priority: a result is what
	 * lets an app use the image of the same frame, so it has to lead. When
	 * the main loop stalls both queues back up, and an app that drains with
	 * acquireLatestImage then takes the newest image against the oldest
	 * result it has - Google Camera matches the two by timestamp, finds the
	 * image newer than every request it is holding, and distributes null.
	 */
	g_idle_add_full(G_PRIORITY_DEFAULT, deliver_event, event, free_event);
}

/* --- the stream engine's callbacks (backend threads) --------------------- */

static void on_started(void *user, int request_id, int64_t frame_number, int64_t timestamp)
{
	struct atl_camera2_device *device = user;
	struct capture_event *event = calloc(1, sizeof(*event));

	event->type = EVENT_STARTED;
	event->request_id = request_id;
	event->frame_number = frame_number;
	event->timestamp = timestamp;
	post_event(device, event);
}

static void on_result(void *user, int request_id, int64_t frame_number,
                      struct atl_camera_metadata *result)
{
	struct atl_camera2_device *device = user;
	struct capture_event *event = calloc(1, sizeof(*event));

	event->type = EVENT_COMPLETED;
	event->request_id = request_id;
	event->frame_number = frame_number;
	event->result = result;
	post_event(device, event);
}

static void on_failed(void *user, int request_id, int64_t frame_number)
{
	struct atl_camera2_device *device = user;
	struct capture_event *event = calloc(1, sizeof(*event));

	event->type = EVENT_FAILED;
	event->request_id = request_id;
	event->frame_number = frame_number;
	post_event(device, event);
}

static void on_buffer_lost(void *user, int request_id, int64_t frame_number, int stream)
{
	struct atl_camera2_device *device = user;
	struct capture_event *event = calloc(1, sizeof(*event));

	event->type = EVENT_BUFFER_LOST;
	event->request_id = request_id;
	event->frame_number = frame_number;
	event->stream = stream;
	post_event(device, event);
}

/* a buffer's planes as the NV21 a recording encoder takes; call with the
 * lock held, the result lives in the output until the next frame */
static const uint8_t *buffer_to_nv21_locked(struct atl_camera2_output *output,
                                            const struct atl_camera_buffer *buffer, int *width,
                                            int *height)
{
	struct atl_camera_yuv420 src;
	size_t size;

	if (buffer->n_planes < 3 || buffer->format != ATL_CAMERA_FORMAT_YUV_420_888)
		return NULL;
	*width = buffer->width & ~1;
	*height = buffer->height & ~1;
	if (*width < 2 || *height < 2)
		return NULL;
	/* an emulated buffer is NV21 already */
	if (buffer->size)
		return buffer->planes[0].data;

	size = (size_t)*width * *height * 3 / 2;
	if (output->nv21_capacity < size) {
		free(output->nv21);
		output->nv21 = malloc(size);
		output->nv21_capacity = output->nv21 ? size : 0;
	}
	if (!output->nv21)
		return NULL;
	src = (struct atl_camera_yuv420){
		.y = buffer->planes[0].data, .u = buffer->planes[1].data, .v = buffer->planes[2].data,
		.y_stride = buffer->planes[0].row_stride, .u_stride = buffer->planes[1].row_stride,
		.v_stride = buffer->planes[2].row_stride,
		.u_pixel = buffer->planes[1].pixel_stride, .v_pixel = buffer->planes[2].pixel_stride,
		.y_len = buffer->planes[0].len, .u_len = buffer->planes[1].len, .v_len = buffer->planes[2].len,
	};
	atl_camera_yuv420_to_nv21(output->nv21, *width, *height, &src, 1);
	return output->nv21;
}

/* one filled buffer of one stream, to the consumer behind that Surface */
static void on_buffer(void *user, struct atl_camera_buffer *buffer)
{
	struct atl_camera2_device *device = user;
	struct atl_camera2_output *output;
	struct atl_surface_texture *texture = NULL;
	struct atl_image_reader *reader = NULL;
	struct atl_video_encoder *encoder = NULL;

	g_mutex_lock(&device->lock);
	if (buffer->stream < 0 || buffer->stream >= device->n_outputs) {
		device->buffers_dropped++;
		g_mutex_unlock(&device->lock);
		atl_camera_buffer_release(buffer);
		return;
	}
	device->buffers++;
	output = &device->outputs[buffer->stream];
	texture = output->texture;
	reader = output->reader;
	encoder = output->encoder;
	atl_surface_texture_ref(texture);
	atl_image_reader_ref(reader);
	atl_video_encoder_ref(encoder);
	if (encoder) {
		int width, height;
		const uint8_t *nv21 = buffer_to_nv21_locked(output, buffer, &width, &height);

		if (nv21)
			atl_video_encoder_submit(encoder, nv21, width, height, width, buffer->timestamp);
	}
	g_mutex_unlock(&device->lock);

	if (reader)
		atl_image_reader_submit(reader, buffer); /* the reader owns it now */
	else if (texture)
		atl_surface_texture_submit_buffer(texture, buffer);
	else
		atl_camera_buffer_release(buffer);

	atl_surface_texture_unref(texture);
	atl_image_reader_unref(reader);
	atl_video_encoder_unref(encoder);
}

static const struct atl_camera_stream_callbacks stream_callbacks = {
	.started = on_started,
	.result = on_result,
	.failed = on_failed,
	.buffer = on_buffer,
	.buffer_lost = on_buffer_lost,
};

static void on_error(int error, void *user)
{
	struct atl_camera2_device *device = user;
	struct capture_event *event = calloc(1, sizeof(*event));

	event->type = EVENT_ERROR;
	event->error = error;
	post_event(device, event);
}

/* --- the reprocessing input on real streams ------------------------------- */

/* one buffer the app queued, out until the camera gives it back */
struct input_buffer {
	struct atl_camera2_device *device;
	struct atl_image_writer *writer;
};

static void device_ref_cb(void *user)
{
	g_atomic_int_inc(&((struct atl_camera2_device *)user)->refcount);
}

static void device_unref_cb(void *user)
{
	device_unref(user);
}

static void on_input_released(void *user, struct atl_camera_buffer *buffer)
{
	struct input_buffer *held = user;

	atl_camera_buffer_release(buffer);
	/* the app hears it can write another only now: the camera had the buffer
	 * until this call, which is what makes the input a queue of real depth */
	atl_image_writer_released(held->writer);
	atl_image_writer_unref(held->writer);
	device_unref(held->device);
	free(held);
}

static bool device_queue_input(void *user, struct atl_camera_buffer *buffer)
{
	struct atl_camera2_device *device = user;
	struct atl_camera_streams *streams;
	struct input_buffer *held;
	struct atl_image_writer *writer;

	g_mutex_lock(&device->lock);
	writer = device->input;
	streams = device->streams;
	atl_image_writer_ref(writer);
	g_mutex_unlock(&device->lock);
	if (!writer || !streams) {
		atl_image_writer_unref(writer);
		return false;
	}

	held = calloc(1, sizeof(*held));
	held->device = device;
	held->writer = writer;
	g_atomic_int_inc(&device->refcount);
	if (atl_camera_streams_queue_input(streams, buffer, on_input_released, held))
		return true;
	device_unref(device);
	atl_image_writer_unref(writer);
	free(held);
	return false;
}

/* the input goes with the session that had it: an ordinary session configured
 * on the same device must not be given a reprocessing input stream */
JNIEXPORT void JNICALL Java_android_hardware_camera2_impl_CameraDeviceNative_native_1clearInputSurface(JNIEnv *env, jclass class,
                                                                                                       jlong device_ptr)
{
	struct atl_camera2_device *device = _PTR(device_ptr);
	struct atl_image_writer *previous;

	if (!device)
		return;
	g_mutex_lock(&device->lock);
	previous = device->input;
	device->input = NULL;
	device->has_input = false;
	memset(&device->input_config, 0, sizeof(device->input_config));
	g_mutex_unlock(&device->lock);
	if (previous) {
		atl_image_writer_clear_sink(previous);
		atl_image_writer_unref(previous);
	}
}

/* --- outputs -------------------------------------------------------------- */

static void output_clear(struct atl_camera2_output *output)
{
	atl_surface_texture_unref(output->texture);
	atl_image_reader_unref(output->reader);
	atl_video_encoder_unref(output->encoder);
	free(output->physical_id);
	free(output->nv21);
	memset(output, 0, sizeof(*output));
}

/*
 * One output: a SurfaceTexture, an ImageReader queue or a recording encoder,
 * and the stream that feeds it. An ImageReader says exactly what it wants. A
 * SurfaceTexture takes YUV at its default buffer size, or the backend's own
 * preview size without one, sampled by the GPU where the buffers can be bound
 * as they are and read by the CPU where they cannot. An encoder takes YUV at
 * its own size, read by the CPU.
 */
static bool output_from_surface(JNIEnv *env, struct atl_camera2_device *device, jobject surface,
                                jstring physical_id, struct atl_camera2_output *output)
{
	int default_width = 0, default_height = 0;
	const struct atl_camera_caps *caps;
	struct atl_camera_stream *stream = &output->stream;

	memset(output, 0, sizeof(*output));
	if (!surface)
		return false;
	if (physical_id) {
		const char *id = _CSTRING(physical_id);

		output->physical_id = id && *id ? strdup(id) : NULL;
		(*env)->ReleaseStringUTFChars(env, physical_id, id);
	}

	output->reader = atl_image_reader_from_surface(env, surface);
	if (output->reader) {
		atl_image_reader_get_stream(output->reader, stream);
		goto done;
	}

	output->encoder = atl_video_encoder_from_surface(env, surface);
	if (output->encoder) {
		atl_video_encoder_get_size(output->encoder, &stream->width, &stream->height);
		stream->format = ATL_CAMERA_FORMAT_YUV_420_888;
		stream->max_buffers = 3;
		stream->usage = ATL_CAMERA_USAGE_CPU_READ_OFTEN;
		goto done;
	}

	jobject surface_texture = _GET_OBJ_FIELD(surface, "surfaceTexture", "Landroid/graphics/SurfaceTexture;");
	if (!surface_texture) {
		fprintf(stderr, "camera2: output surface is backed by neither a SurfaceTexture nor an ImageReader\n");
		output_clear(output);
		return false;
	}
	output->texture = atl_surface_texture_from_java(env, surface_texture);
	(*env)->DeleteLocalRef(env, surface_texture);
	if (!output->texture) {
		output_clear(output);
		return false;
	}

	caps = device->backend->get_caps(device->camera);
	if (!caps || caps->n_preview_sizes < 1) {
		output_clear(output);
		return false;
	}
	stream->width = caps->preview_sizes[0].width;
	stream->height = caps->preview_sizes[0].height;
	atl_surface_texture_get_default_size(output->texture, &default_width, &default_height);
	if (default_width > 0 && default_height > 0) {
		stream->width = default_width;
		stream->height = default_height;
	}
	stream->format = ATL_CAMERA_FORMAT_YUV_420_888;
	stream->max_buffers = 3;
	stream->usage = ATL_CAMERA_USAGE_GPU_SAMPLED_IMAGE | ATL_CAMERA_USAGE_CPU_READ_OFTEN;
done:
	stream->physical_id = output->physical_id;
	return true;
}

/* the camera2 ids are the backend's own; a Camera1-style index opens it */
static int camera_index_of(const struct atl_camera_backend *backend, const char *id)
{
	const char *const *ids;
	int count = 0;

	if (!backend->get_camera2_id_list)
		return -1;
	ids = backend->get_camera2_id_list(&count);
	for (int i = 0; i < count; i++)
		if (!strcmp(ids[i], id))
			return atoi(id);
	return -1;
}

JNIEXPORT jlong JNICALL Java_android_hardware_camera2_impl_CameraDeviceNative_native_1open(JNIEnv *env, jclass class,
                                                                                           jstring id_str, jobject self)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();
	struct atl_camera2_device *device;
	struct atl_camera *camera;
	const char *id;
	int index;

	if (!backend || !id_str || !self || !ensure_java_refs(env, self))
		return 0;

	id = _CSTRING(id_str);
	index = camera_index_of(backend, id);
	(*env)->ReleaseStringUTFChars(env, id_str, id);
	if (index < 0)
		return 0;

	camera = backend->open(index);
	if (!camera)
		return 0;

	device = calloc(1, sizeof(*device));
	device->refcount = 1;
	device->backend = backend;
	device->camera = camera;
	g_mutex_init(&device->lock);
	(*env)->GetJavaVM(env, &device->jvm);
	device->self = (*env)->NewWeakGlobalRef(env, self);
	device->streams = atl_camera_streams_new(backend, camera, &stream_callbacks, device);

	backend->set_error_callback(camera, on_error, device);
	return _INTPTR(device);
}

/*
 * Configure the session: one stream per Surface. The answer is the size of
 * the largest stream, which is what the Java side reports as the session's.
 */
JNIEXPORT jintArray JNICALL Java_android_hardware_camera2_impl_CameraDeviceNative_native_1configure(JNIEnv *env, jclass class,
                                                                                                    jlong device_ptr, jobjectArray surfaces,
                                                                                                    jobjectArray physical_ids)
{
	struct atl_camera2_device *device = _PTR(device_ptr);
	struct atl_camera2_output outputs[MAX_OUTPUTS] = {0}, previous[MAX_OUTPUTS];
	struct atl_camera_stream configs[MAX_OUTPUTS];
	int n_outputs, n_previous;
	jintArray size_array;
	jint size[2] = {0, 0};

	if (!device || !surfaces)
		return NULL;

	n_outputs = (*env)->GetArrayLength(env, surfaces);
	if (n_outputs < 1 || n_outputs > MAX_OUTPUTS) {
		fprintf(stderr, "camera2: %d outputs is not something this backend can configure\n", n_outputs);
		return NULL;
	}

	for (int i = 0; i < n_outputs; i++) {
		jobject surface = (*env)->GetObjectArrayElement(env, surfaces, i);
		jstring physical_id = physical_ids && i < (*env)->GetArrayLength(env, physical_ids)
		                          ? (*env)->GetObjectArrayElement(env, physical_ids, i)
		                          : NULL;
		bool ok = output_from_surface(env, device, surface, physical_id, &outputs[i]);

		(*env)->DeleteLocalRef(env, surface);
		if (physical_id)
			(*env)->DeleteLocalRef(env, physical_id);
		if (!ok) {
			for (int j = 0; j < i; j++)
				output_clear(&outputs[j]);
			return NULL;
		}
		configs[i] = outputs[i].stream;
		if (configs[i].width * configs[i].height > size[0] * size[1]) {
			size[0] = configs[i].width;
			size[1] = configs[i].height;
		}
	}

	/* the old outputs go once no buffer of theirs can arrive any more: the
	 * engine tears the old session down before it builds the new one */
	g_mutex_lock(&device->lock);
	memcpy(previous, device->outputs, sizeof(previous));
	n_previous = device->n_outputs;
	memcpy(device->outputs, outputs, sizeof(device->outputs));
	device->n_outputs = n_outputs;
	g_mutex_unlock(&device->lock);

	if (!atl_camera_streams_configure(device->streams, configs, n_outputs,
	                                  device->has_input ? &device->input_config : NULL)) {
		g_mutex_lock(&device->lock);
		memset(device->outputs, 0, sizeof(device->outputs));
		device->n_outputs = 0;
		g_mutex_unlock(&device->lock);
		for (int i = 0; i < n_outputs; i++)
			output_clear(&outputs[i]);
		for (int i = 0; i < n_previous; i++)
			output_clear(&previous[i]);
		return NULL;
	}
	for (int i = 0; i < n_previous; i++)
		output_clear(&previous[i]);

	fprintf(stderr, "camera2: session configured (%d output(s)%s, %s streams, largest %dx%d)\n",
	        n_outputs, device->has_input ? " and a reprocessing input" : "",
	        atl_camera_streams_are_real(device->streams) ? "HAL" : "emulated", size[0], size[1]);
	if (device->has_input)
		fprintf(stderr, "camera2:   input: %dx%d format 0x%x%s\n", device->input_config.width,
		        device->input_config.height, device->input_config.format,
		        device->input_config.multi_resolution ? " multi-resolution" : "");
	for (int i = 0; i < n_outputs; i++)
		fprintf(stderr, "camera2:   stream %d: %dx%d format 0x%x x%d%s%s -> %s\n", i,
		        configs[i].width, configs[i].height, configs[i].format, configs[i].max_buffers,
		        configs[i].physical_id ? " physical " : "",
		        configs[i].physical_id ? configs[i].physical_id : "",
		        outputs[i].reader ? "ImageReader" : outputs[i].texture ? "SurfaceTexture" : "encoder");

	size_array = (*env)->NewIntArray(env, 2);
	(*env)->SetIntArrayRegion(env, size_array, 0, 2, size);
	return size_array;
}

/*
 * The reprocessing input, hung off a Surface of its own. On the emulated
 * streams it is a queue an ImageWriter pushes NV21 into, and a reprocess takes
 * one through the outputs as if the camera had produced it. On real streams it
 * is a HAL input stream: the app hands back a buffer the camera itself filled,
 * nothing is copied, and the whole session has to be configured with it - so
 * the configuration is only recorded here and native_configure carries it.
 */
JNIEXPORT jboolean JNICALL Java_android_hardware_camera2_impl_CameraDeviceNative_native_1createInputSurface(JNIEnv *env, jclass class,
                                                                                                            jlong device_ptr, jobject surface,
                                                                                                            jint width, jint height, jint format,
                                                                                                            jboolean multi_resolution)
{
	struct atl_camera2_device *device = _PTR(device_ptr);
	struct atl_image_writer *writer, *previous;

	if (!device || !surface)
		return JNI_FALSE;

	if (atl_camera_streams_would_be_real(device->streams)) {
		struct atl_image_writer_sink sink = {
			.queue = device_queue_input,
			.user = device,
			.ref = device_ref_cb,
			.unref = device_unref_cb,
		};

		if (!device->backend->queue_input) {
			fprintf(stderr, "camera2: backend '%s' has no reprocessing input; set "
			                "ATL_CAMERA2_STREAMS=0 to emulate the whole session instead\n",
			        device->backend->name);
			return JNI_FALSE;
		}
		g_atomic_int_inc(&device->refcount); /* the sink's own reference */
		writer = atl_image_writer_new_input(width, height, format, MAX_INPUT_IMAGES, &sink);
		if (!writer) {
			device_unref(device);
			return JNI_FALSE;
		}
	} else {
		writer = atl_image_writer_new(width, height, format, MAX_INPUT_IMAGES);
		if (!writer)
			return JNI_FALSE;
	}
	atl_image_writer_attach_surface(env, writer, surface);

	g_mutex_lock(&device->lock);
	previous = device->input;
	device->input = writer;
	device->has_input = atl_image_writer_is_real(writer);
	device->input_config = (struct atl_camera_stream_input){
		.width = width,
		.height = height,
		.format = format,
		/* a multi-resolution input's size is its first stream's, which is
		 * what the app's InputConfiguration already reduced it to */
		.multi_resolution = multi_resolution == JNI_TRUE,
		.max_buffers = MAX_INPUT_IMAGES,
	};
	g_mutex_unlock(&device->lock);
	if (previous) {
		atl_image_writer_clear_sink(previous);
		atl_image_writer_unref(previous);
	}
	return JNI_TRUE;
}

/*
 * One reprocess capture. On real streams the camera already holds the buffer
 * the app queued, and the request is built from the result of the frame it
 * came from - which the app named by putting that whole result in the
 * request's settings, sensor timestamp and all. On the emulated streams the
 * oldest queued image goes through the targeted outputs instead, converted the
 * way a frame would be.
 */
JNIEXPORT jboolean JNICALL Java_android_hardware_camera2_impl_CameraDeviceNative_native_1reprocess(JNIEnv *env, jclass class,
                                                                                                   jlong device_ptr, jint request_id,
                                                                                                   jlong request_ptr, jint targets)
{
	struct atl_camera2_device *device = _PTR(device_ptr);
	struct atl_camera_metadata *request = _PTR(request_ptr);
	struct atl_image_writer_frame *frame;
	struct atl_image_writer *input;
	const uint8_t *nv21;
	int width, height;
	int64_t timestamp;
	bool ok;

	if (!device)
		return JNI_FALSE;

	if (atl_camera_streams_can_reprocess(device->streams)) {
		const struct atl_camera_metadata_entry *entry =
		    atl_camera_metadata_find(request, ACAMERA_SENSOR_TIMESTAMP);

		if (!entry || entry->type != ATL_CAMERA2_TYPE_INT64 || entry->count < 1) {
			fprintf(stderr, "camera2: a reprocess request with no sensor timestamp names no "
			                "frame to reprocess\n");
			return JNI_FALSE;
		}
		return atl_camera_streams_submit_reprocess(device->streams, request_id, request,
		                                           (uint32_t)targets,
		                                           ((const int64_t *)entry->data)[0])
		           ? JNI_TRUE : JNI_FALSE;
	}

	g_mutex_lock(&device->lock);
	input = device->input;
	atl_image_writer_ref(input);
	g_mutex_unlock(&device->lock);

	frame = atl_image_writer_take(input);
	if (!frame) {
		fprintf(stderr, "camera2: a reprocess capture with nothing queued on the input\n");
		atl_image_writer_unref(input);
		return JNI_FALSE;
	}
	nv21 = atl_image_writer_frame_data(frame, &width, &height, &timestamp);
	ok = atl_camera_streams_reprocess(device->streams, request_id, request, (uint32_t)targets,
	                                  nv21, width, height, timestamp);
	atl_image_writer_release(input, frame);
	atl_image_writer_unref(input);
	return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_android_hardware_camera2_impl_CameraDeviceNative_native_1setRepeatingRequest(JNIEnv *env, jclass class,
                                                                                                             jlong device_ptr, jint request_id,
                                                                                                             jlong request_ptr, jint targets)
{
	struct atl_camera2_device *device = _PTR(device_ptr);
	struct atl_camera_metadata *request = _PTR(request_ptr);

	if (!device || !device->n_outputs)
		return JNI_FALSE;
	return atl_camera_streams_submit(device->streams, request_id, request, (uint32_t)targets, true)
	           ? JNI_TRUE : JNI_FALSE;
}

/* one frame, queued behind the one-shots before it: a burst is several */
JNIEXPORT jboolean JNICALL Java_android_hardware_camera2_impl_CameraDeviceNative_native_1capture(JNIEnv *env, jclass class,
                                                                                                  jlong device_ptr, jint request_id,
                                                                                                  jlong request_ptr, jint targets)
{
	struct atl_camera2_device *device = _PTR(device_ptr);
	struct atl_camera_metadata *request = _PTR(request_ptr);

	if (!device || !device->n_outputs)
		return JNI_FALSE;
	return atl_camera_streams_submit(device->streams, request_id, request, (uint32_t)targets, false)
	           ? JNI_TRUE : JNI_FALSE;
}

/*
 * Drop the one-shot captures that have not completed. Their ids come back so
 * the session can tell the app those requests failed, which is the only way
 * an aborted capture is ever visible: the frames they were waiting for are
 * gone.
 */
JNIEXPORT jintArray JNICALL Java_android_hardware_camera2_impl_CameraDeviceNative_native_1abortCaptures(JNIEnv *env, jclass class,
                                                                                                        jlong device_ptr)
{
	struct atl_camera2_device *device = _PTR(device_ptr);
	jintArray array;
	jint ids[64];
	int count;

	if (!device)
		return NULL;
	count = atl_camera_streams_flush(device->streams, ids, G_N_ELEMENTS(ids));
	array = (*env)->NewIntArray(env, count);
	if (count)
		(*env)->SetIntArrayRegion(env, array, 0, count, ids);
	return array;
}

JNIEXPORT void JNICALL Java_android_hardware_camera2_impl_CameraDeviceNative_native_1stopRepeating(JNIEnv *env, jclass class, jlong device_ptr)
{
	struct atl_camera2_device *device = _PTR(device_ptr);

	if (device)
		atl_camera_streams_cancel_repeating(device->streams);
}

JNIEXPORT void JNICALL Java_android_hardware_camera2_impl_CameraDeviceNative_native_1close(JNIEnv *env, jclass class, jlong device_ptr)
{
	struct atl_camera2_device *device = _PTR(device_ptr);
	struct atl_camera2_output outputs[MAX_OUTPUTS];
	struct atl_camera_streams *streams;
	struct atl_image_writer *input;
	int n_outputs;

	if (!device)
		return;

	/* the session first, then the camera: nothing can reach the outputs once
	 * the engine has torn its streams down. The pointer goes under the lock
	 * before the engine does, so a queue from the app thread sees NULL rather
	 * than an engine being freed. */
	g_mutex_lock(&device->lock);
	streams = device->streams;
	device->streams = NULL;
	g_mutex_unlock(&device->lock);
	atl_camera_streams_free(streams);
	device->backend->set_error_callback(device->camera, NULL, NULL);
	device->backend->close(device->camera);

	g_mutex_lock(&device->lock);
	device->shut_down = true;
	if (device->self)
		(*env)->DeleteWeakGlobalRef(env, device->self);
	device->self = NULL;
	memcpy(outputs, device->outputs, sizeof(outputs));
	n_outputs = device->n_outputs;
	memset(device->outputs, 0, sizeof(device->outputs));
	device->n_outputs = 0;
	input = device->input;
	device->input = NULL;
	if (device->buffers || device->buffers_dropped)
		fprintf(stderr, "camera2: closed after %" G_GUINT64_FORMAT " buffers delivered, %"
		        G_GUINT64_FORMAT " dropped\n", device->buffers, device->buffers_dropped);
	g_mutex_unlock(&device->lock);

	for (int i = 0; i < n_outputs; i++)
		output_clear(&outputs[i]);
	/* the sink holds a reference on the device: without this the two keep
	 * each other alive for as long as the app holds the input Surface */
	atl_image_writer_clear_sink(input);
	atl_image_writer_unref(input);
	device_unref(device);
}
