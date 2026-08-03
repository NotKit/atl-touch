#ifndef ATL_CAMERA_EVENTS_H
#define ATL_CAMERA_EVENTS_H

#include <jni.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * One-off app-visible Camera callbacks (shutter, picture, autofocus, error).
 * Like the preview callbacks (camera_callbacks.h) they are posted to the main
 * loop with g_idle_add and land on private Camera.dispatch* methods, which own
 * the app's callback objects; this side only holds a weak ref to the Camera.
 */

struct atl_camera_events;

struct atl_camera_events *atl_camera_events_new(JNIEnv *env, jobject camera);
/* stops delivery; queued events still in flight only drop their reference */
void atl_camera_events_free(struct atl_camera_events *events, JNIEnv *env);

void atl_camera_events_post_shutter(struct atl_camera_events *events);
/* jpeg = NULL when the capture failed; the buffer is copied */
void atl_camera_events_post_picture(struct atl_camera_events *events, const uint8_t *jpeg, size_t size);
void atl_camera_events_post_autofocus(struct atl_camera_events *events, bool success);
void atl_camera_events_post_error(struct atl_camera_events *events, int error);

#endif
