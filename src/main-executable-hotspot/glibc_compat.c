/*
 * The bionic_* entry points libtranslation_layer_main.so calls, implemented
 * against the host libc/EGL.
 *
 * Under ART these come from the bionic translation layer, which has to bridge
 * the app's bionic-linked libraries to the host. A JVM build has no bionic in
 * it at all: the app's own libraries are host libraries, so each of these is
 * the plain host call.
 */

#define _GNU_SOURCE

#include <dlfcn.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "../api-impl-jni/ATLWindow.h"
#include "../libandroid/native_window.h"

void *bionic_dlopen(const char *filename, int flag)
{
	return dlopen(filename, flag);
}

void *bionic_dlsym(void *handle, const char *symbol)
{
	return dlsym(handle, symbol);
}

EGLDisplay bionic_eglGetDisplay(NativeDisplayType native_display)
{
	/* the legacy GLES1 binding passes no usable display, and everything else
	 * already renders onto the window's display */
	if (!native_display)
		return atl_primary_egl_display();

	return eglGetDisplay(native_display);
}

EGLSurface bionic_eglCreateWindowSurface(EGLDisplay display, EGLConfig config, struct ANativeWindow *native_window, EGLint const *attrib_list)
{
	if (!native_window)
		return EGL_NO_SURFACE;

	return eglCreateWindowSurface(display, config, native_window->egl_window, attrib_list);
}

EGLBoolean bionic_eglDestroySurface(EGLDisplay display, EGLSurface surface)
{
	return eglDestroySurface(display, surface);
}

EGLBoolean bionic_eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context)
{
	return eglMakeCurrent(display, draw, read, context);
}

EGLBoolean bionic_eglSwapBuffers(EGLDisplay display, EGLSurface surface)
{
	return eglSwapBuffers(display, surface);
}

void bionic_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	glBindFramebuffer(target, framebuffer);
}
