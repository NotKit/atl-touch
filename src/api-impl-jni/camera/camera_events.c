/*
 * Shutter / picture / autofocus / error delivery, see camera_events.h.
 *
 * The backend raises these from its own threads (capture worker, gst bus), so
 * every event is queued with g_idle_add and runs on the main loop like the
 * preview callbacks. The struct is refcounted: a queued event can outlive
 * Camera.release().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "camera_events.h"

enum event_type {
	EVENT_SHUTTER,
	EVENT_PICTURE,
	EVENT_AUTOFOCUS,
	EVENT_ERROR,
};

struct atl_camera_events {
	gint refcount;
	JavaVM *jvm;

	GMutex lock;
	jweak camera; /* weak: the Camera must stay collectable */
	bool shut_down;
};

struct camera_event {
	struct atl_camera_events *events; /* holds a reference */
	enum event_type type;
	bool success;
	int error;
	uint8_t *data; /* EVENT_PICTURE: the JPEG, or NULL */
	size_t size;
};

static jmethodID dispatch_shutter;
static jmethodID dispatch_picture;
static jmethodID dispatch_autofocus;
static jmethodID dispatch_error;

static bool ensure_java_refs(JNIEnv *env, jobject camera)
{
	if (dispatch_shutter)
		return true;

	jclass camera_class = (*env)->GetObjectClass(env, camera);
	dispatch_shutter = (*env)->GetMethodID(env, camera_class, "dispatchShutter", "()V");
	dispatch_picture = (*env)->GetMethodID(env, camera_class, "dispatchPictureTaken", "([B)V");
	dispatch_autofocus = (*env)->GetMethodID(env, camera_class, "dispatchAutoFocus", "(Z)V");
	dispatch_error = (*env)->GetMethodID(env, camera_class, "dispatchError", "(I)V");
	(*env)->DeleteLocalRef(env, camera_class);

	if (!dispatch_shutter || !dispatch_picture || !dispatch_autofocus || !dispatch_error) {
		fprintf(stderr, "Camera events: Camera.dispatch* methods not found\n");
		(*env)->ExceptionClear(env);
		dispatch_shutter = NULL;
		return false;
	}
	return true;
}

static void events_unref(struct atl_camera_events *events)
{
	if (!g_atomic_int_dec_and_test(&events->refcount))
		return;

	g_mutex_clear(&events->lock);
	free(events);
}

static JNIEnv *get_env(struct atl_camera_events *events)
{
	JNIEnv *env;

	if ((*events->jvm)->GetEnv(events->jvm, (void **)&env, JNI_VERSION_1_6) == JNI_OK)
		return env;

	/* the main loop thread is normally the Java main thread, but it can be
	 * a thread of its own */
	JavaVMAttachArgs args = {JNI_VERSION_1_6, "atl-camera-events", NULL};
	if ((*events->jvm)->AttachCurrentThreadAsDaemon(events->jvm, (void **)&env, &args) != JNI_OK) {
		fprintf(stderr, "Camera events: failed to attach the delivery thread\n");
		return NULL;
	}
	return env;
}

static gboolean deliver_event(gpointer user)
{
	struct camera_event *event = user;
	struct atl_camera_events *events = event->events;
	JNIEnv *env = get_env(events);

	if (!env)
		return G_SOURCE_REMOVE;

	/* the weak ref must be read under the lock that free() clears it with */
	g_mutex_lock(&events->lock);
	jobject camera = events->shut_down ? NULL : (*env)->NewLocalRef(env, events->camera);
	g_mutex_unlock(&events->lock);
	if (!camera)
		return G_SOURCE_REMOVE;

	switch (event->type) {
	case EVENT_SHUTTER:
		(*env)->CallVoidMethod(env, camera, dispatch_shutter);
		break;
	case EVENT_PICTURE: {
		jbyteArray data = NULL;
		if (event->data) {
			data = (*env)->NewByteArray(env, event->size);
			if (data)
				(*env)->SetByteArrayRegion(env, data, 0, event->size, (const jbyte *)event->data);
		}
		(*env)->CallVoidMethod(env, camera, dispatch_picture, data);
		if (data)
			(*env)->DeleteLocalRef(env, data);
		break;
	}
	case EVENT_AUTOFOCUS:
		(*env)->CallVoidMethod(env, camera, dispatch_autofocus, (jboolean)event->success);
		break;
	case EVENT_ERROR:
		(*env)->CallVoidMethod(env, camera, dispatch_error, event->error);
		break;
	}
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
	}
	(*env)->DeleteLocalRef(env, camera);
	return G_SOURCE_REMOVE;
}

static void free_event(gpointer user)
{
	struct camera_event *event = user;

	events_unref(event->events);
	free(event->data);
	free(event);
}

static void post_event(struct atl_camera_events *events, struct camera_event *event)
{
	g_mutex_lock(&events->lock);
	bool shut_down = events->shut_down;
	g_mutex_unlock(&events->lock);
	if (shut_down) {
		free(event->data);
		free(event);
		return;
	}

	event->events = events;
	g_atomic_int_inc(&events->refcount);
	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, deliver_event, event, free_event);
}

struct atl_camera_events *atl_camera_events_new(JNIEnv *env, jobject camera)
{
	struct atl_camera_events *events = calloc(1, sizeof(*events));

	events->refcount = 1;
	(*env)->GetJavaVM(env, &events->jvm);
	g_mutex_init(&events->lock);
	if (ensure_java_refs(env, camera))
		events->camera = (*env)->NewWeakGlobalRef(env, camera);
	else
		events->shut_down = true;
	return events;
}

void atl_camera_events_post_shutter(struct atl_camera_events *events)
{
	struct camera_event *event = calloc(1, sizeof(*event));

	event->type = EVENT_SHUTTER;
	post_event(events, event);
}

void atl_camera_events_post_picture(struct atl_camera_events *events, const uint8_t *jpeg, size_t size)
{
	struct camera_event *event = calloc(1, sizeof(*event));

	event->type = EVENT_PICTURE;
	if (jpeg && size) {
		event->data = malloc(size);
		if (event->data) {
			memcpy(event->data, jpeg, size);
			event->size = size;
		}
	}
	post_event(events, event);
}

void atl_camera_events_post_autofocus(struct atl_camera_events *events, bool success)
{
	struct camera_event *event = calloc(1, sizeof(*event));

	event->type = EVENT_AUTOFOCUS;
	event->success = success;
	post_event(events, event);
}

void atl_camera_events_post_error(struct atl_camera_events *events, int error)
{
	struct camera_event *event = calloc(1, sizeof(*event));

	event->type = EVENT_ERROR;
	event->error = error;
	post_event(events, event);
}

void atl_camera_events_free(struct atl_camera_events *events, JNIEnv *env)
{
	g_mutex_lock(&events->lock);
	events->shut_down = true;
	if (events->camera)
		(*env)->DeleteWeakGlobalRef(env, events->camera);
	events->camera = NULL;
	g_mutex_unlock(&events->lock);

	events_unref(events);
}
