#ifndef ATL_CAMERA_BACKEND_H
#define ATL_CAMERA_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Pluggable camera backend behind android.hardware.Camera (Camera1),
 * modeled on media/codec_backend.h.
 *
 * Backends:
 *  - "gst" (camera_backend_gst.c): GStreamer source, videotestsrc by default
 *    (v4l2src/pipewiresrc via ATL_CAMERA_GST_SRC). The desktop backend.
 *  - "hybris" (camera_backend_hybris.c): libhybris libcamera_compat_layer,
 *    dlopen'd at runtime. The Ubuntu Touch device backend.
 *  - "none": no cameras; also the result of ATL_UGLY_ENABLE_CAMERA being unset.
 */

struct atl_camera; /* opaque per-session handle, owned by the backend */

#define ATL_CAMERA_FACING_BACK  0
#define ATL_CAMERA_FACING_FRONT 1

/* android.graphics.ImageFormat values */
#define ATL_CAMERA_FORMAT_NV21 17

/* backend death mid-preview, maps to Camera.CAMERA_ERROR_SERVER_DIED */
#define ATL_CAMERA_ERROR_SERVER_DIED 100

struct atl_camera_size {
	int width;
	int height;
};

/* fps scaled by 1000, matching Camera.Parameters (30 fps = 30000) */
struct atl_camera_fps_range {
	int min;
	int max;
};

/* Returned by get_caps; arrays are owned by the backend and stay valid
 * until close(). */
struct atl_camera_caps {
	const struct atl_camera_size *preview_sizes;
	int n_preview_sizes;
	const struct atl_camera_size *picture_sizes;
	int n_picture_sizes;
	const struct atl_camera_fps_range *fps_ranges;
	int n_fps_ranges;
	/* comma-joined Camera.Parameters value strings ("fixed,infinity");
	 * NULL means unsupported and the parameter key is omitted */
	const char *focus_modes;
	const char *flash_modes;
	const char *scene_modes;
	const char *white_balance_modes;
	const char *color_effects;
	const char *antibanding_modes;
	bool zoom_supported;
	int max_zoom; /* 0 when zoom is unsupported */
	/* lens field of view in degrees; 0 means unknown */
	float horizontal_view_angle;
	float vertical_view_angle;
	/* exposure compensation index range and EV step; all zero = unsupported */
	int min_exposure_compensation;
	int max_exposure_compensation;
	float exposure_compensation_step;
	int max_num_focus_areas;
	int max_num_metering_areas;
	int max_num_detected_faces;
	bool video_snapshot_supported;
	bool video_stabilization_supported;
};

/* Preview frame in NV21 layout (Y plane, then interleaved VU); stride is the
 * Y-plane row stride in bytes. Called on a backend thread; the buffer is only
 * valid for the duration of the call. */
typedef void (*atl_camera_frame_cb)(const uint8_t *nv21, int width, int height,
                                    int stride, void *user);
typedef void (*atl_camera_jpeg_cb)(const uint8_t *jpeg, size_t size, void *user);
typedef void (*atl_camera_autofocus_cb)(bool success, void *user);
typedef void (*atl_camera_error_cb)(int error, void *user);
/* a new frame is waiting in the backend's preview texture; the consumer has to
 * call update_preview_texture() from its GL thread */
typedef void (*atl_camera_texture_cb)(void *user);

struct atl_camera_backend {
	const char *name;

	int (*get_camera_count)(void);
	/* facing: ATL_CAMERA_FACING_*; orientation: 0/90/180/270 degrees */
	bool (*get_camera_info)(int id, int *facing, int *orientation);

	/* NULL on failure */
	struct atl_camera *(*open)(int id);
	void (*close)(struct atl_camera *camera);

	const struct atl_camera_caps *(*get_caps)(struct atl_camera *camera);

	bool (*set_preview_size)(struct atl_camera *camera, int width, int height);
	/* format: ATL_CAMERA_FORMAT_* (only NV21 for now) */
	bool (*set_preview_format)(struct atl_camera *camera, int format);
	bool (*set_fps_range)(struct atl_camera *camera, int min, int max);

	/* cb = NULL unsets; must be safe to call while preview runs */
	void (*set_frame_callback)(struct atl_camera *camera, atl_camera_frame_cb cb, void *user);
	void (*set_error_callback)(struct atl_camera *camera, atl_camera_error_cb cb, void *user);

	bool (*start_preview)(struct atl_camera *camera);
	void (*stop_preview)(struct atl_camera *camera);

	/* One-shot capture at the given picture size; stops preview first
	 * (AOSP semantics: startPreview() is needed to resume). */
	bool (*take_picture)(struct atl_camera *camera, int width, int height,
	                     int jpeg_quality, atl_camera_jpeg_cb cb, void *user);

	void (*autofocus)(struct atl_camera *camera, atl_camera_autofocus_cb cb, void *user);
	void (*cancel_autofocus)(struct atl_camera *camera);

	void (*set_display_orientation)(struct atl_camera *camera, int degrees);

	/* Optional Camera.Parameters passthrough; NULL when the backend has no
	 * such control. Mode strings are the AOSP parameter values
	 * ("auto", "torch", ...); unknown ones are ignored by the backend. */
	void (*set_zoom)(struct atl_camera *camera, int zoom);
	void (*set_focus_mode)(struct atl_camera *camera, const char *mode);
	void (*set_flash_mode)(struct atl_camera *camera, const char *mode);

	/* Optional hardware preview-texture path: the backend renders preview
	 * frames straight into the app's GL texture, bypassing the NV21 upload.
	 * update_preview_texture() runs on the app's GL thread and both binds and
	 * updates tex_name; false means the fast path is unusable and the caller
	 * falls back to the frame-callback path. NULL when unsupported. */
	bool (*update_preview_texture)(struct atl_camera *camera, unsigned tex_name);
	bool (*get_preview_texture_transform)(struct atl_camera *camera, float matrix[16]);
	void (*set_texture_callback)(struct atl_camera *camera, atl_camera_texture_cb cb, void *user);
};

/* The active backend: ATL_CAMERA_BACKEND env var selects by name (gst, hybris,
 * none), default auto (hybris if it loads, else gst). Returns NULL when no
 * backend is available or ATL_UGLY_ENABLE_CAMERA is unset; callers then
 * report zero cameras. */
const struct atl_camera_backend *atl_camera_backend_get(void);

/* camera_backend_gst.c; NULL when GStreamer fails to initialize */
const struct atl_camera_backend *atl_camera_backend_gst_get(void);

/* camera_backend_hybris.c; NULL when the libhybris camera compat layer cannot
 * be dlopen'd (i.e. everywhere but a Halium device) */
const struct atl_camera_backend *atl_camera_backend_hybris_get(void);

#endif
