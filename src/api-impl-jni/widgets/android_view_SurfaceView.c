#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_EGL
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>

#include "../defines.h"
#include "../util.h"

#include "../ATLWindow.h"
#include "../graphics/ATLCanvas.h"
#include "atl_surface_layer.h"

#include "../../libandroid/native_window.h"

#include "../generated_headers/android_view_SurfaceView.h"

/* SurfaceHolder.lockCanvas: an offscreen raster canvas; the finished frame is
 * detached as a Bitmap (native_canvas_to_bitmap in android_graphics_Bitmap.cpp)
 * and drawn into the scene by SurfaceView.onDraw. MediaCodec video frames
 * arrive through Surface.postFrame the same way. */
JNIEXPORT jlong JNICALL Java_android_view_SurfaceView_native_1createSnapshot(JNIEnv *env, jobject this, jint width, jint height)
{
	return _INTPTR(atl_canvas_new_raster(width, height));
}

JNIEXPORT jboolean JNICALL Java_android_view_SurfaceView_native_1layersAvailable(JNIEnv *env, jclass class)
{
	return atl_surface_layers_available() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL Java_android_view_SurfaceView_native_1createLayer(JNIEnv *env, jobject this, jlong window_ptr)
{
	if (!window_ptr)
		return 0;
	return _INTPTR(atl_surface_layer_new(_PTR(window_ptr)));
}

/*
 * Build the ANativeWindow the app's native code will get from
 * ANativeWindow_fromSurface() and hand it to the Surface, which owns it. This
 * is the opposite direction from AOSP (there the Surface makes the window),
 * because only the main library has the wl_compositor the layer needs.
 */
JNIEXPORT void JNICALL Java_android_view_SurfaceView_native_1bindSurface(JNIEnv *env, jobject this, jobject surface, jlong layer_ptr, jint width, jint height)
{
	ATLSurfaceLayer *layer = _PTR(layer_ptr);
	struct ANativeWindow *window;
	jfieldID field;
	jclass class;

	if (!layer || !surface)
		return;
	class = (*env)->GetObjectClass(env, surface);
	field = (*env)->GetFieldID(env, class, "nativeWindow", "J");
	if (!field) {
		(*env)->ExceptionClear(env);
		return;
	}
	if ((*env)->MonitorEnter(env, surface) != JNI_OK)
		return;
	/* the surface keeps the same window across a resize: the app may already
	 * be holding it, and its size is what ANativeWindow_getWidth reports */
	window = _PTR((*env)->GetLongField(env, surface, field));
	if (window) {
		atl_native_window_set_size(window, width, height);
	} else {
		window = atl_native_window_new(glfwGetWaylandDisplay(), atl_surface_layer_wl_surface(layer),
		                               atl_surface_layer_egl_window(layer), width, height);
		if (window)
			(*env)->SetLongField(env, surface, field, _INTPTR(window));
	}
	(*env)->MonitorExit(env, surface);
}

JNIEXPORT void JNICALL Java_android_view_SurfaceView_native_1setLayerGeometry(JNIEnv *env, jobject this, jlong layer_ptr, jint x, jint y, jint width, jint height)
{
	atl_surface_layer_set_geometry(_PTR(layer_ptr), x, y, width, height);
}

JNIEXPORT void JNICALL Java_android_view_SurfaceView_native_1setLayerBufferSize(JNIEnv *env, jobject this, jlong layer_ptr, jint width, jint height)
{
	atl_surface_layer_set_buffer_size(_PTR(layer_ptr), width, height);
}

JNIEXPORT void JNICALL Java_android_view_SurfaceView_native_1setLayerZ(JNIEnv *env, jobject this, jlong layer_ptr, jboolean above)
{
	atl_surface_layer_set_above(_PTR(layer_ptr), above == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_android_view_SurfaceView_native_1setLayerVisible(JNIEnv *env, jobject this, jlong layer_ptr, jboolean visible)
{
	atl_surface_layer_set_visible(_PTR(layer_ptr), visible == JNI_TRUE);
}

/*
 * Called after the app's surfaceDestroyed() has returned, i.e. after its
 * compositor has dropped the EGLSurface: only then is destroying the
 * wl_egl_window under it safe.
 */
JNIEXPORT void JNICALL Java_android_view_SurfaceView_native_1destroyLayer(JNIEnv *env, jobject this, jlong layer_ptr, jobject surface)
{
	struct ANativeWindow *window = NULL;

	if (surface) {
		jclass class = (*env)->GetObjectClass(env, surface);
		jfieldID field = (*env)->GetFieldID(env, class, "nativeWindow", "J");

		if (field && (*env)->MonitorEnter(env, surface) == JNI_OK) {
			window = _PTR((*env)->GetLongField(env, surface, field));
			(*env)->SetLongField(env, surface, field, 0);
			(*env)->MonitorExit(env, surface);
		} else {
			(*env)->ExceptionClear(env);
		}
	}
	if (window) {
		atl_native_window_detach(window);
		ANativeWindow_release(window); /* the Surface's own reference */
	}
	atl_surface_layer_destroy(_PTR(layer_ptr));
}

/* --- ATL_SURFACE_TEST: a native GL client, without an app ------------------
 * Takes exactly the road an NDK app takes - ANativeWindow_fromSurface, then
 * eglCreateWindowSurface on the primary display - from a thread of its own, so
 * the layer path can be exercised with any APK that has a SurfaceView. */

struct surface_test {
	struct ANativeWindow *window;
};

static void *surface_test_thread(void *data)
{
	struct surface_test *test = data;
	/* deliberately through the route-B entry points, so this exercises what a
	 * natively-built (non-bionic) client has to call */
	EGLDisplay display = ANativeWindow_getEGLDisplay();
	EGLint config_attrs[] = {
	    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
	    EGL_NONE};
	EGLint context_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
	EGLConfig config;
	EGLint num_config = 0;
	EGLSurface surface;
	EGLContext context;
	unsigned frames = 0;

	eglBindAPI(EGL_OPENGL_ES_API);
	if (!eglChooseConfig(display, config_attrs, &config, 1, &num_config) || !num_config) {
		fprintf(stderr, "ATL_SURFACE_TEST: eglChooseConfig failed 0x%x\n", eglGetError());
		return NULL;
	}
	surface = ANativeWindow_createEGLSurface(test->window, display, config, NULL);
	if (surface == EGL_NO_SURFACE) {
		fprintf(stderr, "ATL_SURFACE_TEST: eglCreateWindowSurface failed 0x%x\n", eglGetError());
		return NULL;
	}
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrs);
	if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) {
		fprintf(stderr, "ATL_SURFACE_TEST: context/makeCurrent failed 0x%x\n", eglGetError());
		return NULL;
	}
	fprintf(stderr, "ATL_SURFACE_TEST: presenting on the layer's EGLSurface\n");
	/* atl_native_window_detach() clears the window on the way out, which is
	 * this loop's cue to stop before the wl_egl_window is destroyed */
	while (test->window->egl_window) {
		float phase = (frames % 120) / 120.0f;

		glClearColor(1.0f - phase, 0.1f, phase, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		if (!eglSwapBuffers(display, surface)) {
			fprintf(stderr, "ATL_SURFACE_TEST: eglSwapBuffers failed 0x%x\n", eglGetError());
			return NULL;
		}
		if (++frames % 60 == 0)
			fprintf(stderr, "ATL_SURFACE_TEST: %u frames\n", frames);
		usleep(8000);
	}
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(display, surface);
	eglDestroyContext(display, context);
	ANativeWindow_release(test->window);
	free(test);
	return NULL;
}

JNIEXPORT void JNICALL Java_android_view_SurfaceView_native_1startTestClient(JNIEnv *env, jobject this, jobject surface)
{
	struct surface_test *test;
	pthread_t thread;

	if (!getenv("ATL_SURFACE_TEST"))
		return;
	test = calloc(1, sizeof(*test));
	if (!test)
		return;
	test->window = ANativeWindow_fromSurface(env, surface);
	if (!test->window || !test->window->egl_window) {
		fprintf(stderr, "ATL_SURFACE_TEST: no native window behind this surface\n");
		free(test);
		return;
	}
	fprintf(stderr, "ATL_SURFACE_TEST: ANativeWindow %p, %dx%d, wl_egl_window %p\n",
	        (void *)test->window, ANativeWindow_getWidth(test->window),
	        ANativeWindow_getHeight(test->window), (void *)test->window->egl_window);
	pthread_create(&thread, NULL, surface_test_thread, test);
}
