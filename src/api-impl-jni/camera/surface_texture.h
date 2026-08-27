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
 * The same, for a producer that already has RGBA rows (a GL renderer reading
 * back its EGL surface). bottom_up flips them: glReadPixels starts at the
 * bottom row, the mailbox keeps frames top row first.
 */
void atl_surface_texture_submit_rgba(struct atl_surface_texture *texture, const uint8_t *rgba,
                                     int width, int height, int stride, bool bottom_up);

/* true while a submitted frame is still waiting to be taken, which is a
 * producer's cue that another one would only be dropped */
bool atl_surface_texture_frame_pending(struct atl_surface_texture *texture);

/*
 * Block until the mailbox is free, i.e. the consumer has taken the frame in it,
 * and return whether it is. This is the backpressure a BufferQueue would give a
 * producer: without it a GL producer's eglSwapBuffers returns at once and the
 * app free-runs. Never waits longer than timeout_us, so a consumer that has
 * stopped drawing slows a producer down instead of stopping it.
 */
bool atl_surface_texture_await_frame_taken(struct atl_surface_texture *texture, int64_t timeout_us);

/* what setDefaultBufferSize() last asked for, 0x0 if it never ran */
void atl_surface_texture_get_default_size(struct atl_surface_texture *texture, int *width, int *height);

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
