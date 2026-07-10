/*
 * NDK EGL shims (bionic_egl* are what bionic_translation resolves EGL symbols
 * to). With the GTK windowing gone these are plain passthroughs; the pieces
 * that need a real native window go live again with the wl_egl_window
 * bring-up (ANativeWindow_fromSurface currently returns NULL, so apps never
 * get this far with a usable window).
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GL/gl.h>
#include <GLES2/gl2.h>

// FIXME: put the header in a common place
#include "../api-impl-jni/defines.h"

#include "native_window.h"

static GHashTable *egl_surface_hashtable;

// this is an extension that only android implements, we can hopefully get away with just stubbing it
EGLBoolean bionic_eglPresentationTimeANDROID(EGLDisplay dpy, EGLSurface surface, EGLnsecsANDROID time)
{
	return EGL_TRUE;
}

void (*bionic_eglGetProcAddress(char const *procname))(void)
{
	if (__unlikely__(!strcmp(procname, "eglPresentationTimeANDROID")))
		return (void (*)(void))bionic_eglPresentationTimeANDROID;

	return eglGetProcAddress(procname);
}

EGLDisplay bionic_eglGetDisplay(EGLNativeDisplayType native_display)
{
	/*
	 * On android, at least SDL passes 0 (EGL_DISPLAY_DEFAULT) to eglGetDisplay
	 * and uses the resulting display. The wl_egl_window bring-up will hand out
	 * the GLFW window's wl_display here; until then the EGL default display is
	 * the best available answer.
	 */
	return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EGLBoolean bionic_eglChooseConfig(EGLDisplay display, EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
	/* Wayland EGL has no pbuffer support; rewrite EGL_PBUFFER_BIT requests to
	 * EGL_WINDOW_BIT so config selection doesn't come up empty. */
	bool has_pbuffer_bit = false;
	int attrib_list_size = 0;
	for (EGLint *attr = attrib_list; *attr != EGL_NONE; attr += 2) {
		if (*attr == EGL_SURFACE_TYPE && (*(attr + 1) & EGL_PBUFFER_BIT) && *(attr + 1) != EGL_DONT_CARE) {
			has_pbuffer_bit = true;
		}
		attrib_list_size += 2;
	}
	attrib_list_size += 1; // for EGL_NONE
	if (has_pbuffer_bit) {
		/* copy the list in case it's mapped read-only */
		EGLint *new_attrib_list = malloc(sizeof(EGLint) * attrib_list_size);
		memcpy(new_attrib_list, attrib_list, sizeof(EGLint) * attrib_list_size);
		for (EGLint *attr = new_attrib_list; *attr != EGL_NONE; attr += 2) {
			if (*attr == EGL_SURFACE_TYPE && *(attr + 1) != EGL_DONT_CARE) {
				*(attr + 1) &= ~EGL_PBUFFER_BIT;
				*(attr + 1) |= EGL_WINDOW_BIT;
			}
		}
		EGLBoolean ret = eglChooseConfig(display, new_attrib_list, configs, config_size, num_config);
		free(new_attrib_list);
		return ret;
	}
	return eglChooseConfig(display, attrib_list, configs, config_size, num_config);
}

EGLSurface bionic_eglCreatePbufferSurface(EGLDisplay display, EGLConfig config, EGLint const *attrib_list)
{
	return eglCreatePbufferSurface(display, config, attrib_list);
}

EGLSurface bionic_eglCreateWindowSurface(EGLDisplay display, EGLConfig config, struct ANativeWindow *native_window, EGLint const *attrib_list)
{
	// better than crashing (TODO: check if apps try to use the NULL value anyway)
	if (!native_window)
		return NULL;

	if (!egl_surface_hashtable)
		egl_surface_hashtable = g_hash_table_new(NULL, NULL);

	ANativeWindow_acquire(native_window);

	EGLSurface surface = eglCreateWindowSurface(display, config, native_window->egl_window, attrib_list);
	if (surface)
		g_hash_table_insert(egl_surface_hashtable, surface, native_window);
	else
		ANativeWindow_release(native_window);

	return surface;
}

EGLBoolean bionic_eglDestroySurface(EGLDisplay display, EGLSurface surface)
{
	struct ANativeWindow *native_window =
	    egl_surface_hashtable ? g_hash_table_lookup(egl_surface_hashtable, surface) : NULL;

	EGLBoolean ret = eglDestroySurface(display, surface);
	if (ret && native_window) {
		g_hash_table_remove(egl_surface_hashtable, surface);
		ANativeWindow_release(native_window);
	}
	return ret;
}

EGLBoolean bionic_eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context)
{
	return eglMakeCurrent(display, draw, read, context);
}

EGLBoolean bionic_eglSwapBuffers(EGLDisplay display, EGLSurface surface)
{
	return eglSwapBuffers(display, surface);
}

EGLBoolean bionic_eglQuerySurface(EGLDisplay display, EGLSurface surface, EGLint attribute, EGLint *value)
{
	return eglQuerySurface(display, surface, attribute, value);
}

EGLSurface bionic_eglGetCurrentSurface(EGLint readdraw)
{
	return eglGetCurrentSurface(readdraw);
}

void bionic_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	glBindFramebuffer(target, framebuffer);
}
