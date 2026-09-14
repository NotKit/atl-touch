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

/*
 * EGL_ANDROID_get_native_client_buffer: an app's native code turning a camera
 * or gralloc AHardwareBuffer into an EGLImage (Google Camera's viewfinder
 * effects link it by name). The handle it holds is the real gralloc one - that
 * is what AHardwareBuffer_fromHardwareBuffer hands out on a device - so this
 * is the host EGL's own entry point, where it has one.
 */
EGLClientBuffer bionic_eglGetNativeClientBufferANDROID(const void *buffer)
{
	static EGLClientBuffer (*get_native_client_buffer)(const void *);
	static bool tried;

	if (!tried) {
		tried = true;
		get_native_client_buffer = (void *)eglGetProcAddress("eglGetNativeClientBufferANDROID");
		if (!get_native_client_buffer)
			fprintf(stderr, "EGL: this EGL has no eglGetNativeClientBufferANDROID, an app's "
			                "EGLImage of a hardware buffer cannot be made\n");
	}
	return get_native_client_buffer ? get_native_client_buffer(buffer) : NULL;
}

void (*bionic_eglGetProcAddress(char const *procname))(void)
{
	if (__unlikely__(!strcmp(procname, "eglGetNativeClientBufferANDROID")))
		return (void (*)(void))bionic_eglGetNativeClientBufferANDROID;
	if (__unlikely__(!strcmp(procname, "eglPresentationTimeANDROID")))
		return (void (*)(void))bionic_eglPresentationTimeANDROID;

	return eglGetProcAddress(procname);
}

static EGLDisplay primary_display = EGL_NO_DISPLAY;

/* GLFW made its display with eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND, its
 * own wl_display); a wl_egl_window on that connection is only usable there,
 * and eglGetDisplay(EGL_DEFAULT_DISPLAY) is a different display over a
 * different connection. Called from atl_window_new. */
void bionic_egl_set_primary_display(EGLDisplay display)
{
	primary_display = display;
}

EGLDisplay bionic_eglGetDisplay(EGLNativeDisplayType native_display)
{
	/*
	 * On android, at least SDL passes 0 (EGL_DISPLAY_DEFAULT) to eglGetDisplay
	 * and uses the resulting display; an app that gets the default display
	 * cannot present through a SurfaceView's wl_egl_window.
	 */
	if (primary_display != EGL_NO_DISPLAY)
		return primary_display;
	return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EGLDisplay ANativeWindow_getEGLDisplay(void)
{
	return primary_display != EGL_NO_DISPLAY ? primary_display : eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EGLSurface ANativeWindow_createEGLSurface(struct ANativeWindow *native_window, EGLDisplay display,
                                          EGLConfig config, const EGLint *attrib_list)
{
	if (!native_window || !native_window->egl_window)
		return EGL_NO_SURFACE;
	return bionic_eglCreateWindowSurface(display, config, native_window, attrib_list);
}

EGLBoolean bionic_eglTerminate(EGLDisplay display)
{
	/* the whole process shares one display with GLFW and Skia; an app that
	 * tears down its own EGL must not take theirs with it */
	if (display == primary_display)
		return EGL_TRUE;
	return eglTerminate(display);
}

EGLBoolean bionic_eglChooseConfig(EGLDisplay display, EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
	/* Wayland EGL has no pbuffer support; rewrite EGL_PBUFFER_BIT requests to
	 * EGL_WINDOW_BIT so config selection doesn't come up empty. Only as a
	 * fallback: on platforms that do have pbuffer configs (surfaceless, X11)
	 * the rewrite is what would come up empty. */
	bool has_pbuffer_bit = false;
	int attrib_list_size = 0;
	/* a NULL list is legal and means "no constraints" */
	for (EGLint *attr = attrib_list; attr && *attr != EGL_NONE; attr += 2) {
		if (*attr == EGL_SURFACE_TYPE && (*(attr + 1) & EGL_PBUFFER_BIT) && *(attr + 1) != EGL_DONT_CARE) {
			has_pbuffer_bit = true;
		}
		attrib_list_size += 2;
	}
	attrib_list_size += 1; // for EGL_NONE
	if (has_pbuffer_bit) {
		EGLint num = 0;
		if (eglChooseConfig(display, attrib_list, configs, config_size, &num) && num > 0) {
			*num_config = num;
			return EGL_TRUE;
		}

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

/*
 * Android exports eglCreateImageKHR as a direct libEGL.so symbol, where Mesa
 * only has it through eglGetProcAddress and links the core EGL 1.5
 * eglCreateImage instead. The two do not share an attribute ABI:
 * EGL_KHR_image_base's list is EGLint, the core one's is EGLAttrib. Forwarding
 * the narrow list to the wide entry point makes the driver scan 8-byte entries
 * for an EGL_NONE it never finds and read off the end of the caller's array -
 * on Google Camera's mode switch that walked past the top of the main thread's
 * stack.
 *
 * So take the driver's own KHR entry point where it has one, and widen the list
 * only for the core call.
 */
EGLImage bionic_eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list)
{
	static EGLImage (*create_image_khr)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint *);
	static bool tried;
	/* more pairs than any EGLImage target defines, so no allocation in practice */
	EGLAttrib inline_attribs[17];
	EGLAttrib *wide = inline_attribs;
	EGLImage image;
	size_t n = 0;

	if (!tried) {
		tried = true;
		create_image_khr = (void *)eglGetProcAddress("eglCreateImageKHR");
	}
	if (create_image_khr)
		return create_image_khr(dpy, ctx, target, buffer, attrib_list);

	while (attrib_list && attrib_list[n] != EGL_NONE)
		n += 2;
	if (n + 1 > ARRAY_SIZE(inline_attribs)) {
		wide = malloc((n + 1) * sizeof(*wide));
		if (!wide)
			return EGL_NO_IMAGE;
	}
	for (size_t i = 0; i < n; i++)
		wide[i] = attrib_list[i]; /* sign-extends EGL_DONT_CARE and friends */
	wide[n] = EGL_NONE;

	image = eglCreateImage(dpy, ctx, target, buffer, wide);
	if (wide != inline_attribs)
		free(wide);
	return image;
}

EGLBoolean bionic_eglDestroyImageKHR(EGLDisplay dpy, EGLImage image)
{
	return eglDestroyImage(dpy, image);
}

EGLSurface bionic_eglCreateWindowSurface(EGLDisplay display, EGLConfig config, struct ANativeWindow *native_window, EGLint const *attrib_list)
{
	// better than crashing (TODO: check if apps try to use the NULL value anyway)
	if (!native_window)
		return NULL;

	/* A readback-mode window (no subsurface, so no wl_egl_window) has nothing a
	 * GL driver can render into. Handing the NULL down is not survivable:
	 * libhybris' wayland EGL platform aborts the process on it, where Mesa only
	 * fails. Fail the call instead, so the app can fall back. (eglGetError is
	 * the driver's, so it still says EGL_SUCCESS; the log line is the signal.) */
	if (!native_window->egl_window) {
		fprintf(stderr, "eglCreateWindowSurface: this ANativeWindow has no EGL window "
		                "(readback mode); a GL producer needs the subsurface path\n");
		return EGL_NO_SURFACE;
	}

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
