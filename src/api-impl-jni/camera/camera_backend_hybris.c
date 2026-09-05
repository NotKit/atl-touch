/*
 * libhybris camera backend: the Ubuntu Touch device camera.
 *
 * Everything goes through the libhybris camera compat layer (libcamera.so.1),
 * which is dlopen'd — there is no build-time hybris dependency, and on a
 * desktop the dlopen simply fails and the gst backend takes over. The vendored
 * headers under hybris/ are the compat layer's public API.
 *
 * Preview frames arrive as NV21 on a binder thread through on_preview_frame_cb
 * and feed the same path as the gst backend. When the app draws through a
 * SurfaceTexture there is also a zero-copy path: the HAL renders into the app's
 * GL texture and only tells us when to call update_preview_texture (which must
 * happen on the GL thread) — see update_preview_texture() below.
 */

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "camera_backend.h"
#include "camera_frame.h"
#include "hybris/camera_compatibility_layer.h"
#include "hybris/camera_compatibility_layer_capabilities.h"

/* the compat layer entry points we use, all resolved by dlsym */
static struct {
	void *handle;

	int (*get_number_of_devices)(void);
	int (*get_device_info)(int32_t id, int *facing, int *orientation);
	struct CameraControl *(*connect_by_id)(int32_t id, struct CameraControlListener *listener);
	void (*disconnect)(struct CameraControl *control);
	void (*delete_control)(struct CameraControl *control);

	void (*start_preview)(struct CameraControl *control);
	void (*stop_preview)(struct CameraControl *control);
	int (*set_preview_callback_mode)(struct CameraControl *control, PreviewCallbackMode mode);
	void (*set_preview_size)(struct CameraControl *control, int width, int height);
	void (*set_preview_format)(struct CameraControl *control, CameraPixelFormat format);
	void (*set_preview_fps)(struct CameraControl *control, int fps);
	void (*get_preview_size)(struct CameraControl *control, int *width, int *height);
	void (*get_preview_fps_range)(struct CameraControl *control, int *min, int *max);
	void (*enumerate_supported_preview_sizes)(struct CameraControl *control, size_callback cb, void *ctx);
	void (*enumerate_supported_picture_sizes)(struct CameraControl *control, size_callback cb, void *ctx);
	void (*enumerate_supported_flash_modes)(struct CameraControl *control, flash_mode_callback cb, void *ctx);
	void (*enumerate_supported_scene_modes)(struct CameraControl *control, scene_mode_callback cb, void *ctx);

	void (*set_picture_size)(struct CameraControl *control, int width, int height);
	void (*set_jpeg_quality)(struct CameraControl *control, int quality);
	void (*take_snapshot)(struct CameraControl *control);

	void (*start_autofocus)(struct CameraControl *control);
	void (*stop_autofocus)(struct CameraControl *control);
	void (*set_auto_focus_mode)(struct CameraControl *control, AutoFocusMode mode);
	void (*set_flash_mode)(struct CameraControl *control, FlashMode mode);
	void (*set_zoom)(struct CameraControl *control, int32_t zoom);
	void (*get_max_zoom)(struct CameraControl *control, int *max_zoom);
	void (*set_display_orientation)(struct CameraControl *control, int32_t degrees);

	void (*set_preview_texture)(struct CameraControl *control, int texture_id);
	void (*update_preview_texture)(struct CameraControl *control);
	void (*get_preview_texture_transformation)(struct CameraControl *control, float m[16]);
} hy;

struct atl_camera {
	struct CameraControl *control;
	struct CameraControlListener listener;

	int width;
	int height;
	int fps_max; /* scaled by 1000, like Camera.Parameters */
	bool previewing;

	GMutex lock;
	atl_camera_frame_cb frame_cb;
	void *frame_user;
	atl_camera_error_cb error_cb;
	void *error_user;
	atl_camera_autofocus_cb autofocus_cb;
	void *autofocus_user;
	atl_camera_jpeg_cb jpeg_cb;
	void *jpeg_user;
	atl_camera_texture_cb texture_cb;
	void *texture_user;

	unsigned texture_id; /* the GL texture the HAL renders into, 0 = none */

	/* caps, owned here and alive until close() */
	GArray *preview_sizes;
	GArray *picture_sizes;
	GArray *fps_ranges;
	GString *flash_modes;
	GString *scene_modes;
	struct atl_camera_caps caps;

	uint64_t frame_count;
	uint64_t texture_frame_count;
	char *dump_dir;
};

/*
 * The compat layer is one library among several called "libcamera": the
 * freedesktop libcamera has the same soname. Resolving a compat-layer-only
 * symbol is what tells them apart.
 */
static bool hybris_load(void)
{
	static bool tried;
	static bool ok;
	static GMutex load_lock;

	g_mutex_lock(&load_lock);
	if (tried)
		goto out;
	tried = true;

	const char *soname = getenv("ATL_CAMERA_HYBRIS_LIB");
	if (!soname || !*soname)
		soname = "libcamera.so.1";

	hy.handle = dlopen(soname, RTLD_NOW | RTLD_LOCAL);
	if (!hy.handle && !getenv("ATL_CAMERA_HYBRIS_LIB"))
		hy.handle = dlopen("libcamera.so", RTLD_NOW | RTLD_LOCAL);
	if (!hy.handle) {
		fprintf(stderr, "Camera hybris: dlopen(%s) failed: %s\n", soname, dlerror());
		goto out;
	}

#define SYM(field, name) hy.field = dlsym(hy.handle, name)
	SYM(get_number_of_devices, "android_camera_get_number_of_devices");
	SYM(get_device_info, "android_camera_get_device_info");
	SYM(connect_by_id, "android_camera_connect_by_id");
	SYM(disconnect, "android_camera_disconnect");
	SYM(delete_control, "android_camera_delete");
	SYM(start_preview, "android_camera_start_preview");
	SYM(stop_preview, "android_camera_stop_preview");
	SYM(set_preview_callback_mode, "android_camera_set_preview_callback_mode");
	SYM(set_preview_size, "android_camera_set_preview_size");
	SYM(set_preview_format, "android_camera_set_preview_format");
	SYM(set_preview_fps, "android_camera_set_preview_fps");
	SYM(get_preview_size, "android_camera_get_preview_size");
	SYM(get_preview_fps_range, "android_camera_get_preview_fps_range");
	SYM(enumerate_supported_preview_sizes, "android_camera_enumerate_supported_preview_sizes");
	SYM(enumerate_supported_picture_sizes, "android_camera_enumerate_supported_picture_sizes");
	SYM(enumerate_supported_flash_modes, "android_camera_enumerate_supported_flash_modes");
	SYM(enumerate_supported_scene_modes, "android_camera_enumerate_supported_scene_modes");
	SYM(set_picture_size, "android_camera_set_picture_size");
	SYM(set_jpeg_quality, "android_camera_set_jpeg_quality");
	SYM(take_snapshot, "android_camera_take_snapshot");
	SYM(start_autofocus, "android_camera_start_autofocus");
	SYM(stop_autofocus, "android_camera_stop_autofocus");
	SYM(set_auto_focus_mode, "android_camera_set_auto_focus_mode");
	SYM(set_flash_mode, "android_camera_set_flash_mode");
	SYM(set_zoom, "android_camera_set_zoom");
	SYM(get_max_zoom, "android_camera_get_max_zoom");
	SYM(set_display_orientation, "android_camera_set_display_orientation");
	SYM(set_preview_texture, "android_camera_set_preview_texture");
	SYM(update_preview_texture, "android_camera_update_preview_texture");
	SYM(get_preview_texture_transformation, "android_camera_get_preview_texture_transformation");
#undef SYM

	/* the minimum needed to run a preview; the rest degrade gracefully */
	if (!hy.get_number_of_devices || !hy.connect_by_id || !hy.start_preview ||
	    !hy.stop_preview || !hy.set_preview_callback_mode) {
		fprintf(stderr, "Camera hybris: %s is not the libhybris camera compat layer "
		                "(android_camera_* symbols missing)\n", soname);
		dlclose(hy.handle);
		hy.handle = NULL;
		goto out;
	}

	fprintf(stderr, "Camera hybris: loaded %s\n", soname);
	ok = true;
out:
	g_mutex_unlock(&load_lock);
	return ok;
}

/* --- callbacks from the HAL, on binder threads --------------------------- */

static void hybris_on_preview_frame(void *data, uint32_t size, void *context)
{
	struct atl_camera *camera = context;

	g_mutex_lock(&camera->lock);
	int width = camera->width;
	int height = camera->height;
	atl_camera_frame_cb cb = camera->frame_cb;
	void *cb_user = camera->frame_user;
	g_mutex_unlock(&camera->lock);

	/* the HAL delivers whatever size it settled on, not necessarily the one we
	 * asked for; a mismatch means our idea of the preview size is stale */
	if (size < (size_t)width * height * 3 / 2) {
		if (!hy.get_preview_size)
			return;
		hy.get_preview_size(camera->control, &width, &height);
		if (size < (size_t)width * height * 3 / 2) {
			fprintf(stderr, "Camera hybris: dropping a %u byte frame, too small for %dx%d\n",
			        size, width, height);
			return;
		}
		g_mutex_lock(&camera->lock);
		camera->width = width;
		camera->height = height;
		g_mutex_unlock(&camera->lock);
	}

	camera->frame_count++;
	if (camera->frame_count == 1)
		fprintf(stderr, "Camera hybris: first frame (%dx%d, %u bytes)\n", width, height, size);

	if (cb)
		cb(data, width, height, width, cb_user);
	atl_camera_dump_frame(camera->dump_dir, camera->frame_count, data, width, height, width);
}

static void hybris_on_msg_error(void *context)
{
	struct atl_camera *camera = context;

	fprintf(stderr, "Camera hybris: the camera HAL reported an error\n");
	g_mutex_lock(&camera->lock);
	atl_camera_error_cb cb = camera->error_cb;
	void *cb_user = camera->error_user;
	g_mutex_unlock(&camera->lock);
	if (cb)
		cb(ATL_CAMERA_ERROR_SERVER_DIED, cb_user);
}

static void hybris_on_msg_focus(void *context)
{
	struct atl_camera *camera = context;

	g_mutex_lock(&camera->lock);
	atl_camera_autofocus_cb cb = camera->autofocus_cb;
	void *cb_user = camera->autofocus_user;
	camera->autofocus_cb = NULL;
	g_mutex_unlock(&camera->lock);
	/* the compat layer has no failure message: reaching here is a focus lock */
	if (cb)
		cb(true, cb_user);
}

static void hybris_on_data_compressed_image(void *data, uint32_t size, void *context)
{
	struct atl_camera *camera = context;

	g_mutex_lock(&camera->lock);
	atl_camera_jpeg_cb cb = camera->jpeg_cb;
	void *cb_user = camera->jpeg_user;
	camera->jpeg_cb = NULL;
	/* AOSP: take_snapshot stops the preview, startPreview() resumes it */
	camera->previewing = false;
	g_mutex_unlock(&camera->lock);

	fprintf(stderr, "Camera hybris: captured a picture, %u bytes of JPEG\n", size);
	if (cb)
		cb(data, size, cb_user);
}

static void hybris_on_preview_texture_needs_update(void *context)
{
	struct atl_camera *camera = context;

	if (++camera->texture_frame_count == 1)
		fprintf(stderr, "Camera hybris: first preview texture frame\n");

	g_mutex_lock(&camera->lock);
	atl_camera_texture_cb cb = camera->texture_cb;
	void *cb_user = camera->texture_user;
	g_mutex_unlock(&camera->lock);
	if (cb)
		cb(cb_user);
}

/* --- capabilities -------------------------------------------------------- */

static void collect_size(void *ctx, int width, int height)
{
	GArray *sizes = ctx;
	struct atl_camera_size size = {width, height};

	for (guint i = 0; i < sizes->len; i++) {
		struct atl_camera_size *have = &g_array_index(sizes, struct atl_camera_size, i);
		if (have->width == width && have->height == height)
			return;
	}
	g_array_append_val(sizes, size);
}

/* append to a comma-joined Camera.Parameters value list, skipping duplicates
 * (a HAL may enumerate the same mode twice) */
static void append_mode(GString *s, const char *mode)
{
	for (const char *p = s->str; *p; p = strchr(p, ',') + 1) {
		size_t len = strcspn(p, ",");
		if (len == strlen(mode) && !strncmp(p, mode, len))
			return;
		if (!p[len])
			break;
	}
	if (s->len)
		g_string_append_c(s, ',');
	g_string_append(s, mode);
}

static void collect_flash_mode(void *ctx, FlashMode mode)
{
	static const char *const names[] = {"off", "auto", "on", "torch", "red-eye"};

	if (mode >= 0 && mode < (int)G_N_ELEMENTS(names))
		append_mode(ctx, names[mode]);
}

static void collect_scene_mode(void *ctx, SceneMode mode)
{
	static const char *const names[] = {"auto", "action", "night", "party", "sunset", "hdr"};

	if (mode >= 0 && mode < (int)G_N_ELEMENTS(names))
		append_mode(ctx, names[mode]);
}

/* fall back to whatever the HAL currently has when it enumerates nothing */
static void ensure_one_size(struct atl_camera *camera, GArray *sizes)
{
	struct atl_camera_size size = {camera->width, camera->height};

	if (sizes->len)
		return;
	g_array_append_val(sizes, size);
}

static void query_caps(struct atl_camera *camera)
{
	camera->preview_sizes = g_array_new(FALSE, FALSE, sizeof(struct atl_camera_size));
	camera->picture_sizes = g_array_new(FALSE, FALSE, sizeof(struct atl_camera_size));
	camera->fps_ranges = g_array_new(FALSE, FALSE, sizeof(struct atl_camera_fps_range));
	camera->flash_modes = g_string_new(NULL);
	camera->scene_modes = g_string_new(NULL);

	if (hy.get_preview_size)
		hy.get_preview_size(camera->control, &camera->width, &camera->height);
	if (camera->width < 1 || camera->height < 1) {
		camera->width = 640;
		camera->height = 480;
	}

	if (hy.enumerate_supported_preview_sizes)
		hy.enumerate_supported_preview_sizes(camera->control, collect_size, camera->preview_sizes);
	if (hy.enumerate_supported_picture_sizes)
		hy.enumerate_supported_picture_sizes(camera->control, collect_size, camera->picture_sizes);
	ensure_one_size(camera, camera->preview_sizes);
	ensure_one_size(camera, camera->picture_sizes);

	struct atl_camera_fps_range fps = {30000, 30000};
	if (hy.get_preview_fps_range) {
		int min = 0, max = 0;
		hy.get_preview_fps_range(camera->control, &min, &max);
		/* Camera.Parameters units are fps*1000, but not every HAL agrees */
		if (max > 0) {
			fps.min = min < 1000 ? min * 1000 : min;
			fps.max = max < 1000 ? max * 1000 : max;
		}
	}
	g_array_append_val(camera->fps_ranges, fps);
	camera->fps_max = fps.max;

	if (hy.enumerate_supported_flash_modes)
		hy.enumerate_supported_flash_modes(camera->control, collect_flash_mode, camera->flash_modes);
	if (hy.enumerate_supported_scene_modes)
		hy.enumerate_supported_scene_modes(camera->control, collect_scene_mode, camera->scene_modes);

	int max_zoom = 0;
	if (hy.get_max_zoom)
		hy.get_max_zoom(camera->control, &max_zoom);

	camera->caps = (struct atl_camera_caps){
		.preview_sizes = (const struct atl_camera_size *)camera->preview_sizes->data,
		.n_preview_sizes = camera->preview_sizes->len,
		.picture_sizes = (const struct atl_camera_size *)camera->picture_sizes->data,
		.n_picture_sizes = camera->picture_sizes->len,
		.fps_ranges = (const struct atl_camera_fps_range *)camera->fps_ranges->data,
		.n_fps_ranges = camera->fps_ranges->len,
		/* the compat layer enumerates no focus modes: advertise the ones its
		 * AutoFocusMode enum can express */
		.focus_modes = hy.set_auto_focus_mode ? "auto,infinity,macro,continuous-video,continuous-picture" : NULL,
		.flash_modes = camera->flash_modes->len ? camera->flash_modes->str : NULL,
		.scene_modes = camera->scene_modes->len ? camera->scene_modes->str : NULL,
		.white_balance_modes = NULL,
		.color_effects = NULL,
		.antibanding_modes = NULL,
		.zoom_supported = max_zoom > 0 && hy.set_zoom,
		.max_zoom = hy.set_zoom ? max_zoom : 0,
		.horizontal_view_angle = 0.0f,
		.vertical_view_angle = 0.0f,
		.min_exposure_compensation = 0,
		.max_exposure_compensation = 0,
		.exposure_compensation_step = 0.0f,
		.max_num_focus_areas = 0,
		.max_num_metering_areas = 0,
		.max_num_detected_faces = 0,
		.video_snapshot_supported = false,
		.video_stabilization_supported = false,
	};
}

/* --- backend vtable ------------------------------------------------------ */

static int hybris_get_count(void)
{
	if (!hybris_load())
		return 0;
	return hy.get_number_of_devices();
}

static bool hybris_get_info(int id, int *facing, int *orientation)
{
	if (!hybris_load() || !hy.get_device_info)
		return false;
	/* the compat layer passes Android's CameraInfo values straight through */
	return hy.get_device_info(id, facing, orientation) == 0;
}

static void hybris_close(struct atl_camera *camera);

static struct atl_camera *hybris_open(int id)
{
	if (!hybris_load())
		return NULL;

	struct atl_camera *camera = calloc(1, sizeof(*camera));
	camera->width = 640;
	camera->height = 480;
	camera->fps_max = 30000;
	g_mutex_init(&camera->lock);

	const char *dump = getenv("ATL_CAMERA_DUMP_FRAMES");
	if (dump && *dump) {
		g_mkdir_with_parents(dump, 0755);
		camera->dump_dir = g_strdup(dump);
	}

	camera->listener = (struct CameraControlListener){
		.on_msg_error_cb = hybris_on_msg_error,
		.on_msg_focus_cb = hybris_on_msg_focus,
		.on_data_compressed_image_cb = hybris_on_data_compressed_image,
		.on_preview_texture_needs_update_cb = hybris_on_preview_texture_needs_update,
		.on_preview_frame_cb = hybris_on_preview_frame,
		.context = camera,
	};

	camera->control = hy.connect_by_id(id, &camera->listener);
	if (!camera->control) {
		fprintf(stderr, "Camera hybris: failed to connect to camera %d\n", id);
		hybris_close(camera);
		return NULL;
	}

	query_caps(camera);
	fprintf(stderr, "Camera hybris: opened camera %d (%d preview sizes, %d picture sizes)\n",
	        id, camera->caps.n_preview_sizes, camera->caps.n_picture_sizes);
	return camera;
}

static void hybris_stop_preview(struct atl_camera *camera);

static void hybris_close(struct atl_camera *camera)
{
	if (camera->control) {
		hybris_stop_preview(camera);
		if (hy.set_preview_callback_mode)
			hy.set_preview_callback_mode(camera->control, PREVIEW_CALLBACK_DISABLED);
		if (hy.disconnect)
			hy.disconnect(camera->control);
		if (hy.delete_control)
			hy.delete_control(camera->control);
	}
	if (camera->preview_sizes)
		g_array_free(camera->preview_sizes, TRUE);
	if (camera->picture_sizes)
		g_array_free(camera->picture_sizes, TRUE);
	if (camera->fps_ranges)
		g_array_free(camera->fps_ranges, TRUE);
	if (camera->flash_modes)
		g_string_free(camera->flash_modes, TRUE);
	if (camera->scene_modes)
		g_string_free(camera->scene_modes, TRUE);
	g_mutex_clear(&camera->lock);
	g_free(camera->dump_dir);
	free(camera);
}

static const struct atl_camera_caps *hybris_get_caps(struct atl_camera *camera)
{
	return &camera->caps;
}

static bool hybris_set_preview_size(struct atl_camera *camera, int width, int height)
{
	if (width <= 0 || height <= 0 || !hy.set_preview_size)
		return false;
	hy.set_preview_size(camera->control, width, height);
	g_mutex_lock(&camera->lock);
	camera->width = width;
	camera->height = height;
	g_mutex_unlock(&camera->lock);
	return true;
}

static bool hybris_set_preview_format(struct atl_camera *camera, int format)
{
	if (format != ATL_CAMERA_FORMAT_NV21)
		return false;
	/* Android's YUV420SP is NV21 */
	if (hy.set_preview_format)
		hy.set_preview_format(camera->control, CAMERA_PIXEL_FORMAT_YUV420SP);
	return true;
}

static bool hybris_set_fps_range(struct atl_camera *camera, int min, int max)
{
	if (max < 1000)
		return false;
	camera->fps_max = max;
	if (hy.set_preview_fps)
		hy.set_preview_fps(camera->control, max / 1000);
	return true;
}

static void hybris_set_frame_callback(struct atl_camera *camera, atl_camera_frame_cb cb, void *user)
{
	g_mutex_lock(&camera->lock);
	camera->frame_cb = cb;
	camera->frame_user = user;
	g_mutex_unlock(&camera->lock);
}

static void hybris_set_error_callback(struct atl_camera *camera, atl_camera_error_cb cb, void *user)
{
	g_mutex_lock(&camera->lock);
	camera->error_cb = cb;
	camera->error_user = user;
	g_mutex_unlock(&camera->lock);
}

static bool hybris_start_preview(struct atl_camera *camera)
{
	if (camera->previewing)
		return true;
	/* software frames stay on even when the texture fast path runs: the byte
	 * callbacks and the Surface preview path both need them */
	hy.set_preview_callback_mode(camera->control, PREVIEW_CALLBACK_ENABLED);
	hy.start_preview(camera->control);
	camera->previewing = true;
	fprintf(stderr, "Camera hybris: preview started (%dx%d @ %d/1000 fps)\n",
	        camera->width, camera->height, camera->fps_max);
	return true;
}

static void hybris_stop_preview(struct atl_camera *camera)
{
	if (!camera->previewing)
		return;
	camera->previewing = false;
	hy.stop_preview(camera->control);
	fprintf(stderr, "Camera hybris: preview stopped\n");
}

static bool hybris_take_picture(struct atl_camera *camera, int width, int height,
                                int jpeg_quality, atl_camera_jpeg_cb cb, void *user)
{
	if (!cb || !hy.take_snapshot)
		return false;

	g_mutex_lock(&camera->lock);
	bool busy = camera->jpeg_cb != NULL;
	if (!busy) {
		camera->jpeg_cb = cb;
		camera->jpeg_user = user;
	}
	g_mutex_unlock(&camera->lock);
	if (busy) {
		fprintf(stderr, "Camera hybris: a capture is already in flight\n");
		return false;
	}

	if (width > 0 && height > 0 && hy.set_picture_size)
		hy.set_picture_size(camera->control, width, height);
	if (jpeg_quality > 0 && hy.set_jpeg_quality)
		hy.set_jpeg_quality(camera->control, jpeg_quality);
	/* the HAL only snapshots out of a running preview */
	hybris_start_preview(camera);
	hy.take_snapshot(camera->control);
	return true;
}

static void hybris_autofocus(struct atl_camera *camera, atl_camera_autofocus_cb cb, void *user)
{
	if (!hy.start_autofocus) {
		if (cb)
			cb(false, user);
		return;
	}
	g_mutex_lock(&camera->lock);
	camera->autofocus_cb = cb;
	camera->autofocus_user = user;
	g_mutex_unlock(&camera->lock);
	hy.start_autofocus(camera->control);
}

static void hybris_cancel_autofocus(struct atl_camera *camera)
{
	g_mutex_lock(&camera->lock);
	camera->autofocus_cb = NULL;
	g_mutex_unlock(&camera->lock);
	if (hy.stop_autofocus)
		hy.stop_autofocus(camera->control);
}

static void hybris_set_display_orientation(struct atl_camera *camera, int degrees)
{
	if (hy.set_display_orientation)
		hy.set_display_orientation(camera->control, degrees);
}

static void hybris_set_zoom(struct atl_camera *camera, int zoom)
{
	if (hy.set_zoom)
		hy.set_zoom(camera->control, zoom);
}

static void hybris_set_focus_mode(struct atl_camera *camera, const char *mode)
{
	static const struct {
		const char *name;
		AutoFocusMode mode;
	} modes[] = {
		{"auto", AUTO_FOCUS_MODE_AUTO},
		{"infinity", AUTO_FOCUS_MODE_INFINITY},
		{"macro", AUTO_FOCUS_MODE_MACRO},
		{"continuous-video", AUTO_FOCUS_MODE_CONTINUOUS_VIDEO},
		{"continuous-picture", AUTO_FOCUS_MODE_CONTINUOUS_PICTURE},
		{"fixed", AUTO_FOCUS_MODE_OFF},
		{"off", AUTO_FOCUS_MODE_OFF},
	};

	if (!mode || !hy.set_auto_focus_mode)
		return;
	for (size_t i = 0; i < G_N_ELEMENTS(modes); i++)
		if (!strcmp(mode, modes[i].name)) {
			hy.set_auto_focus_mode(camera->control, modes[i].mode);
			return;
		}
}

static void hybris_set_flash_mode(struct atl_camera *camera, const char *mode)
{
	static const struct {
		const char *name;
		FlashMode mode;
	} modes[] = {
		{"off", FLASH_MODE_OFF},
		{"auto", FLASH_MODE_AUTO},
		{"on", FLASH_MODE_ON},
		{"torch", FLASH_MODE_TORCH},
		{"red-eye", FLASH_MODE_RED_EYE},
	};

	if (!mode || !hy.set_flash_mode)
		return;
	for (size_t i = 0; i < G_N_ELEMENTS(modes); i++)
		if (!strcmp(mode, modes[i].name)) {
			hy.set_flash_mode(camera->control, modes[i].mode);
			return;
		}
}

/*
 * Point the camera at the app's GL texture. android_camera_set_preview_texture()
 * is the compat layer's only setPreviewTarget() call, and a camera with no
 * preview target configures no streams: startPreview() then succeeds and
 * delivers nothing, software preview callbacks included. So this has to run
 * before start_preview(), and it deliberately needs no GL context -- only
 * update_preview_texture() below does.
 */
static bool hybris_attach_preview_texture(struct atl_camera *camera, unsigned tex_name)
{
	if (!hy.set_preview_texture || !hy.update_preview_texture)
		return false;
	if (camera->texture_id == tex_name)
		return tex_name != 0;

	/* setPreviewTexture only takes effect on a stopped preview */
	bool was_previewing = camera->previewing;
	if (was_previewing)
		hybris_stop_preview(camera);
	hy.set_preview_texture(camera->control, (int)tex_name);
	camera->texture_id = tex_name;
	if (tex_name)
		fprintf(stderr, "Camera hybris: preview texture fast path on GL texture %u\n", tex_name);
	else
		fprintf(stderr, "Camera hybris: preview texture detached\n");
	if (was_previewing)
		hybris_start_preview(camera);
	return tex_name != 0;
}

/*
 * The GL-thread half of the texture fast path: pull in the newest frame. The
 * HAL binds the texture itself. Attaching here too covers a consumer that moved
 * the texture between GL contexts after setPreviewTexture().
 */
static bool hybris_update_preview_texture(struct atl_camera *camera, unsigned tex_name)
{
	if (!hybris_attach_preview_texture(camera, tex_name))
		return false;
	hy.update_preview_texture(camera->control);
	return true;
}

static bool hybris_get_preview_texture_transform(struct atl_camera *camera, float matrix[16])
{
	if (!hy.get_preview_texture_transformation || !camera->texture_id)
		return false;
	hy.get_preview_texture_transformation(camera->control, matrix);
	return true;
}

static void hybris_set_texture_callback(struct atl_camera *camera, atl_camera_texture_cb cb, void *user)
{
	g_mutex_lock(&camera->lock);
	camera->texture_cb = cb;
	camera->texture_user = user;
	g_mutex_unlock(&camera->lock);
}

static const struct atl_camera_backend hybris_backend = {
	.name = "hybris",
	.get_camera_count = hybris_get_count,
	.get_camera_info = hybris_get_info,
	.open = hybris_open,
	.close = hybris_close,
	.get_caps = hybris_get_caps,
	.set_preview_size = hybris_set_preview_size,
	.set_preview_format = hybris_set_preview_format,
	.set_fps_range = hybris_set_fps_range,
	.set_frame_callback = hybris_set_frame_callback,
	.set_error_callback = hybris_set_error_callback,
	.start_preview = hybris_start_preview,
	.stop_preview = hybris_stop_preview,
	.take_picture = hybris_take_picture,
	.autofocus = hybris_autofocus,
	.cancel_autofocus = hybris_cancel_autofocus,
	.set_display_orientation = hybris_set_display_orientation,
	.set_zoom = hybris_set_zoom,
	.set_focus_mode = hybris_set_focus_mode,
	.set_flash_mode = hybris_set_flash_mode,
	.attach_preview_texture = hybris_attach_preview_texture,
	.update_preview_texture = hybris_update_preview_texture,
	.get_preview_texture_transform = hybris_get_preview_texture_transform,
	.set_texture_callback = hybris_set_texture_callback,
};

const struct atl_camera_backend *atl_camera_backend_hybris_get(void)
{
	if (!hybris_load())
		return NULL;
	return &hybris_backend;
}
