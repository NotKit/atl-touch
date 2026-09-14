#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <GLES2/gl2.h>

#include "../../libandroid/native_window.h"
#include "../../libandroid/window_frame.h"
#include "surface_view_target.h"

struct view_target {
	int refcount;
	struct ANativeWindow *window;
	int width;
	int height;
	uint8_t *pixels;
	bool warned_gl_error;
	uint64_t frames;
};

static GMutex targets_lock;
static GHashTable *targets;

bool atl_egl_surface_view_matches(JNIEnv *env, jobject surface)
{
	jclass surface_class = (*env)->GetObjectClass(env, surface);
	jfieldID view_field = (*env)->GetFieldID(env, surface_class, "view", "Landroid/view/SurfaceView;");
	jobject view = view_field ? (*env)->GetObjectField(env, surface, view_field) : NULL;
	bool matches = view != NULL;

	if (!view_field)
		(*env)->ExceptionClear(env);
	if (view)
		(*env)->DeleteLocalRef(env, view);
	(*env)->DeleteLocalRef(env, surface_class);
	return matches;
}

static void target_unref_locked(struct view_target *target)
{
	if (--target->refcount)
		return;
	ANativeWindow_release(target->window);
	free(target->pixels);
	free(target);
}

static struct view_target *target_lookup_ref(EGLSurface surface)
{
	struct view_target *target;

	g_mutex_lock(&targets_lock);
	target = targets ? g_hash_table_lookup(targets, surface) : NULL;
	if (target)
		target->refcount++;
	g_mutex_unlock(&targets_lock);
	return target;
}

static void target_unref(struct view_target *target)
{
	g_mutex_lock(&targets_lock);
	target_unref_locked(target);
	g_mutex_unlock(&targets_lock);
}

EGLSurface atl_egl_surface_view_create(EGLDisplay display, EGLConfig config,
                                       struct ANativeWindow *window)
{
	struct view_target *target;
	EGLSurface surface;
	EGLint width, height;

	if (!window || window->egl_window || !window->surface)
		return EGL_NO_SURFACE;
	width = window->width;
	height = window->height;
	if (width <= 0 || height <= 0)
		return EGL_NO_SURFACE;

	EGLint attributes[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
	surface = eglCreatePbufferSurface(display, config, attributes);
	if (surface == EGL_NO_SURFACE) {
		fprintf(stderr, "SurfaceView: no %dx%d EGL pbuffer: 0x%x\n", width, height, eglGetError());
		return EGL_NO_SURFACE;
	}

	target = calloc(1, sizeof(*target));
	if (!target) {
		eglDestroySurface(display, surface);
		return EGL_NO_SURFACE;
	}
	target->pixels = malloc((size_t)width * height * 4);
	if (!target->pixels) {
		free(target);
		eglDestroySurface(display, surface);
		return EGL_NO_SURFACE;
	}
	target->refcount = 1;
	target->window = window;
	target->width = width;
	target->height = height;
	ANativeWindow_acquire(window);

	g_mutex_lock(&targets_lock);
	if (!targets)
		targets = g_hash_table_new(NULL, NULL);
	g_hash_table_insert(targets, surface, target);
	g_mutex_unlock(&targets_lock);
	fprintf(stderr, "SurfaceView: rendering into a %dx%d EGL pbuffer on X11\n", width, height);
	return surface;
}

bool atl_egl_surface_view_swap(EGLSurface surface)
{
	struct view_target *target = target_lookup_ref(surface);
	struct atl_window_frame frame;

	if (!target)
		return false;
	glReadPixels(0, 0, target->width, target->height, GL_RGBA, GL_UNSIGNED_BYTE, target->pixels);
	if (glGetError() != GL_NO_ERROR) {
		if (!target->warned_gl_error) {
			fprintf(stderr, "SurfaceView: GL readback failed\n");
			target->warned_gl_error = true;
		}
	} else {
		frame = (struct atl_window_frame){
			.pixels = target->pixels,
			.width = target->width,
			.height = target->height,
			.stride = (size_t)target->width * 4,
			.format = ATL_WINDOW_FORMAT_RGBA_8888,
		};
		/* glReadPixels starts with the bottom row. */
		if (atl_native_window_present(target->window, &frame, NULL,
		                              ATL_WINDOW_TRANSFORM_MIRROR_VERTICAL) &&
		    target->frames++ == 0)
			fprintf(stderr, "SurfaceView: first %dx%d GL frame posted\n",
			        target->width, target->height);
	}
	target_unref(target);
	return true;
}

void atl_egl_surface_view_release(EGLSurface surface)
{
	g_mutex_lock(&targets_lock);
	struct view_target *target = targets ? g_hash_table_lookup(targets, surface) : NULL;
	if (target) {
		g_hash_table_remove(targets, surface);
		target_unref_locked(target);
	}
	g_mutex_unlock(&targets_lock);
}
