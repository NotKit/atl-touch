#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "../defines.h"

#include "camera_backend.h"

#include "../generated_headers/android_hardware_Camera.h"

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

	if (!backend)
		return 0;
	return _INTPTR(backend->open(camera_id));
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1release(JNIEnv *env, jobject this, jlong camera_ptr)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();

	if (backend && camera_ptr)
		backend->close(_PTR(camera_ptr));
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1startPreview(JNIEnv *env, jobject this, jlong camera_ptr)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();

	if (backend && camera_ptr)
		backend->start_preview(_PTR(camera_ptr));
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1stopPreview(JNIEnv *env, jobject this, jlong camera_ptr)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();

	if (backend && camera_ptr)
		backend->stop_preview(_PTR(camera_ptr));
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
	const struct atl_camera_backend *backend = atl_camera_backend_get();
	const struct atl_camera_caps *caps;
	int i;

	if (!backend || !camera_ptr)
		return NULL;
	caps = backend->get_caps(_PTR(camera_ptr));
	if (!caps || caps->n_preview_sizes < 1 || caps->n_picture_sizes < 1 || caps->n_fps_ranges < 1) {
		fprintf(stderr, "Camera: backend '%s' reported unusable caps\n", backend->name);
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
	const struct atl_camera_backend *backend = atl_camera_backend_get();
	struct atl_camera *camera = _PTR(camera_ptr);

	if (!backend || !camera)
		return JNI_FALSE;
	if (!backend->set_preview_size(camera, width, height))
		return JNI_FALSE;
	if (!backend->set_preview_format(camera, format))
		return JNI_FALSE;
	if (fps_max > 0 && !backend->set_fps_range(camera, fps_min, fps_max))
		return JNI_FALSE;
	return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1setDisplayOrientation(JNIEnv *env, jobject this, jlong camera_ptr, jint degrees)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();

	if (backend && camera_ptr)
		backend->set_display_orientation(_PTR(camera_ptr), degrees);
}
