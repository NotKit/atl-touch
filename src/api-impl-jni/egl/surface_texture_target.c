/*
 * eglCreateWindowSurface(SurfaceTexture) -- the producer side of
 * android.graphics.SurfaceTexture for apps that render with GL.
 *
 * AOSP hands the app the SurfaceTexture's BufferQueue and the consumer samples
 * the very buffer the app drew into. There is no BufferQueue here, and the
 * consumer (TextureView) composites through Skia in the window's own GL
 * context on another thread, so a rendered buffer could not be shared with it
 * anyway. What the mailbox in camera/surface_texture.h wants is pixels.
 *
 * So the surface is a pbuffer, and each eglSwapBuffers reads it back and
 * submits it to the mailbox, where TextureView.onDraw picks it up as a Bitmap
 * like a camera frame. A readback per frame is the cost of the missing
 * BufferQueue; a swap waits for the consumer to take the previous frame, which
 * paces the app at the rate the view actually draws, and skips the readback
 * only once that wait times out.
 *
 * Known limitation: a pbuffer cannot be resized, and apps do not recreate the
 * surface when the TextureView changes size (they just move their viewport).
 * The pbuffer is therefore made at the size the SurfaceTexture had when the
 * app asked for it: a view that later shrinks is fine, one that grows past it
 * is read back clipped, once with a warning.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <GLES2/gl2.h>

#include "../camera/surface_texture.h"

#include "surface_texture_target.h"

/*
 * How long a swap waits for the consumer before giving up on the frame. Two
 * 60Hz frames: long enough to pace a producer against a view that is drawing,
 * short enough that one which has stopped only slows the producer down.
 */
#define ST_SWAP_WAIT_US (2 * 16667)

struct st_target {
	int refcount;
	EGLDisplay display;
	EGLSurface surface;
	struct atl_surface_texture *texture;
	int width;
	int height;
	uint8_t *pixels;
	bool warned_clipped;
	bool warned_gl_error;
	uint64_t frames;
	uint64_t skipped;
};

/* guards the table and every refcount in it */
static GMutex targets_lock;
static GHashTable *targets;

static void target_unref_locked(struct st_target *target)
{
	if (--target->refcount > 0)
		return;

	fprintf(stderr, "SurfaceTexture: EGL target %dx%d: %" G_GUINT64_FORMAT " frames read back, %"
	        G_GUINT64_FORMAT " skipped\n", target->width, target->height, target->frames, target->skipped);
	atl_surface_texture_unref(target->texture);
	free(target->pixels);
	free(target);
}

static struct st_target *target_lookup_ref(EGLSurface surface)
{
	struct st_target *target;

	g_mutex_lock(&targets_lock);
	target = targets ? g_hash_table_lookup(targets, surface) : NULL;
	if (target)
		target->refcount++;
	g_mutex_unlock(&targets_lock);
	return target;
}

static void target_unref(struct st_target *target)
{
	g_mutex_lock(&targets_lock);
	target_unref_locked(target);
	g_mutex_unlock(&targets_lock);
}

/*
 * The app picked its config for a window surface, so it may have no
 * EGL_PBUFFER_BIT. Find one that has, with the same colour and ancillary
 * buffer depths -- that is what makes a config compatible with the context the
 * app has already created.
 */
static bool pbuffer_capable_config(EGLDisplay display, EGLConfig config, EGLConfig *out)
{
	EGLint renderable = 0, r = 0, g = 0, b = 0, a = 0, depth = 0, stencil = 0;
	EGLint sample_buffers = 0, samples = 0, num = 0;

	eglGetConfigAttrib(display, config, EGL_RENDERABLE_TYPE, &renderable);
	eglGetConfigAttrib(display, config, EGL_RED_SIZE, &r);
	eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &g);
	eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &b);
	eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &a);
	eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth);
	eglGetConfigAttrib(display, config, EGL_STENCIL_SIZE, &stencil);
	eglGetConfigAttrib(display, config, EGL_SAMPLE_BUFFERS, &sample_buffers);
	eglGetConfigAttrib(display, config, EGL_SAMPLES, &samples);

	EGLint spec[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, renderable,
		EGL_RED_SIZE, r,
		EGL_GREEN_SIZE, g,
		EGL_BLUE_SIZE, b,
		EGL_ALPHA_SIZE, a,
		EGL_DEPTH_SIZE, depth,
		EGL_STENCIL_SIZE, stencil,
		EGL_SAMPLE_BUFFERS, sample_buffers,
		EGL_SAMPLES, samples,
		EGL_NONE
	};

	if (eglChooseConfig(display, spec, out, 1, &num) && num > 0)
		return true;

	/* multisampling is the likeliest thing no pbuffer config offers; the
	 * frame is read back resolved either way */
	for (EGLint *attr = spec; *attr != EGL_NONE; attr += 2) {
		if (attr[0] == EGL_SAMPLE_BUFFERS || attr[0] == EGL_SAMPLES)
			attr[1] = EGL_DONT_CARE;
	}
	if (eglChooseConfig(display, spec, out, 1, &num) && num > 0)
		return true;

	return false;
}

EGLSurface atl_egl_surface_texture_create(JNIEnv *env, EGLDisplay display, EGLConfig config,
                                          jobject surface_texture, const EGLint *attrib_list)
{
	struct atl_surface_texture *texture = atl_surface_texture_from_java(env, surface_texture);
	struct st_target *target;
	EGLSurface surface;
	int width = 0, height = 0;

	if (!texture) {
		fprintf(stderr, "eglCreateWindowSurface: the SurfaceTexture has been released\n");
		return EGL_NO_SURFACE;
	}

	atl_surface_texture_get_default_size(texture, &width, &height);
	for (const EGLint *attr = attrib_list; attr && *attr != EGL_NONE; attr += 2) {
		if (*attr == EGL_WIDTH)
			width = attr[1];
		else if (*attr == EGL_HEIGHT)
			height = attr[1];
	}
	if (width <= 0 || height <= 0) {
		/* the view has not been laid out: nothing can be drawn into it yet,
		 * but the app's GL thread must still get a surface it can use */
		fprintf(stderr, "eglCreateWindowSurface: the SurfaceTexture has no size yet, using 1x1\n");
		width = 1;
		height = 1;
	}

	EGLint pbuffer_attribs[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
	surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);
	if (surface == EGL_NO_SURFACE) {
		EGLConfig fallback;

		if (!pbuffer_capable_config(display, config, &fallback) ||
		    (surface = eglCreatePbufferSurface(display, fallback, pbuffer_attribs)) == EGL_NO_SURFACE) {
			fprintf(stderr, "eglCreateWindowSurface: no pbuffer for a %dx%d SurfaceTexture: 0x%x\n",
			        width, height, eglGetError());
			atl_surface_texture_unref(texture);
			return EGL_NO_SURFACE;
		}
		fprintf(stderr, "eglCreateWindowSurface: the app's config has no pbuffer support, "
		                "using a matching one that has\n");
	}

	target = calloc(1, sizeof(*target));
	target->refcount = 1;
	target->display = display;
	target->surface = surface;
	target->texture = texture; /* the reference from _from_java */
	target->width = width;
	target->height = height;
	target->pixels = malloc((size_t)width * height * 4);
	if (!target->pixels) {
		eglDestroySurface(display, surface);
		atl_surface_texture_unref(texture);
		free(target);
		return EGL_NO_SURFACE;
	}

	g_mutex_lock(&targets_lock);
	if (!targets)
		targets = g_hash_table_new(NULL, NULL);
	g_hash_table_insert(targets, surface, target);
	g_mutex_unlock(&targets_lock);

	fprintf(stderr, "eglCreateWindowSurface: rendering into a %dx%d SurfaceTexture\n", width, height);
	return surface;
}

bool atl_egl_surface_texture_swap(EGLDisplay display, EGLSurface surface)
{
	struct st_target *target = target_lookup_ref(surface);
	int width, height, default_width, default_height;

	if (!target)
		return false;

	/*
	 * Wait for the consumer to take the frame already in the mailbox. AOSP's
	 * eglSwapBuffers blocks on the BufferQueue the same way, and apps rely on
	 * it: GLIconTextureView busy-waits out the rest of its frame budget after
	 * the swap, so a swap that returns at once pins a core for as long as the
	 * view exists. The timeout keeps a consumer that has stopped drawing from
	 * blocking the producer for good -- then the frame really would only be
	 * dropped, so skip the readback as before.
	 */
	if (atl_surface_texture_frame_pending(target->texture) &&
	    !atl_surface_texture_await_frame_taken(target->texture, ST_SWAP_WAIT_US)) {
		target->skipped++;
		target_unref(target);
		return true;
	}

	width = target->width;
	height = target->height;
	atl_surface_texture_get_default_size(target->texture, &default_width, &default_height);
	if (default_width > 0 && default_height > 0) {
		if ((default_width > width || default_height > height) && !target->warned_clipped) {
			fprintf(stderr, "SurfaceTexture: the view grew to %dx%d after its EGL surface was made "
			                "at %dx%d; the frame is clipped\n",
			        default_width, default_height, width, height);
			target->warned_clipped = true;
		}
		width = MIN(default_width, width);
		height = MIN(default_height, height);
	}

	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, target->pixels);

	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		if (!target->warned_gl_error) {
			fprintf(stderr, "SurfaceTexture: GL error 0x%x reading back a %dx%d frame "
			                "(no current context?)\n", error, width, height);
			target->warned_gl_error = true;
		}
		target_unref(target);
		return true;
	}

	/* glReadPixels starts at the bottom row */
	atl_surface_texture_submit_rgba(target->texture, target->pixels, width, height, width * 4, true);
	if (target->frames++ == 0) /* the run is usually killed, so the stats never print */
		fprintf(stderr, "SurfaceTexture: first %dx%d frame read back\n", width, height);
	target_unref(target);
	return true;
}

void atl_egl_surface_texture_release(EGLSurface surface)
{
	struct st_target *target;

	g_mutex_lock(&targets_lock);
	target = targets ? g_hash_table_lookup(targets, surface) : NULL;
	if (target) {
		g_hash_table_remove(targets, surface);
		target_unref_locked(target);
	}
	g_mutex_unlock(&targets_lock);
}
