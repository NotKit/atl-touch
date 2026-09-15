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
 *  - "camera2ndk" (camera_backend_camera2ndk.c): the device's own Android
 *    camera2 stack, loaded with libhybris. The Ubuntu Touch device backend.
 *  - "hybris" (camera_backend_hybris.c): libhybris libcamera_compat_layer,
 *    the Camera1 device backend; it serves no camera2.
 *  - "replay" (camera_replay.c): the frames of a recording ATL_CAMERA_RECORD
 *    made on a device, played back on a desktop.
 *  - "none": no cameras; also the result of ATL_UGLY_ENABLE_CAMERA being unset.
 */

struct atl_camera;          /* opaque per-session handle, owned by the backend */
struct atl_camera_metadata; /* camera2_metadata.h */

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
 * valid for the duration of the call. nv21 = NULL means the frame went straight
 * into the app's texture and was never copied to the CPU (the zero-copy path,
 * see set_frame_pixels_needed): the caller can count and time it, but only the
 * texture consumer can see its pixels. */
typedef void (*atl_camera_frame_cb)(const uint8_t *nv21, int width, int height,
                                    int stride, void *user);
typedef void (*atl_camera_jpeg_cb)(const uint8_t *jpeg, size_t size, void *user);
typedef void (*atl_camera_autofocus_cb)(bool success, void *user);
typedef void (*atl_camera_error_cb)(int error, void *user);
/* a new frame is waiting in the backend's preview texture; the consumer has to
 * call update_preview_texture() from its GL thread */
typedef void (*atl_camera_texture_cb)(void *user);


/*
 * camera2 streams, shaped like AOSP's Camera3Device: a session is a set of
 * output streams, one per app Surface, each in its own format and size; a
 * request names the streams it targets; the device answers with a capture
 * started event, one buffer per targeted stream and a result, all carrying the
 * frame's sensor timestamp so the app can pair them. A backend that speaks
 * camera2 itself (camera2ndk) implements these directly; one that has a single
 * preview stream (gst, hybris) leaves them NULL and camera_streams.c emulates
 * them over its NV21 frames.
 */

#define ATL_CAMERA_MAX_STREAMS 8
#define ATL_CAMERA_MAX_PLANES  4

/* android.graphics.ImageFormat values a stream can be in */
#define ATL_CAMERA_FORMAT_RAW_SENSOR  0x20
#define ATL_CAMERA_FORMAT_PRIVATE     0x22
#define ATL_CAMERA_FORMAT_YUV_420_888 0x23
#define ATL_CAMERA_FORMAT_RAW10       0x25
#define ATL_CAMERA_FORMAT_RAW12       0x26
#define ATL_CAMERA_FORMAT_JPEG        0x100

/* android.hardware.HardwareBuffer usage bits a consumer asks for */
#define ATL_CAMERA_USAGE_CPU_READ_OFTEN    3ULL
#define ATL_CAMERA_USAGE_GPU_SAMPLED_IMAGE (1ULL << 8)

/* one output stream, as configured (AOSP: an OutputConfiguration) */
struct atl_camera_stream {
	int width;
	int height;
	int format;              /* ATL_CAMERA_FORMAT_* */
	int max_buffers;         /* the consumer's depth (ImageReader.maxImages) */
	uint64_t usage;          /* ATL_CAMERA_USAGE_* */
	const char *physical_id; /* a physical camera of a logical one, or NULL */
};

struct atl_camera_plane {
	uint8_t *data;
	int len;
	int row_stride;
	int pixel_stride;
};

/*
 * One filled buffer of one stream. The consumer owns it until it calls
 * release(), which hands it back to the producer (the HAL's own buffer queue,
 * or the emulation's pool); the memory behind the planes is valid until then.
 * An opaque (PRIVATE) buffer has no planes, only its native handle.
 */
struct atl_camera_buffer {
	int stream;              /* index into the configured streams */
	int width;
	int height;
	int format;
	int64_t timestamp;       /* the frame's sensor timestamp, in ns */
	int n_planes;
	struct atl_camera_plane planes[ATL_CAMERA_MAX_PLANES];
	/* the bytes planes[0].data spans when the planes are one contiguous run
	 * (an emulated buffer); 0 for a HAL buffer, whose planes are separate */
	size_t size;
	/* the gralloc AHardwareBuffer behind a HAL buffer, NULL on a CPU backend;
	 * a consumer that keeps it past release() takes its own reference */
	void *native;
	void (*release)(struct atl_camera_buffer *buffer);
	void *owner;             /* the producer's own state */
};

/*
 * A reprocessing session's input stream (AOSP: an InputConfiguration). The
 * app writes frames it kept into it and a reprocess request takes the oldest
 * one; every buffer that goes in is a HAL buffer of this camera's, so only a
 * backend with real streams can have one.
 */
struct atl_camera_stream_input {
	int width;
	int height;
	int format;           /* ATL_CAMERA_FORMAT_* */
	bool multi_resolution;
	int max_buffers;      /* how many may be queued at once */
};

/* the camera has consumed a queued input buffer and no longer touches it;
 * called on a backend thread */
typedef void (*atl_camera_input_released_cb)(void *user, struct atl_camera_buffer *buffer);

/* AOSP: ICameraDeviceCallbacks. All of these arrive on backend threads. The
 * result bag is owned by the callee. */
struct atl_camera_stream_callbacks {
	void (*started)(void *user, int request_id, int64_t frame_number, int64_t timestamp);
	void (*result)(void *user, int request_id, int64_t frame_number,
	               struct atl_camera_metadata *result);
	void (*failed)(void *user, int request_id, int64_t frame_number);
	void (*buffer)(void *user, struct atl_camera_buffer *buffer);
	/* one stream's buffer of a frame will never come (a physical camera's
	 * stream while another lens is the active one, for instance) */
	void (*buffer_lost)(void *user, int request_id, int64_t frame_number, int stream);
};

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
	 *
	 * attach_preview_texture() points the camera at tex_name (0 detaches) and
	 * needs no GL context, so it can and must run before start_preview():
	 * a HAL with no preview target produces no frames at all, not even the
	 * software ones the caller would otherwise wait for. False means the fast
	 * path is unusable and the caller falls back to the frame-callback path.
	 *
	 * update_preview_texture() runs on the app's GL thread and both binds and
	 * updates tex_name. NULL when unsupported. */
	bool (*attach_preview_texture)(struct atl_camera *camera, unsigned tex_name);
	bool (*update_preview_texture)(struct atl_camera *camera, unsigned tex_name);
	bool (*get_preview_texture_transform)(struct atl_camera *camera, float matrix[16]);
	void (*set_texture_callback)(struct atl_camera *camera, atl_camera_texture_cb cb, void *user);
	/* Optional: whether anything besides that texture still wants the frame's
	 * pixels. A backend whose fast path is running stops copying them to the
	 * CPU when nothing does, and reports those frames with nv21 = NULL. NULL
	 * when the backend has no such choice; the default is that they are. */
	void (*set_frame_pixels_needed)(struct atl_camera *camera, bool needed);
	/* Optional: the largest frame any consumer of the moment needs. A backend
	 * whose stream is bigger than that may deliver a smaller frame instead of
	 * repacking the whole thing - a 640x480 viewfinder off a 4080x3072 sensor
	 * stream is the shape that matters. 0x0 means nothing is known, and so does
	 * a backend that does not implement this: the full stream is delivered. */
	void (*set_frame_size_needed)(struct atl_camera *camera, int width, int height);

	/* Optional camera2 ops. get_camera2_id_list == NULL means this backend
	 * cannot serve camera2 at all, and android.hardware.camera2 then reports
	 * no access rather than an empty camera list. */

	/* camera2 ids ("0", "1", ...); array and strings owned by the backend */
	const char *const *(*get_camera2_id_list)(int *count);
	/* static characteristics of one camera; the caller owns the result and
	 * frees it with atl_camera_metadata_free(). NULL on failure. */
	struct atl_camera_metadata *(*get_static_metadata)(const char *id);
	/* tags valid in capture requests / present in results / listed in the
	 * characteristics (ATL_CAMERA2_KEYS_*); NULL when the backend does not
	 * know, array owned by the backend */
	const uint32_t *(*get_available_keys)(const char *id, int which, int *count);
	/* Optional: the settings of the request whose frames are being delivered,
	 * for a backend that speaks camera2 itself and can apply them as they are
	 * to its one preview stream (the emulated camera2 session, see below, and
	 * Camera1). NULL for the synthetic backends, which act on the few keys
	 * they understand through the calls above instead. */
	void (*set_request_metadata)(struct atl_camera *camera,
	                             const struct atl_camera_metadata *request);
	/* The result metadata of the frame being delivered: what of the request
	 * the backend honoured, plus its own sensor readings. request may be NULL
	 * (no capture in flight is not a case that reaches here, but a request
	 * with no settings is). The caller owns the result and adds the frame's
	 * timestamp itself. NULL when the backend has no per-frame metadata. */
	struct atl_camera_metadata *(*get_result_metadata)(struct atl_camera *camera,
	                                                   const struct atl_camera_metadata *request);

	/*
	 * Optional real camera2 streams (AOSP: configureStreams / submitRequest).
	 * NULL means camera_streams.c emulates them over the preview ops above.
	 * configure_streams with n_streams = 0 tears the session down; the
	 * callbacks are fixed for the session's lifetime. A one-shot request that
	 * has not started when flush_requests is called is dropped and reported
	 * through failed(). Every one of these is called from the app thread.
	 */
	bool (*configure_streams)(struct atl_camera *camera, const struct atl_camera_stream *streams,
	                          int n_streams, const struct atl_camera_stream_input *input,
	                          const struct atl_camera_stream_callbacks *callbacks, void *user);
	bool (*submit_request)(struct atl_camera *camera, int request_id,
	                       const struct atl_camera_metadata *settings, uint32_t streams,
	                       bool repeating);
	void (*cancel_repeating)(struct atl_camera *camera);
	void (*flush_requests)(struct atl_camera *camera);

	/*
	 * Optional reprocessing, on a session configured with an input. NULL when
	 * the backend cannot express an input stream at all - the NDK could not
	 * until libcamera2ndk grew the Halium extension, and the device's own
	 * library may still be one that has not.
	 *
	 * queue_input hands the camera a buffer of this camera's own, taken from
	 * an ImageReader on one of the session's streams; the backend holds it
	 * and calls released() from one of its threads once the camera is done
	 * with it. submit_reprocess is a one-shot capture whose input is the
	 * oldest queued buffer: input_timestamp is that frame's sensor timestamp,
	 * which is how the backend finds the result to build the request from.
	 */
	bool (*queue_input)(struct atl_camera *camera, struct atl_camera_buffer *buffer,
	                    atl_camera_input_released_cb released, void *user);
	bool (*submit_reprocess)(struct atl_camera *camera, int request_id,
	                         const struct atl_camera_metadata *settings, uint32_t streams,
	                         int64_t input_timestamp);
};

/* The active backend: ATL_CAMERA_BACKEND env var selects by name (gst,
 * camera2ndk, hybris, none), default auto (camera2ndk, then hybris, then gst -
 * the first whose libraries load). Returns NULL when no backend is available or
 * ATL_UGLY_ENABLE_CAMERA is unset; callers then report zero cameras. */
const struct atl_camera_backend *atl_camera_backend_get(void);

/* True while a backend can hand the app's GL thread real external textures (the
 * camera2ndk zero-copy preview binds camera buffers as GL_TEXTURE_EXTERNAL_OES).
 * ATL's SurfaceTexture is a plain GL_TEXTURE_2D everywhere else, and
 * android_opengl_GLES20.c rewrites the target to match. */
bool atl_camera_external_textures(void);
void atl_camera_set_external_textures(bool available);

/* camera_backend_gst.c; NULL when GStreamer fails to initialize */
const struct atl_camera_backend *atl_camera_backend_gst_get(void);

/* camera_backend_camera2ndk.c; NULL when the Android camera2 NDK libraries
 * cannot be loaded through libhybris (i.e. everywhere but a Halium device) */
const struct atl_camera_backend *atl_camera_backend_camera2ndk_get(void);

/* camera_backend_hybris.c; NULL when the libhybris camera compat layer cannot
 * be dlopen'd (i.e. everywhere but a Halium device) */
const struct atl_camera_backend *atl_camera_backend_hybris_get(void);

/* camera_replay.c; the recorded frames of ATL_CAMERA_REPLAY as a camera2
 * backend of their own, NULL when that file is missing or unreadable */
const struct atl_camera_backend *atl_camera_backend_replay_get(void);


#endif
