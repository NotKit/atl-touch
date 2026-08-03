#ifndef ATL_SURFACE_TEXTURE_H
#define ATL_SURFACE_TEXTURE_H

#include <jni.h>
#include <stdbool.h>
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

/*
 * A producer that can fill the GL texture itself (the hybris preview-texture
 * fast path). update() is called from updateTexImage on the app's GL thread and
 * must bind and update tex_name; returning false drops the texture back to the
 * NV21 upload path for good. Both callbacks run under the texture's lock.
 */
struct atl_surface_texture_source {
	bool (*update)(void *user, unsigned tex_name);
	bool (*get_transform)(void *user, float matrix[16]);
	void *user;
};

/* source = NULL detaches the fast path */
void atl_surface_texture_set_source(struct atl_surface_texture *texture,
                                    const struct atl_surface_texture_source *source);
/* fast path producer side: a new frame is waiting in the texture */
void atl_surface_texture_notify_frame_available(struct atl_surface_texture *texture);

#endif
