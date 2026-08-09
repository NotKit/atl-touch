#include <stdint.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <jni.h>

struct wl_display;
struct wl_surface;

struct ANativeWindow {
	EGLNativeWindowType egl_window; /* the ATLSurfaceLayer's wl_egl_window */
	struct wl_display *wayland_display;
	struct wl_surface *wayland_surface;
	int refcount;
	int width;
	int height;
};

/*
 * Built by the main library (widgets/android_view_SurfaceView.c) around an
 * ATLSurfaceLayer's Wayland objects and stashed in android.view.Surface's
 * nativeWindow field; libandroid only ever reads it, so it never has to talk
 * Wayland or call back into the main library.
 *
 * The layer owns the wl_surface/wl_egl_window, so dropping the last reference
 * to a window frees the struct only. atl_native_window_detach() is what the
 * layer calls on the way out, after the app's surfaceDestroyed() has returned.
 */
struct ANativeWindow *atl_native_window_new(struct wl_display *display, struct wl_surface *surface,
                                            void *egl_window, int width, int height);
void atl_native_window_detach(struct ANativeWindow *native_window);

/*
 * For code that is not loaded through bionic_translation (route B: Gecko built
 * natively against glibc), whose eglGetDisplay() goes straight to the host
 * libEGL and would get a different display over a different connection, and
 * whose eglCreateWindowSurface() would be handed an ANativeWindow * where the
 * host expects a struct wl_egl_window *.
 */
EGLDisplay ANativeWindow_getEGLDisplay(void);
EGLSurface ANativeWindow_createEGLSurface(struct ANativeWindow *native_window, EGLDisplay display,
                                          EGLConfig config, const EGLint *attrib_list);

/* the EGLDisplay GLFW created its Wayland connection on: a wl_egl_window is
 * only usable on that display, so app-side eglGetDisplay() must return it */
void bionic_egl_set_primary_display(EGLDisplay display);

struct ANativeWindow *ANativeWindow_fromSurface(JNIEnv *env, jobject surface);
EGLSurface bionic_eglCreateWindowSurface(EGLDisplay display, EGLConfig config, struct ANativeWindow *native_window, EGLint const *attrib_list);
EGLBoolean bionic_eglDestroySurface(EGLDisplay display, EGLSurface surface);
EGLBoolean bionic_eglTerminate(EGLDisplay display);
EGLDisplay bionic_eglGetDisplay(NativeDisplayType native_display);
EGLBoolean bionic_eglChooseConfig(EGLDisplay display, EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLSurface bionic_eglCreatePbufferSurface(EGLDisplay display, EGLConfig config, EGLint const *attrib_list);
EGLBoolean bionic_eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context);
EGLBoolean bionic_eglSwapBuffers(EGLDisplay display, EGLSurface surface);
EGLBoolean bionic_eglQuerySurface(EGLDisplay display, EGLSurface surface, EGLint attribute, EGLint *value);
EGLSurface bionic_eglGetCurrentSurface(EGLint readdraw);
EGLBoolean bionic_eglPresentationTimeANDROID(EGLDisplay dpy, EGLSurface surface, EGLnsecsANDROID time);
void bionic_glBindFramebuffer(GLenum target, GLuint framebuffer);
void ANativeWindow_acquire(struct ANativeWindow *native_window);
void ANativeWindow_release(struct ANativeWindow *native_window);
int32_t ANativeWindow_getWidth(struct ANativeWindow *native_window);
int32_t ANativeWindow_getHeight(struct ANativeWindow *native_window);
int32_t ANativeWindow_getFormat(struct ANativeWindow *native_window);
