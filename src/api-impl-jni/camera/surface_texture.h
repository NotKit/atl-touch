#ifndef ATL_SURFACE_TEXTURE_H
#define ATL_SURFACE_TEXTURE_H

#include <jni.h>
#include <stdint.h>

/*
 * The native side of android.graphics.SurfaceTexture: a single-frame mailbox
 * between a producer (the camera backend) and a consumer that either uploads
 * the frame into a GL texture (updateTexImage) or draws it as a Bitmap
 * (TextureView). See android_graphics_SurfaceTexture.c.
 */

struct atl_surface_texture;

/* the object behind an android.graphics.SurfaceTexture, with a reference
 * taken; NULL for a null or released texture */
struct atl_surface_texture *atl_surface_texture_from_java(JNIEnv *env, jobject surface_texture);
void atl_surface_texture_ref(struct atl_surface_texture *texture);
void atl_surface_texture_unref(struct atl_surface_texture *texture);

/* producer side, called on the backend thread; the newest frame wins */
void atl_surface_texture_submit(struct atl_surface_texture *texture, const uint8_t *nv21,
                                int width, int height, int stride);

#endif
