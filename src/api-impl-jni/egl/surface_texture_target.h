#ifndef ATL_EGL_SURFACE_TEXTURE_TARGET_H
#define ATL_EGL_SURFACE_TEXTURE_TARGET_H

#include <EGL/egl.h>
#include <jni.h>
#include <stdbool.h>

/*
 * eglCreateWindowSurface(SurfaceTexture): an EGL surface an app can render
 * into whose frames end up in the SurfaceTexture's mailbox. See
 * surface_texture_target.c for why it is a pbuffer and not a window surface.
 */

/* EGL_NO_SURFACE if the SurfaceTexture is gone or no pbuffer could be made */
EGLSurface atl_egl_surface_texture_create(JNIEnv *env, EGLDisplay display, EGLConfig config,
                                          jobject surface_texture, const EGLint *attrib_list);

/* the swap of a surface from create(): reads the frame back into the mailbox.
 * false means this is not one of ours and the caller should swap it itself. */
bool atl_egl_surface_texture_swap(EGLDisplay display, EGLSurface surface);

/* drop the bookkeeping; the caller still destroys the EGL surface itself */
void atl_egl_surface_texture_release(EGLSurface surface);

#endif
