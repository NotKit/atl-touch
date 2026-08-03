#ifndef ATL_CAMERA_PREVIEW_H
#define ATL_CAMERA_PREVIEW_H

#include <jni.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Camera preview -> android.view.Surface (the setPreviewDisplay path).
 *
 * NV21 frames arrive on a backend thread, get converted to RGBA there and are
 * handed to a poster thread that does the JNI (Surface.postFrame(Bitmap), the
 * same hand-off MediaCodec uses for decoded video). Only the newest frame is
 * kept: a frame that arrives while the previous one is still unposted replaces
 * it, so a slow consumer costs frames, never memory.
 */

struct atl_camera_preview;

struct atl_camera_preview *atl_camera_preview_new(JNIEnv *env);
/* stops delivery, joins the poster thread and drops the surface reference */
void atl_camera_preview_free(struct atl_camera_preview *preview, JNIEnv *env);

/* surface: an android.view.Surface, or NULL to detach and stop delivery */
void atl_camera_preview_set_surface(struct atl_camera_preview *preview, JNIEnv *env, jobject surface);

/* backend frame callback side; no-op (and no conversion) without a surface */
void atl_camera_preview_submit(struct atl_camera_preview *preview, const uint8_t *nv21,
                               int width, int height, int stride);

#endif
