/*
 * Preview frames to an android.view.Surface, see camera_preview.h.
 *
 * The Java classes are looked up here rather than taken from handle_cache so
 * this works in the headless camera tests too (they load the library into a
 * plain dalvikvm, which never runs set_up_handle_cache()).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "../defines.h"

#include "camera_frame.h"
#include "camera_preview.h"

extern void *atl_video_frame_wrap_skbitmap(uint8_t *pixels, int width, int height);
extern void atl_video_frame_skbitmap_pixels_changed(void *skbitmap);

/* posted bitmaps are reused round-robin: enough of them that the scene is not
 * still drawing the one we are about to overwrite */
#define PREVIEW_BUFFERS 3

struct preview_buffer {
	jobject bitmap;  /* global ref to the android.graphics.Bitmap */
	uint8_t *pixels; /* its RGBA pixels, owned by the SkBitmap */
	void *skbitmap;
	int width;
	int height;
};

struct atl_camera_preview {
	JavaVM *jvm;

	GMutex lock;
	GCond cond;
	jobject surface; /* global ref, NULL when detached */

	/* the one frame waiting to be posted; a frame arriving while it is still
	 * unposted replaces it (drop-if-unconsumed, no queue) */
	uint8_t *pending; /* NV21, contiguous */
	size_t pending_capacity;
	bool pending_valid;
	int pending_width;
	int pending_height;
	uint8_t *scratch; /* swapped with pending by the poster thread */
	size_t scratch_capacity;

	bool running;
	GThread *thread;
	struct preview_buffer buffers[PREVIEW_BUFFERS];
	int next_buffer;

	uint64_t posted;
	uint64_t dropped;
};

static jclass bitmap_class;
static jmethodID bitmap_from_native;
static jclass surface_class;
static jmethodID surface_post_frame;

static bool ensure_java_refs(JNIEnv *env)
{
	if (bitmap_class && surface_class)
		return true;

	jclass bitmap = (*env)->FindClass(env, "android/graphics/Bitmap");
	jclass surface = (*env)->FindClass(env, "android/view/Surface");
	if (!bitmap || !surface) {
		fprintf(stderr, "Camera preview: android/graphics/Bitmap or android/view/Surface not found\n");
		(*env)->ExceptionClear(env);
		return false;
	}
	bitmap_from_native = (*env)->GetStaticMethodID(env, bitmap, "fromNative", "(J)Landroid/graphics/Bitmap;");
	surface_post_frame = (*env)->GetMethodID(env, surface, "postFrame", "(Landroid/graphics/Bitmap;)V");
	if (!bitmap_from_native || !surface_post_frame) {
		fprintf(stderr, "Camera preview: Bitmap.fromNative or Surface.postFrame not found\n");
		(*env)->ExceptionClear(env);
		return false;
	}
	bitmap_class = (*env)->NewGlobalRef(env, bitmap);
	surface_class = (*env)->NewGlobalRef(env, surface);
	return true;
}

static void buffer_release(struct preview_buffer *buffer, JNIEnv *env)
{
	if (buffer->bitmap)
		(*env)->DeleteGlobalRef(env, buffer->bitmap); /* the Bitmap owns the SkBitmap */
	memset(buffer, 0, sizeof(*buffer));
}

static bool buffer_prepare(struct preview_buffer *buffer, JNIEnv *env, int width, int height)
{
	if (buffer->bitmap && buffer->width == width && buffer->height == height)
		return true;

	buffer_release(buffer, env);

	uint8_t *pixels = malloc((size_t)width * height * 4);
	if (!pixels)
		return false;
	void *skbitmap = atl_video_frame_wrap_skbitmap(pixels, width, height); /* takes the pixels */
	if (!skbitmap)
		return false;

	jobject bitmap = (*env)->CallStaticObjectMethod(env, bitmap_class, bitmap_from_native, _INTPTR(skbitmap));
	if (!bitmap || (*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		return false;
	}
	buffer->bitmap = (*env)->NewGlobalRef(env, bitmap);
	(*env)->DeleteLocalRef(env, bitmap);
	buffer->pixels = pixels;
	buffer->skbitmap = skbitmap;
	buffer->width = width;
	buffer->height = height;
	return true;
}

static void post_frame(struct atl_camera_preview *preview, JNIEnv *env, jobject surface,
                       const uint8_t *nv21, int width, int height)
{
	struct preview_buffer *buffer = &preview->buffers[preview->next_buffer];

	if (!buffer_prepare(buffer, env, width, height))
		return;
	preview->next_buffer = (preview->next_buffer + 1) % PREVIEW_BUFFERS;

	atl_camera_nv21_to_rgba(nv21, width, height, width, buffer->pixels);
	atl_video_frame_skbitmap_pixels_changed(buffer->skbitmap);

	(*env)->CallVoidMethod(env, surface, surface_post_frame, buffer->bitmap);
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
	}
}

static gpointer preview_thread(gpointer user)
{
	struct atl_camera_preview *preview = user;
	JavaVMAttachArgs args = {JNI_VERSION_1_6, "atl-camera-preview", NULL};
	JNIEnv *env;

	if ((*preview->jvm)->AttachCurrentThreadAsDaemon(preview->jvm, (void **)&env, &args) != JNI_OK) {
		fprintf(stderr, "Camera preview: failed to attach the poster thread\n");
		return NULL;
	}

	for (;;) {
		g_mutex_lock(&preview->lock);
		while (preview->running && !preview->pending_valid)
			g_cond_wait(&preview->cond, &preview->lock);
		if (!preview->running) {
			g_mutex_unlock(&preview->lock);
			break;
		}
		/* take the frame by swapping buffers, so the backend can keep
		 * filling while we convert and hand this one to Java */
		uint8_t *nv21 = preview->pending;
		size_t capacity = preview->pending_capacity;
		int width = preview->pending_width;
		int height = preview->pending_height;
		jobject surface = preview->surface;
		preview->pending = preview->scratch;
		preview->pending_capacity = preview->scratch_capacity;
		preview->scratch = nv21;
		preview->scratch_capacity = capacity;
		preview->pending_valid = false;
		preview->posted++;
		g_mutex_unlock(&preview->lock);

		/* the surface global ref outlives this call: it is only dropped
		 * after the thread has been joined */
		post_frame(preview, env, surface, nv21, width, height);
	}

	for (int i = 0; i < PREVIEW_BUFFERS; i++)
		buffer_release(&preview->buffers[i], env);
	(*preview->jvm)->DetachCurrentThread(preview->jvm);
	return NULL;
}

struct atl_camera_preview *atl_camera_preview_new(JNIEnv *env)
{
	struct atl_camera_preview *preview = calloc(1, sizeof(*preview));

	(*env)->GetJavaVM(env, &preview->jvm);
	g_mutex_init(&preview->lock);
	g_cond_init(&preview->cond);
	return preview;
}

static void stop_thread(struct atl_camera_preview *preview)
{
	g_mutex_lock(&preview->lock);
	if (!preview->thread) {
		g_mutex_unlock(&preview->lock);
		return;
	}
	GThread *thread = preview->thread;
	preview->thread = NULL;
	preview->running = false;
	preview->pending_valid = false;
	g_cond_broadcast(&preview->cond);
	g_mutex_unlock(&preview->lock);

	g_thread_join(thread);
}

void atl_camera_preview_set_surface(struct atl_camera_preview *preview, JNIEnv *env, jobject surface)
{
	if (surface && !ensure_java_refs(env))
		surface = NULL;

	stop_thread(preview);

	g_mutex_lock(&preview->lock);
	if (preview->surface)
		(*env)->DeleteGlobalRef(env, preview->surface);
	preview->surface = surface ? (*env)->NewGlobalRef(env, surface) : NULL;
	if (preview->surface) {
		preview->running = true;
		preview->thread = g_thread_new("atl-camera-preview", preview_thread, preview);
	}
	g_mutex_unlock(&preview->lock);
}

void atl_camera_preview_submit(struct atl_camera_preview *preview, const uint8_t *nv21,
                               int width, int height, int stride)
{
	size_t size = (size_t)width * height * 3 / 2;

	g_mutex_lock(&preview->lock);
	if (!preview->running || !preview->surface) {
		g_mutex_unlock(&preview->lock);
		return;
	}
	if (preview->pending_capacity < size) {
		free(preview->pending);
		preview->pending = malloc(size);
		preview->pending_capacity = preview->pending ? size : 0;
	}
	if (!preview->pending) {
		g_mutex_unlock(&preview->lock);
		return;
	}
	if (stride == width) {
		memcpy(preview->pending, nv21, size);
	} else {
		for (int row = 0; row < height; row++)
			memcpy(preview->pending + (size_t)row * width, nv21 + (size_t)row * stride, width);
		for (int row = 0; row < height / 2; row++)
			memcpy(preview->pending + (size_t)width * height + (size_t)row * width,
			       nv21 + (size_t)stride * height + (size_t)row * stride, width);
	}
	if (preview->pending_valid) /* the poster thread is behind: keep the newest frame only */
		preview->dropped++;
	preview->pending_valid = true;
	preview->pending_width = width;
	preview->pending_height = height;
	g_cond_signal(&preview->cond);
	g_mutex_unlock(&preview->lock);
}

void atl_camera_preview_free(struct atl_camera_preview *preview, JNIEnv *env)
{
	stop_thread(preview);

	if (preview->posted || preview->dropped)
		fprintf(stderr, "Camera preview: %" G_GUINT64_FORMAT " frames posted, %" G_GUINT64_FORMAT " dropped\n",
		        preview->posted, preview->dropped);

	if (preview->surface)
		(*env)->DeleteGlobalRef(env, preview->surface);
	free(preview->pending);
	free(preview->scratch);
	g_cond_clear(&preview->cond);
	g_mutex_clear(&preview->lock);
	free(preview);
}
