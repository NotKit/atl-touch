#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"

#include "camera_backend.h"
#include "camera_callbacks.h"
#include "camera_events.h"
#include "camera_preview.h"
#include "surface_texture.h"

#include "../generated_headers/android_hardware_Camera.h"

/* what Camera.nativePtr points at: the backend session plus the JNI-side
 * state hanging off it */
struct atl_camera_jni {
	const struct atl_camera_backend *backend;
	struct atl_camera *camera;
	struct atl_camera_preview *preview;
	struct atl_camera_callbacks *callbacks;
	struct atl_camera_events *events;

	GMutex texture_lock; /* setPreviewTexture races the backend thread */
	struct atl_surface_texture *texture;
};

/* the single backend frame callback, fanned out to every consumer */
static void on_frame(const uint8_t *nv21, int width, int height, int stride, void *user)
{
	struct atl_camera_jni *camera = user;

	atl_camera_preview_submit(camera->preview, nv21, width, height, stride);
	atl_camera_callbacks_submit(camera->callbacks, nv21, width, height, stride);

	/* hold a reference of our own: the app may release the SurfaceTexture
	 * while this frame is being handed over */
	g_mutex_lock(&camera->texture_lock);
	struct atl_surface_texture *texture = camera->texture;
	if (texture)
		atl_surface_texture_ref(texture);
	g_mutex_unlock(&camera->texture_lock);
	if (texture) {
		atl_surface_texture_submit(texture, nv21, width, height, stride);
		atl_surface_texture_unref(texture);
	}
}

/* backend threads: everything below only queues work for the main loop */
static void on_jpeg(const uint8_t *jpeg, size_t size, void *user)
{
	struct atl_camera_jni *camera = user;

	atl_camera_events_post_picture(camera->events, jpeg, size);
}

static void on_autofocus(bool success, void *user)
{
	struct atl_camera_jni *camera = user;

	atl_camera_events_post_autofocus(camera->events, success);
}

static void on_error(int error, void *user)
{
	struct atl_camera_jni *camera = user;

	atl_camera_events_post_error(camera->events, error);
}

JNIEXPORT jint JNICALL Java_android_hardware_Camera_native_1getNumberOfCameras(JNIEnv *env, jclass class)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();

	if (!backend)
		return 0;
	return backend->get_camera_count();
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1getCameraInfo(JNIEnv *env, jclass class, jint camera_id, jobject info)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();
	int facing = ATL_CAMERA_FACING_BACK;
	int orientation = 0;

	if (backend && backend->get_camera_info(camera_id, &facing, &orientation)) {
		_SET_INT_FIELD(info, "facing", facing);
		_SET_INT_FIELD(info, "orientation", orientation);
	}
}

JNIEXPORT jlong JNICALL Java_android_hardware_Camera_native_1open(JNIEnv *env, jobject this, jint camera_id)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();
	struct atl_camera *session;

	if (!backend)
		return 0;
	session = backend->open(camera_id);
	if (!session)
		return 0;

	struct atl_camera_jni *camera = calloc(1, sizeof(*camera));
	camera->backend = backend;
	camera->camera = session;
	g_mutex_init(&camera->texture_lock);
	camera->preview = atl_camera_preview_new(env);
	camera->callbacks = atl_camera_callbacks_new(env, this);
	camera->events = atl_camera_events_new(env, this);
	backend->set_frame_callback(session, on_frame, camera);
	backend->set_error_callback(session, on_error, camera);
	return _INTPTR(camera);
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1release(JNIEnv *env, jobject this, jlong camera_ptr)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (!camera)
		return;
	camera->backend->stop_preview(camera->camera);
	camera->backend->set_frame_callback(camera->camera, NULL, NULL);
	camera->backend->set_error_callback(camera->camera, NULL, NULL);
	/* close() first: it joins any capture still in flight, so no backend
	 * thread can reach the state freed below */
	camera->backend->close(camera->camera);
	atl_camera_preview_free(camera->preview, env);
	atl_camera_callbacks_free(camera->callbacks, env);
	atl_camera_events_free(camera->events, env);
	atl_surface_texture_unref(camera->texture);
	g_mutex_clear(&camera->texture_lock);
	free(camera);
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1startPreview(JNIEnv *env, jobject this, jlong camera_ptr)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (camera)
		camera->backend->start_preview(camera->camera);
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1stopPreview(JNIEnv *env, jobject this, jlong camera_ptr)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (camera)
		camera->backend->stop_preview(camera->camera);
}

/* surface: an android.view.Surface, or NULL for setPreviewDisplay(null) */
JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1setPreviewSurface(JNIEnv *env, jobject this, jlong camera_ptr, jobject surface)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (camera)
		atl_camera_preview_set_surface(camera->preview, env, surface);
}

/* surface_texture: an android.graphics.SurfaceTexture, or NULL to stop */
JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1setPreviewTexture(JNIEnv *env, jobject this, jlong camera_ptr,
                                                                              jobject surface_texture)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (!camera)
		return;

	struct atl_surface_texture *texture = atl_surface_texture_from_java(env, surface_texture);

	g_mutex_lock(&camera->texture_lock);
	struct atl_surface_texture *previous = camera->texture;
	camera->texture = texture;
	g_mutex_unlock(&camera->texture_lock);

	atl_surface_texture_unref(previous);
}

/* callback: a Camera.PreviewCallback or NULL; mode: ATL_CAMERA_CB_* */
JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1setPreviewCallback(JNIEnv *env, jobject this, jlong camera_ptr,
                                                                               jobject callback, jint mode)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (camera)
		atl_camera_callbacks_set(camera->callbacks, env, callback, mode);
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1addCallbackBuffer(JNIEnv *env, jobject this, jlong camera_ptr,
                                                                              jbyteArray buffer)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (camera)
		atl_camera_callbacks_add_buffer(camera->callbacks, env, buffer);
}

static void append_size_values(GString *s, const char *key, const struct atl_camera_size *sizes, int n)
{
	g_string_append_printf(s, ";%s=", key);
	for (int i = 0; i < n; i++)
		g_string_append_printf(s, "%s%dx%d", i ? "," : "", sizes[i].width, sizes[i].height);
}

/* Camera.Parameters defaults, flattened AOSP-style (key=value;key=value) */
JNIEXPORT jstring JNICALL Java_android_hardware_Camera_native_1getDefaultParameters(JNIEnv *env, jobject this, jlong camera_ptr)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);
	const struct atl_camera_caps *caps;
	int i;

	if (!camera)
		return NULL;
	caps = camera->backend->get_caps(camera->camera);
	if (!caps || caps->n_preview_sizes < 1 || caps->n_picture_sizes < 1 || caps->n_fps_ranges < 1) {
		fprintf(stderr, "Camera: backend '%s' reported unusable caps\n", camera->backend->name);
		return NULL;
	}

	GString *s = g_string_new(NULL);

	g_string_append_printf(s, "preview-size=%dx%d",
	                       caps->preview_sizes[0].width, caps->preview_sizes[0].height);
	append_size_values(s, "preview-size-values", caps->preview_sizes, caps->n_preview_sizes);

	int best = 0; /* default picture size: largest by area */
	for (i = 1; i < caps->n_picture_sizes; i++)
		if ((long)caps->picture_sizes[i].width * caps->picture_sizes[i].height >
		    (long)caps->picture_sizes[best].width * caps->picture_sizes[best].height)
			best = i;
	g_string_append_printf(s, ";picture-size=%dx%d",
	                       caps->picture_sizes[best].width, caps->picture_sizes[best].height);
	append_size_values(s, "picture-size-values", caps->picture_sizes, caps->n_picture_sizes);

	g_string_append(s, ";preview-format=yuv420sp;preview-format-values=yuv420sp");
	g_string_append(s, ";picture-format=jpeg;picture-format-values=jpeg");

	g_string_append_printf(s, ";preview-fps-range=%d,%d",
	                       caps->fps_ranges[0].min, caps->fps_ranges[0].max);
	g_string_append(s, ";preview-fps-range-values=");
	for (i = 0; i < caps->n_fps_ranges; i++)
		g_string_append_printf(s, "%s(%d,%d)", i ? "," : "",
		                       caps->fps_ranges[i].min, caps->fps_ranges[i].max);

	g_string_append_printf(s, ";preview-frame-rate=%d", caps->fps_ranges[0].max / 1000);
	g_string_append(s, ";preview-frame-rate-values=");
	int n_rates = 0;
	for (i = 0; i < caps->n_fps_ranges; i++) {
		int rate = caps->fps_ranges[i].max / 1000, j;
		for (j = 0; j < i; j++)
			if (caps->fps_ranges[j].max / 1000 == rate)
				break;
		if (j == i)
			g_string_append_printf(s, "%s%d", n_rates++ ? "," : "", rate);
	}

	if (caps->focus_modes) {
		const char *comma = strchr(caps->focus_modes, ',');
		g_string_append_printf(s, ";focus-mode=%.*s;focus-mode-values=%s",
		                       comma ? (int)(comma - caps->focus_modes) : (int)strlen(caps->focus_modes),
		                       caps->focus_modes, caps->focus_modes);
	}
	if (caps->flash_modes) {
		const char *comma = strchr(caps->flash_modes, ',');
		g_string_append_printf(s, ";flash-mode=%.*s;flash-mode-values=%s",
		                       comma ? (int)(comma - caps->flash_modes) : (int)strlen(caps->flash_modes),
		                       caps->flash_modes, caps->flash_modes);
	}

	g_string_append_printf(s, ";zoom-supported=%s;max-zoom=%d;zoom=0",
	                       caps->zoom_supported ? "true" : "false", caps->max_zoom);
	g_string_append(s, ";jpeg-quality=85;rotation=0");

	jstring result = _JSTRING(s->str);
	g_string_free(s, TRUE);
	return result;
}

JNIEXPORT jboolean JNICALL Java_android_hardware_Camera_native_1setParameters(JNIEnv *env, jobject this, jlong camera_ptr,
                                                                              jint width, jint height, jint format,
                                                                              jint fps_min, jint fps_max)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (!camera)
		return JNI_FALSE;
	if (!camera->backend->set_preview_size(camera->camera, width, height))
		return JNI_FALSE;
	if (!camera->backend->set_preview_format(camera->camera, format))
		return JNI_FALSE;
	if (fps_max > 0 && !camera->backend->set_fps_range(camera->camera, fps_min, fps_max))
		return JNI_FALSE;
	return JNI_TRUE;
}

/* the picture lands later on Camera.dispatchPictureTaken, off the main loop */
JNIEXPORT jboolean JNICALL Java_android_hardware_Camera_native_1takePicture(JNIEnv *env, jobject this, jlong camera_ptr,
                                                                            jint width, jint height, jint jpeg_quality)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (!camera)
		return JNI_FALSE;
	if (!camera->backend->take_picture(camera->camera, width, height, jpeg_quality, on_jpeg, camera))
		return JNI_FALSE;
	/* AOSP fires the shutter as the frame is grabbed; both callbacks go
	 * through the same idle queue, so this keeps shutter -> jpeg ordering */
	atl_camera_events_post_shutter(camera->events);
	return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1autoFocus(JNIEnv *env, jobject this, jlong camera_ptr)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (camera)
		camera->backend->autofocus(camera->camera, on_autofocus, camera);
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1cancelAutoFocus(JNIEnv *env, jobject this, jlong camera_ptr)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (camera)
		camera->backend->cancel_autofocus(camera->camera);
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1setDisplayOrientation(JNIEnv *env, jobject this, jlong camera_ptr, jint degrees)
{
	struct atl_camera_jni *camera = _PTR(camera_ptr);

	if (camera)
		camera->backend->set_display_orientation(camera->camera, degrees);
}
