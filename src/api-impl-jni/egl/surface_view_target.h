#ifndef ATL_EGL_SURFACE_VIEW_TARGET_H
#define ATL_EGL_SURFACE_VIEW_TARGET_H

#include <EGL/egl.h>
#include <stdbool.h>
#include <jni.h>

struct ANativeWindow;

/* A pbuffer target for a SurfaceView with no native EGL window, as on X11. */
bool atl_egl_surface_view_matches(JNIEnv *env, jobject surface);
EGLSurface atl_egl_surface_view_create(EGLDisplay display, EGLConfig config,
                                       struct ANativeWindow *window);
bool atl_egl_surface_view_swap(EGLSurface surface);
void atl_egl_surface_view_release(EGLSurface surface);

#endif
