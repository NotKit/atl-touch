#ifndef ATL_CAMERA_CALLBACKS_H
#define ATL_CAMERA_CALLBACKS_H

#include <jni.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Camera.PreviewCallback delivery (the byte[] preview path).
 *
 * NV21 frames arrive on a backend thread and are handed to the app's main
 * loop with g_idle_add, so onPreviewFrame() runs where the app expects to
 * touch its UI. Only the newest frame is kept: one arriving before the
 * previous one was delivered replaces it.
 */

/* keep in sync with Camera.java's PREVIEW_CB_* */
#define ATL_CAMERA_CB_NONE        0
#define ATL_CAMERA_CB_EVERY_FRAME 1
#define ATL_CAMERA_CB_ONE_SHOT    2
#define ATL_CAMERA_CB_WITH_BUFFER 3

struct atl_camera_callbacks;

/* camera: the android.hardware.Camera passed back to onPreviewFrame */
struct atl_camera_callbacks *atl_camera_callbacks_new(JNIEnv *env, jobject camera);
/* stops delivery and drops every reference; safe with an idle call in flight */
void atl_camera_callbacks_free(struct atl_camera_callbacks *callbacks, JNIEnv *env);

/* callback = NULL (or mode ATL_CAMERA_CB_NONE) stops delivery */
void atl_camera_callbacks_set(struct atl_camera_callbacks *callbacks, JNIEnv *env,
                              jobject callback, int mode);
/* with-buffer mode: queue a byte[] for the next frame to be written into */
void atl_camera_callbacks_add_buffer(struct atl_camera_callbacks *callbacks, JNIEnv *env,
                                     jbyteArray buffer);

/* backend frame callback side; no copy is made when nothing is registered */
void atl_camera_callbacks_submit(struct atl_camera_callbacks *callbacks, const uint8_t *nv21,
                                 int width, int height, int stride);

#endif
