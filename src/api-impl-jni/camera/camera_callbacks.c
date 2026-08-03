/*
 * Camera.PreviewCallback delivery, see camera_callbacks.h.
 *
 * Unlike the Surface preview path (camera_preview.c, own poster thread), these
 * callbacks are app code that expects the main thread, so they go through
 * g_idle_add like the SensorManager listeners. The struct is refcounted
 * because a queued idle call can outlive Camera.release().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "../defines.h"

#include "camera_callbacks.h"
#include "camera_frame.h"

struct atl_camera_callbacks {
	gint refcount;
	JavaVM *jvm;

	GMutex lock;
	jweak camera;     /* weak: the Camera must stay collectable */
	jobject callback; /* global ref to the Camera.PreviewCallback, NULL when off */
	int mode;
	GQueue buffers;   /* global refs to byte[], oldest first (with-buffer mode) */
	bool shut_down;

	/* the one frame waiting to be delivered; a newer frame replaces it */
	uint8_t *pending;
	size_t pending_capacity;
	bool pending_valid;
	int pending_width;
	int pending_height;
	uint8_t *scratch; /* swapped with pending on delivery */
	size_t scratch_capacity;

	bool idle_queued;
	uint64_t delivered;
	uint64_t dropped;
};

static jmethodID on_preview_frame;

static bool ensure_java_refs(JNIEnv *env)
{
	if (on_preview_frame)
		return true;

	jclass callback_class = (*env)->FindClass(env, "android/hardware/Camera$PreviewCallback");
	if (callback_class)
		on_preview_frame = (*env)->GetMethodID(env, callback_class, "onPreviewFrame",
		                                       "([BLandroid/hardware/Camera;)V");
	if (!on_preview_frame) {
		fprintf(stderr, "Camera callbacks: Camera$PreviewCallback.onPreviewFrame not found\n");
		(*env)->ExceptionClear(env);
		return false;
	}
	return true;
}

static void callbacks_unref(struct atl_camera_callbacks *callbacks)
{
	if (!g_atomic_int_dec_and_test(&callbacks->refcount))
		return;

	free(callbacks->pending);
	free(callbacks->scratch);
	g_mutex_clear(&callbacks->lock);
	free(callbacks);
}

/* takes ownership of the global ref, dropping delivery state with it */
static void clear_callback_locked(struct atl_camera_callbacks *callbacks, JNIEnv *env)
{
	if (callbacks->callback)
		(*env)->DeleteGlobalRef(env, callbacks->callback);
	callbacks->callback = NULL;
	callbacks->mode = ATL_CAMERA_CB_NONE;
	callbacks->pending_valid = false;
}

static JNIEnv *get_env(struct atl_camera_callbacks *callbacks)
{
	JNIEnv *env;

	if ((*callbacks->jvm)->GetEnv(callbacks->jvm, (void **)&env, JNI_VERSION_1_6) == JNI_OK)
		return env;

	/* the main loop thread is normally the Java main thread, but it can be
	 * a thread of its own */
	JavaVMAttachArgs args = {JNI_VERSION_1_6, "atl-camera-callbacks", NULL};
	if ((*callbacks->jvm)->AttachCurrentThreadAsDaemon(callbacks->jvm, (void **)&env, &args) != JNI_OK) {
		fprintf(stderr, "Camera callbacks: failed to attach the delivery thread\n");
		return NULL;
	}
	return env;
}

static gboolean deliver_frame(gpointer user)
{
	struct atl_camera_callbacks *callbacks = user;
	JNIEnv *env = get_env(callbacks);

	if (!env)
		return G_SOURCE_REMOVE;

	g_mutex_lock(&callbacks->lock);
	callbacks->idle_queued = false;
	if (callbacks->shut_down || !callbacks->pending_valid || !callbacks->callback) {
		g_mutex_unlock(&callbacks->lock);
		return G_SOURCE_REMOVE;
	}

	/* take the frame by swapping buffers, so the backend can keep filling
	 * while the app runs its callback */
	uint8_t *nv21 = callbacks->pending;
	size_t capacity = callbacks->pending_capacity;
	int width = callbacks->pending_width;
	int height = callbacks->pending_height;
	callbacks->pending = callbacks->scratch;
	callbacks->pending_capacity = callbacks->scratch_capacity;
	callbacks->scratch = nv21;
	callbacks->scratch_capacity = capacity;
	callbacks->pending_valid = false;

	/* hold our own references: the app may unregister from the callback */
	jobject callback = (*env)->NewGlobalRef(env, callbacks->callback);
	jobject camera = (*env)->NewLocalRef(env, callbacks->camera);
	jbyteArray buffer = NULL;
	if (callbacks->mode == ATL_CAMERA_CB_WITH_BUFFER)
		buffer = g_queue_pop_head(&callbacks->buffers); /* owns the global ref now */
	if (callbacks->mode == ATL_CAMERA_CB_ONE_SHOT)
		clear_callback_locked(callbacks, env);
	callbacks->delivered++;
	g_mutex_unlock(&callbacks->lock);

	size_t size = (size_t)width * height * 3 / 2;
	jbyteArray data = buffer;

	if (data && (size_t)(*env)->GetArrayLength(env, data) < size) {
		fprintf(stderr, "Camera callbacks: callback buffer too small (%d < %zu), dropping frame\n",
		        (*env)->GetArrayLength(env, data), size);
		(*env)->DeleteGlobalRef(env, data);
		data = NULL;
		size = 0;
	} else if (!data) {
		data = (*env)->NewByteArray(env, size); /* no buffer queue: a fresh array per frame, AOSP-style */
	}

	if (data && camera) {
		(*env)->SetByteArrayRegion(env, data, 0, size, (const jbyte *)nv21);
		(*env)->CallVoidMethod(env, callback, on_preview_frame, data, camera);
		if ((*env)->ExceptionCheck(env)) {
			(*env)->ExceptionDescribe(env);
			(*env)->ExceptionClear(env);
		}
	}

	/* the buffer keeps its identity for the app: it got the same array back
	 * and re-adds it (taking a fresh global ref) from the callback */
	if (buffer && data)
		(*env)->DeleteGlobalRef(env, data);
	else if (data)
		(*env)->DeleteLocalRef(env, data);
	if (camera)
		(*env)->DeleteLocalRef(env, camera);
	(*env)->DeleteGlobalRef(env, callback);
	return G_SOURCE_REMOVE;
}

struct atl_camera_callbacks *atl_camera_callbacks_new(JNIEnv *env, jobject camera)
{
	struct atl_camera_callbacks *callbacks = calloc(1, sizeof(*callbacks));

	callbacks->refcount = 1;
	(*env)->GetJavaVM(env, &callbacks->jvm);
	g_mutex_init(&callbacks->lock);
	g_queue_init(&callbacks->buffers);
	callbacks->camera = (*env)->NewWeakGlobalRef(env, camera);
	return callbacks;
}

void atl_camera_callbacks_set(struct atl_camera_callbacks *callbacks, JNIEnv *env,
                              jobject callback, int mode)
{
	if (callback && !ensure_java_refs(env)) {
		callback = NULL;
		mode = ATL_CAMERA_CB_NONE;
	}

	g_mutex_lock(&callbacks->lock);
	clear_callback_locked(callbacks, env);
	if (callback && mode != ATL_CAMERA_CB_NONE) {
		callbacks->callback = (*env)->NewGlobalRef(env, callback);
		callbacks->mode = mode;
	}
	g_mutex_unlock(&callbacks->lock);
}

void atl_camera_callbacks_add_buffer(struct atl_camera_callbacks *callbacks, JNIEnv *env,
                                     jbyteArray buffer)
{
	if (!buffer)
		return;

	g_mutex_lock(&callbacks->lock);
	g_queue_push_tail(&callbacks->buffers, (*env)->NewGlobalRef(env, buffer));
	g_mutex_unlock(&callbacks->lock);
}

void atl_camera_callbacks_submit(struct atl_camera_callbacks *callbacks, const uint8_t *nv21,
                                 int width, int height, int stride)
{
	size_t size = (size_t)width * height * 3 / 2;

	g_mutex_lock(&callbacks->lock);
	if (!callbacks->callback || callbacks->shut_down)
		goto out;
	/* with-buffer mode delivers nothing until the app queues a buffer */
	if (callbacks->mode == ATL_CAMERA_CB_WITH_BUFFER && g_queue_is_empty(&callbacks->buffers)) {
		callbacks->dropped++;
		goto out;
	}

	if (callbacks->pending_capacity < size) {
		free(callbacks->pending);
		callbacks->pending = malloc(size);
		callbacks->pending_capacity = callbacks->pending ? size : 0;
	}
	if (!callbacks->pending)
		goto out;

	atl_camera_nv21_pack(callbacks->pending, nv21, width, height, stride);
	if (callbacks->pending_valid) /* the main loop is behind: keep the newest frame only */
		callbacks->dropped++;
	callbacks->pending_valid = true;
	callbacks->pending_width = width;
	callbacks->pending_height = height;

	if (!callbacks->idle_queued) {
		callbacks->idle_queued = true;
		g_atomic_int_inc(&callbacks->refcount);
		g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, deliver_frame, callbacks,
		                (GDestroyNotify)callbacks_unref);
	}
out:
	g_mutex_unlock(&callbacks->lock);
}

void atl_camera_callbacks_free(struct atl_camera_callbacks *callbacks, JNIEnv *env)
{
	g_mutex_lock(&callbacks->lock);
	callbacks->shut_down = true;
	clear_callback_locked(callbacks, env);
	if (callbacks->camera)
		(*env)->DeleteWeakGlobalRef(env, callbacks->camera);
	callbacks->camera = NULL;
	for (jobject buffer; (buffer = g_queue_pop_head(&callbacks->buffers));)
		(*env)->DeleteGlobalRef(env, buffer);
	if (callbacks->delivered || callbacks->dropped)
		fprintf(stderr, "Camera callbacks: %" G_GUINT64_FORMAT " frames delivered, %"
		        G_GUINT64_FORMAT " dropped\n", callbacks->delivered, callbacks->dropped);
	g_mutex_unlock(&callbacks->lock);

	/* a queued idle call still holds a reference; it sees shut_down and
	 * only drops it */
	callbacks_unref(callbacks);
}
