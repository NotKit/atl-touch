/*
 * GStreamer camera backend: a synthetic desktop camera.
 *
 * Pipeline: <ATL_CAMERA_GST_SRC, default "videotestsrc is-live=true">
 *           ! videoconvert ! capsfilter (NV21, WxH, fps) ! appsink
 *
 * One back-facing camera (id 0), plus a front-facing id 1 when
 * ATL_CAMERA_GST_SRC_1 names a source for it. ATL_CAMERA_DUMP_FRAMES=<dir>
 * writes every 30th frame as PNG plus a running "frame-count" file — the test
 * loop's eyes.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <third_party/android-headers/camera/NdkCameraMetadataTags.h>

#include "../media/atl_gst.h"
#include "camera2_metadata.h"
#include "camera_backend.h"
#include "camera_frame.h"

struct atl_camera {
	GstElement *pipeline;
	GstElement *capsfilter;
	GstElement *appsink;

	int width;
	int height;
	int fps_max; /* scaled by 1000 */
	bool previewing;

	GMutex lock; /* guards the callback pointers */
	atl_camera_frame_cb frame_cb;
	void *frame_user;
	atl_camera_error_cb error_cb;
	void *error_user;

	uint64_t frame_count;
	char *dump_dir;
	uint8_t *pack_buf; /* scratch for repacking non-contiguous frames */
	size_t pack_buf_size;

	/* takePicture: the worker thread waits for the streaming thread to hand
	 * over one frame at picture size, then encodes it (all under lock) */
	GCond capture_cond;
	GThread *capture_thread;
	bool capture_active;
	bool capture_have_frame;
	bool capture_abort;
	int capture_width;
	int capture_height;
	int capture_quality;
	atl_camera_jpeg_cb capture_cb;
	void *capture_user;
	uint8_t *capture_nv21;
	size_t capture_nv21_size;
};

static const struct atl_camera_size gst_sizes[] = {
	{640, 480},
	{1280, 720},
};
static const struct atl_camera_fps_range gst_fps_ranges[] = {
	{30000, 30000},
};
static const struct atl_camera_caps gst_camera_caps = {
	.preview_sizes = gst_sizes,
	.n_preview_sizes = G_N_ELEMENTS(gst_sizes),
	.picture_sizes = gst_sizes,
	.n_picture_sizes = G_N_ELEMENTS(gst_sizes),
	.fps_ranges = gst_fps_ranges,
	.n_fps_ranges = G_N_ELEMENTS(gst_fps_ranges),
	.focus_modes = "fixed,infinity",
	.flash_modes = "off",
	/* a video source has no scene/white-balance/effect/antibanding control */
	.scene_modes = NULL,
	.white_balance_modes = NULL,
	.color_effects = NULL,
	.antibanding_modes = NULL,
	.zoom_supported = false,
	.max_zoom = 0,
	/* nominal 4:3 webcam optics; apps use these only to size focus overlays */
	.horizontal_view_angle = 60.0f,
	.vertical_view_angle = 45.0f,
	.min_exposure_compensation = 0,
	.max_exposure_compensation = 0,
	.exposure_compensation_step = 0.0f,
	.max_num_focus_areas = 0,
	.max_num_metering_areas = 0,
	.max_num_detected_faces = 0,
	.video_snapshot_supported = false,
	.video_stabilization_supported = false,
};

static GstCaps *make_caps(struct atl_camera *camera, int width, int height)
{
	return gst_caps_new_simple("video/x-raw",
	                           "format", G_TYPE_STRING, "NV21",
	                           "width", G_TYPE_INT, width,
	                           "height", G_TYPE_INT, height,
	                           "framerate", GST_TYPE_FRACTION, camera->fps_max / 1000, 1,
	                           NULL);
}

/* push size/fps to the capsfilter; live pipelines renegotiate */
static void apply_caps(struct atl_camera *camera, int width, int height)
{
	GstCaps *caps = make_caps(camera, width, height);

	g_object_set(camera->capsfilter, "caps", caps, NULL);
	gst_caps_unref(caps);
}

static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user)
{
	struct atl_camera *camera = user;
	GstSample *sample = gst_app_sink_pull_sample(sink);
	GstVideoInfo info;
	GstVideoFrame frame;

	if (!sample)
		return GST_FLOW_OK;
	if (!gst_video_info_from_caps(&info, gst_sample_get_caps(sample)) ||
	    !gst_video_frame_map(&frame, &info, gst_sample_get_buffer(sample), GST_MAP_READ)) {
		gst_sample_unref(sample);
		return GST_FLOW_OK;
	}

	int width = GST_VIDEO_FRAME_WIDTH(&frame);
	int height = GST_VIDEO_FRAME_HEIGHT(&frame);
	int y_stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
	int vu_stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 1);
	const uint8_t *y = GST_VIDEO_FRAME_PLANE_DATA(&frame, 0);
	const uint8_t *vu = GST_VIDEO_FRAME_PLANE_DATA(&frame, 1);
	const uint8_t *nv21 = y;
	int stride = y_stride;

	/* the frame callback contract is one contiguous NV21 buffer; repack when
	 * gst hands us split or differently-strided planes */
	if (vu != y + (size_t)y_stride * height || vu_stride != y_stride) {
		size_t need = (size_t)width * height * 3 / 2;
		if (camera->pack_buf_size < need) {
			camera->pack_buf = realloc(camera->pack_buf, need);
			camera->pack_buf_size = need;
		}
		for (int row = 0; row < height; row++)
			memcpy(camera->pack_buf + (size_t)row * width, y + (size_t)row * y_stride, width);
		for (int row = 0; row < height / 2; row++)
			memcpy(camera->pack_buf + (size_t)width * height + (size_t)row * width,
			       vu + (size_t)row * vu_stride, width);
		nv21 = camera->pack_buf;
		stride = width;
	}

	camera->frame_count++;
	if (camera->frame_count == 1)
		fprintf(stderr, "Camera gst: first frame (%dx%d, stride %d)\n", width, height, stride);

	g_mutex_lock(&camera->lock);
	/* a capture in flight owns the stream: the frames are at picture size and
	 * the preview is about to stop anyway */
	bool capturing = camera->capture_active;
	if (capturing && !camera->capture_have_frame &&
	    width == camera->capture_width && height == camera->capture_height) {
		size_t need = (size_t)width * height * 3 / 2;
		if (camera->capture_nv21_size < need) {
			camera->capture_nv21 = realloc(camera->capture_nv21, need);
			camera->capture_nv21_size = camera->capture_nv21 ? need : 0;
		}
		if (camera->capture_nv21) {
			atl_camera_nv21_pack(camera->capture_nv21, nv21, width, height, stride);
			camera->capture_have_frame = true;
			g_cond_broadcast(&camera->capture_cond);
		}
	}
	atl_camera_frame_cb cb = capturing ? NULL : camera->frame_cb;
	void *cb_user = camera->frame_user;
	g_mutex_unlock(&camera->lock);
	if (cb)
		cb(nv21, width, height, stride, cb_user);

	atl_camera_dump_frame(camera->dump_dir, camera->frame_count, nv21, width, height, stride);

	gst_video_frame_unmap(&frame);
	gst_sample_unref(sample);
	return GST_FLOW_OK;
}

/* sync handler: a GLib main loop is not guaranteed to be running */
static GstBusSyncReply on_bus_message(GstBus *bus, GstMessage *message, gpointer user)
{
	struct atl_camera *camera = user;

	/* a source that ends is a camera that died, as far as the app is concerned */
	if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR || GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
		if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
			GError *error = NULL;
			gst_message_parse_error(message, &error, NULL);
			fprintf(stderr, "Camera gst: pipeline error: %s\n", error ? error->message : "(no detail)");
			g_clear_error(&error);
		} else {
			fprintf(stderr, "Camera gst: pipeline reached end of stream\n");
		}

		g_mutex_lock(&camera->lock);
		atl_camera_error_cb cb = camera->error_cb;
		void *cb_user = camera->error_user;
		g_mutex_unlock(&camera->lock);
		if (cb)
			cb(ATL_CAMERA_ERROR_SERVER_DIED, cb_user);
	}
	gst_message_unref(message);
	return GST_BUS_DROP;
}

/*
 * The source for a camera: ATL_CAMERA_GST_SRC is camera 0, and setting
 * ATL_CAMERA_GST_SRC_1 adds a second, front-facing camera so an app's
 * camera-switch path has somewhere to switch to. NULL means "no such camera".
 */
static const char *gst_camera_source(int id)
{
	const char *src_desc;

	if (id == 0) {
		src_desc = getenv("ATL_CAMERA_GST_SRC");
		return src_desc && *src_desc ? src_desc : "videotestsrc is-live=true";
	}
	if (id != 1)
		return NULL;
	src_desc = getenv("ATL_CAMERA_GST_SRC_1");
	return src_desc && *src_desc ? src_desc : NULL;
}

static int gst_camera_get_count(void)
{
	return gst_camera_source(1) ? 2 : 1;
}

static bool gst_camera_get_info(int id, int *facing, int *orientation)
{
	if (!gst_camera_source(id))
		return false;
	*facing = id == 0 ? ATL_CAMERA_FACING_BACK : ATL_CAMERA_FACING_FRONT;
	*orientation = 0;
	return true;
}

static void gst_camera_close(struct atl_camera *camera);

static struct atl_camera *gst_camera_open(int id)
{
	const char *src_desc = gst_camera_source(id);
	GError *error = NULL;

	if (!src_desc)
		return NULL;

	struct atl_camera *camera = calloc(1, sizeof(*camera));
	camera->width = 640;
	camera->height = 480;
	camera->fps_max = 30000;
	g_mutex_init(&camera->lock);
	g_cond_init(&camera->capture_cond);

	const char *dump = getenv("ATL_CAMERA_DUMP_FRAMES");
	if (dump && *dump) {
		g_mkdir_with_parents(dump, 0755);
		camera->dump_dir = g_strdup(dump);
	}

	GstElement *src = gst_parse_bin_from_description(src_desc, TRUE, &error);
	if (!src) {
		fprintf(stderr, "Camera gst: bad source '%s': %s\n", src_desc, error ? error->message : "");
		g_clear_error(&error);
		goto fail;
	}

	camera->pipeline = gst_pipeline_new("atl-camera");
	camera->capsfilter = gst_element_factory_make("capsfilter", NULL);
	camera->appsink = gst_element_factory_make("appsink", NULL);
	GstElement *convert = gst_element_factory_make("videoconvert", NULL);
	if (!camera->capsfilter || !camera->appsink || !convert) {
		fprintf(stderr, "Camera gst: missing base elements (videoconvert/capsfilter/appsink)\n");
		gst_object_unref(src);
		g_clear_object(&convert);
		g_clear_object(&camera->capsfilter);
		g_clear_object(&camera->appsink);
		goto fail;
	}

	apply_caps(camera, camera->width, camera->height);
	/* stay real-time: keep at most 2 queued frames, drop older ones */
	g_object_set(camera->appsink, "max-buffers", 2, "drop", TRUE, "sync", FALSE, NULL);

	gst_bin_add_many(GST_BIN(camera->pipeline), src, convert, camera->capsfilter, camera->appsink, NULL);
	if (!gst_element_link_many(src, convert, camera->capsfilter, camera->appsink, NULL)) {
		fprintf(stderr, "Camera gst: failed to link pipeline\n");
		goto fail;
	}

	GstAppSinkCallbacks callbacks = { .new_sample = on_new_sample };
	gst_app_sink_set_callbacks(GST_APP_SINK(camera->appsink), &callbacks, camera, NULL);

	GstBus *bus = gst_element_get_bus(camera->pipeline);
	gst_bus_set_sync_handler(bus, on_bus_message, camera, NULL);
	gst_object_unref(bus);

	fprintf(stderr, "Camera gst: opened camera %d (source: %s)\n", id, src_desc);
	return camera;

fail:
	gst_camera_close(camera);
	return NULL;
}

/* a capture in flight would outlive the session, so wake it and wait */
static void gst_camera_join_capture(struct atl_camera *camera, bool abort)
{
	if (!camera->capture_thread)
		return;
	if (abort) {
		g_mutex_lock(&camera->lock);
		camera->capture_abort = true;
		g_cond_broadcast(&camera->capture_cond);
		g_mutex_unlock(&camera->lock);
	}
	g_thread_join(camera->capture_thread);
	camera->capture_thread = NULL;
	camera->capture_abort = false;
}

static void gst_camera_close(struct atl_camera *camera)
{
	gst_camera_join_capture(camera, true);
	if (camera->pipeline) {
		gst_element_set_state(camera->pipeline, GST_STATE_NULL);
		gst_object_unref(camera->pipeline);
	}
	g_cond_clear(&camera->capture_cond);
	g_mutex_clear(&camera->lock);
	g_free(camera->dump_dir);
	free(camera->pack_buf);
	free(camera->capture_nv21);
	free(camera);
}

static const struct atl_camera_caps *gst_camera_get_caps(struct atl_camera *camera)
{
	return &gst_camera_caps;
}

static bool gst_camera_apply_caps(struct atl_camera *camera)
{
	apply_caps(camera, camera->width, camera->height);
	return true;
}

static bool gst_camera_set_preview_size(struct atl_camera *camera, int width, int height)
{
	if (width <= 0 || height <= 0)
		return false;
	camera->width = width;
	camera->height = height;
	return gst_camera_apply_caps(camera);
}

static bool gst_camera_set_preview_format(struct atl_camera *camera, int format)
{
	return format == ATL_CAMERA_FORMAT_NV21;
}

static bool gst_camera_set_fps_range(struct atl_camera *camera, int min, int max)
{
	if (max < 1000)
		return false;
	camera->fps_max = max;
	return gst_camera_apply_caps(camera);
}

static void gst_camera_set_frame_callback(struct atl_camera *camera, atl_camera_frame_cb cb, void *user)
{
	g_mutex_lock(&camera->lock);
	camera->frame_cb = cb;
	camera->frame_user = user;
	g_mutex_unlock(&camera->lock);
}

static void gst_camera_set_error_callback(struct atl_camera *camera, atl_camera_error_cb cb, void *user)
{
	g_mutex_lock(&camera->lock);
	camera->error_cb = cb;
	camera->error_user = user;
	g_mutex_unlock(&camera->lock);
}

static bool gst_camera_start_preview(struct atl_camera *camera)
{
	if (gst_element_set_state(camera->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		fprintf(stderr, "Camera gst: failed to start preview\n");
		return false;
	}
	camera->previewing = true;
	fprintf(stderr, "Camera gst: preview started (%dx%d @ %d/1000 fps)\n",
	        camera->width, camera->height, camera->fps_max);
	return true;
}

static void gst_camera_stop_preview(struct atl_camera *camera)
{
	if (!camera->previewing)
		return;
	camera->previewing = false;
	gst_element_set_state(camera->pipeline, GST_STATE_READY);
	fprintf(stderr, "Camera gst: preview stopped\n");
}

/*
 * Capture worker: waits for the streaming thread to hand over one frame at
 * picture size, stops the preview (AOSP: startPreview() is needed to resume),
 * restores the preview caps and JPEG-encodes off the streaming thread.
 */
static gpointer capture_worker(gpointer user)
{
	struct atl_camera *camera = user;
	gint64 deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
	uint8_t *nv21 = NULL, *rgba = NULL, *jpeg = NULL;
	size_t jpeg_size = 0;

	g_mutex_lock(&camera->lock);
	while (!camera->capture_have_frame && !camera->capture_abort)
		if (!g_cond_wait_until(&camera->capture_cond, &camera->lock, deadline))
			break;
	int width = camera->capture_width;
	int height = camera->capture_height;
	int quality = camera->capture_quality;
	atl_camera_jpeg_cb cb = camera->capture_cb;
	void *cb_user = camera->capture_user;
	if (camera->capture_have_frame) {
		nv21 = camera->capture_nv21;
		camera->capture_nv21 = NULL;
		camera->capture_nv21_size = 0;
	}
	g_mutex_unlock(&camera->lock);

	gst_camera_stop_preview(camera);
	gst_camera_apply_caps(camera);

	if (nv21) {
		rgba = malloc((size_t)width * height * 4);
		if (rgba) {
			atl_camera_nv21_to_rgba(nv21, width, height, width, rgba);
			if (!atl_camera_encode_jpeg(rgba, width, height, quality, &jpeg, &jpeg_size))
				fprintf(stderr, "Camera gst: JPEG encoding failed\n");
		}
	} else {
		fprintf(stderr, "Camera gst: no frame captured at %dx%d\n", width, height);
	}

	g_mutex_lock(&camera->lock);
	camera->capture_active = false;
	g_mutex_unlock(&camera->lock);

	if (jpeg)
		fprintf(stderr, "Camera gst: captured %dx%d picture, %zu bytes of JPEG\n",
		        width, height, jpeg_size);
	cb(jpeg, jpeg_size, cb_user); /* NULL means the capture failed */

	free(jpeg);
	free(rgba);
	free(nv21);
	return NULL;
}

static bool gst_camera_take_picture(struct atl_camera *camera, int width, int height,
                                    int jpeg_quality, atl_camera_jpeg_cb cb, void *user)
{
	if (width <= 0 || height <= 0 || !cb)
		return false;

	g_mutex_lock(&camera->lock);
	bool busy = camera->capture_active;
	g_mutex_unlock(&camera->lock);
	if (busy) {
		fprintf(stderr, "Camera gst: a capture is already in flight\n");
		return false;
	}
	gst_camera_join_capture(camera, false); /* reap the previous capture */

	g_mutex_lock(&camera->lock);
	camera->capture_active = true;
	camera->capture_have_frame = false;
	camera->capture_width = width;
	camera->capture_height = height;
	camera->capture_quality = jpeg_quality;
	camera->capture_cb = cb;
	camera->capture_user = user;
	g_mutex_unlock(&camera->lock);

	/* the picture size is usually not the preview size: renegotiate, and make
	 * sure frames are flowing at all */
	apply_caps(camera, width, height);
	if (!camera->previewing && !gst_camera_start_preview(camera)) {
		g_mutex_lock(&camera->lock);
		camera->capture_active = false;
		g_mutex_unlock(&camera->lock);
		gst_camera_apply_caps(camera);
		return false;
	}

	camera->capture_thread = g_thread_new("atl-camera-capture", capture_worker, camera);
	return true;
}

/* synthetic source: focus always succeeds immediately; the Java layer
 * marshals this onto the main loop */
static void gst_camera_autofocus(struct atl_camera *camera, atl_camera_autofocus_cb cb, void *user)
{
	if (cb)
		cb(true, user);
}

static void gst_camera_cancel_autofocus(struct atl_camera *camera)
{
}

static void gst_camera_set_display_orientation(struct atl_camera *camera, int degrees)
{
}

/*
 * camera2: one synthetic FULL back camera built out of the same caps the
 * Camera1 path advertises. The numbers a videotestsrc has no opinion about
 * (sensitivity, exposure, optics) are plausible constants — apps only use them
 * to size UI and to pick request values.
 */

/* android.graphics.ImageFormat values the streams are advertised in */
#define GST_CAMERA2_FORMAT_PRIVATE     0x22
#define GST_CAMERA2_FORMAT_YUV_420_888 0x23
#define GST_CAMERA2_FORMAT_JPEG        0x100

/* A tag of our own in the vendor range, so the opaque-tag path is exercised on
 * the desktop too (the Pixel HAL has 511 of these). */
#define GST_CAMERA2_VENDOR_TAG      0x80000000u
#define GST_CAMERA2_VENDOR_TAG_NAME "org.atl.gst.syntheticSource"

static const char *const gst_camera2_ids[] = {"0", "1"};

/* -1 for an id this backend does not serve */
static int gst_camera2_index_of(const char *id)
{
	if (!id)
		return -1;
	for (unsigned i = 0; i < G_N_ELEMENTS(gst_camera2_ids); i++)
		if (!strcmp(id, gst_camera2_ids[i]))
			return gst_camera_source((int)i) ? (int)i : -1;
	return -1;
}

/* what a repeating request may set; the result adds the sensor readings */
static const uint32_t gst_camera2_request_keys[] = {
	ACAMERA_CONTROL_AE_ANTIBANDING_MODE,
	ACAMERA_CONTROL_AE_MODE,
	ACAMERA_CONTROL_AE_TARGET_FPS_RANGE,
	ACAMERA_CONTROL_AF_MODE,
	ACAMERA_CONTROL_AWB_MODE,
	ACAMERA_CONTROL_CAPTURE_INTENT,
	ACAMERA_CONTROL_MODE,
	ACAMERA_FLASH_MODE,
	ACAMERA_JPEG_ORIENTATION,
	ACAMERA_JPEG_QUALITY,
	ACAMERA_JPEG_THUMBNAIL_SIZE,
	ACAMERA_SCALER_CROP_REGION,
	ACAMERA_STATISTICS_FACE_DETECT_MODE,
};

static const uint32_t gst_camera2_result_keys[] = {
	ACAMERA_COLOR_CORRECTION_GAINS,
	ACAMERA_COLOR_CORRECTION_TRANSFORM,
	ACAMERA_CONTROL_AE_MODE,
	ACAMERA_CONTROL_AE_STATE,
	ACAMERA_CONTROL_AF_MODE,
	ACAMERA_CONTROL_AF_STATE,
	ACAMERA_CONTROL_AWB_MODE,
	ACAMERA_CONTROL_AWB_STATE,
	ACAMERA_CONTROL_MODE,
	ACAMERA_FLASH_STATE,
	ACAMERA_LENS_FOCAL_LENGTH,
	ACAMERA_SCALER_CROP_REGION,
	ACAMERA_SENSOR_EXPOSURE_TIME,
	ACAMERA_SENSOR_FRAME_DURATION,
	ACAMERA_SENSOR_SENSITIVITY,
	ACAMERA_SENSOR_TIMESTAMP,
	ACAMERA_STATISTICS_FACE_DETECT_MODE,
	ACAMERA_STATISTICS_FACE_RECTANGLES,
	ACAMERA_STATISTICS_FACE_SCORES,
	ACAMERA_STATISTICS_LENS_SHADING_MAP,
};

/* the synthetic camera's shading map grid, columns x rows */
#define GST_CAMERA2_SHADING_MAP_COLUMNS 4
#define GST_CAMERA2_SHADING_MAP_ROWS    3

/* the characteristics key list is the metadata's own tag list, minus the three
 * key lists themselves; filled in on the first build */
static uint32_t *gst_camera2_characteristics_keys;
static int gst_camera2_n_characteristics_keys;

static void md_add_u8(struct atl_camera_metadata *md, uint32_t tag, const uint8_t *v, int n)
{
	atl_camera_metadata_add(md, tag, ATL_CAMERA2_TYPE_BYTE, v, n);
}

static void md_add_i32(struct atl_camera_metadata *md, uint32_t tag, const int32_t *v, int n)
{
	atl_camera_metadata_add(md, tag, ATL_CAMERA2_TYPE_INT32, v, n);
}

static void md_add_i64(struct atl_camera_metadata *md, uint32_t tag, const int64_t *v, int n)
{
	atl_camera_metadata_add(md, tag, ATL_CAMERA2_TYPE_INT64, v, n);
}

static void md_add_f32(struct atl_camera_metadata *md, uint32_t tag, const float *v, int n)
{
	atl_camera_metadata_add(md, tag, ATL_CAMERA2_TYPE_FLOAT, v, n);
}

/* a rational is a (numerator, denominator) int32 pair */
static void md_add_rational(struct atl_camera_metadata *md, uint32_t tag, int32_t num, int32_t den)
{
	const int32_t v[2] = {num, den};

	atl_camera_metadata_add(md, tag, ATL_CAMERA2_TYPE_RATIONAL, v, 1);
}

static const int32_t gst_camera2_formats[] = {
	GST_CAMERA2_FORMAT_PRIVATE,
	GST_CAMERA2_FORMAT_YUV_420_888,
	GST_CAMERA2_FORMAT_JPEG,
};

/* (format, width, height, output) quadruples for every format x size, plus the
 * matching frame-duration and stall tables */
static void md_add_stream_configurations(struct atl_camera_metadata *md)
{
	int n = (int)G_N_ELEMENTS(gst_camera2_formats) * (int)G_N_ELEMENTS(gst_sizes);
	int32_t *configs = g_new(int32_t, n * 4);
	int64_t *durations = g_new(int64_t, n * 4);
	int64_t *stalls = g_new(int64_t, n * 4);
	int i = 0;

	for (unsigned f = 0; f < G_N_ELEMENTS(gst_camera2_formats); f++) {
		for (unsigned s = 0; s < G_N_ELEMENTS(gst_sizes); s++, i++) {
			int32_t format = gst_camera2_formats[f];

			configs[i * 4 + 0] = format;
			configs[i * 4 + 1] = gst_sizes[s].width;
			configs[i * 4 + 2] = gst_sizes[s].height;
			configs[i * 4 + 3] = ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

			durations[i * 4 + 0] = format;
			durations[i * 4 + 1] = gst_sizes[s].width;
			durations[i * 4 + 2] = gst_sizes[s].height;
			durations[i * 4 + 3] = 1000000000LL / (gst_fps_ranges[0].max / 1000);

			stalls[i * 4 + 0] = format;
			stalls[i * 4 + 1] = gst_sizes[s].width;
			stalls[i * 4 + 2] = gst_sizes[s].height;
			/* only the JPEG stream stalls the pipeline */
			stalls[i * 4 + 3] = format == GST_CAMERA2_FORMAT_JPEG ? 33000000LL : 0;
		}
	}

	md_add_i32(md, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, configs, n * 4);
	md_add_i64(md, ACAMERA_SCALER_AVAILABLE_MIN_FRAME_DURATIONS, durations, n * 4);
	md_add_i64(md, ACAMERA_SCALER_AVAILABLE_STALL_DURATIONS, stalls, n * 4);
	g_free(configs);
	g_free(durations);
	g_free(stalls);
}

static struct atl_camera_metadata *gst_camera2_get_static_metadata(const char *id)
{
	/* the largest advertised size is the whole sensor */
	const int32_t width = gst_sizes[G_N_ELEMENTS(gst_sizes) - 1].width;
	const int32_t height = gst_sizes[G_N_ELEMENTS(gst_sizes) - 1].height;
	struct atl_camera_metadata *md;
	int32_t fps_ranges[G_N_ELEMENTS(gst_fps_ranges) * 2];
	int32_t vendor_value = 1;
	int index = gst_camera2_index_of(id);

	if (index < 0)
		return NULL;

	md = atl_camera_metadata_new();
	if (!md)
		return NULL;

	md_add_u8(md, ACAMERA_LENS_FACING,
	          (const uint8_t[]){index == 0 ? ACAMERA_LENS_FACING_BACK : ACAMERA_LENS_FACING_FRONT}, 1);
	md_add_u8(md, ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL,
	          (const uint8_t[]){ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_FULL}, 1);

	md_add_i32(md, ACAMERA_SENSOR_INFO_ACTIVE_ARRAY_SIZE, (const int32_t[]){0, 0, width, height}, 4);
	md_add_i32(md, ACAMERA_SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE,
	           (const int32_t[]){0, 0, width, height}, 4);
	md_add_i32(md, ACAMERA_SENSOR_INFO_PIXEL_ARRAY_SIZE, (const int32_t[]){width, height}, 2);
	md_add_f32(md, ACAMERA_SENSOR_INFO_PHYSICAL_SIZE, (const float[]){3.20f, 2.40f}, 2);
	md_add_i32(md, ACAMERA_SENSOR_ORIENTATION, (const int32_t[]){0}, 1);
	md_add_u8(md, ACAMERA_SENSOR_INFO_TIMESTAMP_SOURCE,
	          (const uint8_t[]){ACAMERA_SENSOR_INFO_TIMESTAMP_SOURCE_UNKNOWN}, 1);
	md_add_u8(md, ACAMERA_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT,
	          (const uint8_t[]){ACAMERA_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB}, 1);
	md_add_i32(md, ACAMERA_SENSOR_INFO_SENSITIVITY_RANGE, (const int32_t[]){100, 1600}, 2);
	md_add_i32(md, ACAMERA_SENSOR_INFO_WHITE_LEVEL, (const int32_t[]){1023}, 1);
	md_add_i64(md, ACAMERA_SENSOR_INFO_EXPOSURE_TIME_RANGE,
	           (const int64_t[]){100000LL, 100000000LL}, 2);
	md_add_i64(md, ACAMERA_SENSOR_INFO_MAX_FRAME_DURATION, (const int64_t[]){100000000LL}, 1);
	md_add_u8(md, ACAMERA_SENSOR_AVAILABLE_TEST_PATTERN_MODES,
	          (const uint8_t[]){ACAMERA_SENSOR_TEST_PATTERN_MODE_OFF}, 1);

	md_add_stream_configurations(md);
	md_add_f32(md, ACAMERA_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, (const float[]){1.0f}, 1);
	md_add_u8(md, ACAMERA_SCALER_CROPPING_TYPE,
	          (const uint8_t[]){ACAMERA_SCALER_CROPPING_TYPE_CENTER_ONLY}, 1);

	md_add_u8(md, ACAMERA_CONTROL_AVAILABLE_MODES,
	          (const uint8_t[]){ACAMERA_CONTROL_MODE_OFF, ACAMERA_CONTROL_MODE_AUTO}, 2);
	md_add_u8(md, ACAMERA_CONTROL_AE_AVAILABLE_MODES,
	          (const uint8_t[]){ACAMERA_CONTROL_AE_MODE_OFF, ACAMERA_CONTROL_AE_MODE_ON}, 2);
	for (unsigned i = 0; i < G_N_ELEMENTS(gst_fps_ranges); i++) {
		fps_ranges[i * 2 + 0] = gst_fps_ranges[i].min / 1000;
		fps_ranges[i * 2 + 1] = gst_fps_ranges[i].max / 1000;
	}
	md_add_i32(md, ACAMERA_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, fps_ranges,
	           (int)G_N_ELEMENTS(gst_fps_ranges) * 2);
	/* no exposure compensation: an empty range still needs a non-zero step */
	md_add_i32(md, ACAMERA_CONTROL_AE_COMPENSATION_RANGE, (const int32_t[]){0, 0}, 2);
	md_add_rational(md, ACAMERA_CONTROL_AE_COMPENSATION_STEP, 1, 1);
	md_add_u8(md, ACAMERA_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
	          (const uint8_t[]){ACAMERA_CONTROL_AE_ANTIBANDING_MODE_OFF,
	                            ACAMERA_CONTROL_AE_ANTIBANDING_MODE_AUTO}, 2);
	/* the pipeline has no focus control, so AF is off-only (AOSP's fixed-focus shape) */
	md_add_u8(md, ACAMERA_CONTROL_AF_AVAILABLE_MODES,
	          (const uint8_t[]){ACAMERA_CONTROL_AF_MODE_OFF}, 1);
	md_add_u8(md, ACAMERA_CONTROL_AWB_AVAILABLE_MODES,
	          (const uint8_t[]){ACAMERA_CONTROL_AWB_MODE_AUTO}, 1);
	md_add_u8(md, ACAMERA_CONTROL_AVAILABLE_EFFECTS,
	          (const uint8_t[]){ACAMERA_CONTROL_EFFECT_MODE_OFF}, 1);
	md_add_u8(md, ACAMERA_CONTROL_AVAILABLE_SCENE_MODES,
	          (const uint8_t[]){ACAMERA_CONTROL_SCENE_MODE_DISABLED}, 1);
	md_add_u8(md, ACAMERA_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
	          (const uint8_t[]){ACAMERA_CONTROL_VIDEO_STABILIZATION_MODE_OFF}, 1);
	/* max AE/AWB/AF metering regions */
	md_add_i32(md, ACAMERA_CONTROL_MAX_REGIONS, (const int32_t[]){0, 0, 0}, 3);

	md_add_u8(md, ACAMERA_FLASH_INFO_AVAILABLE, (const uint8_t[]){0}, 1);

	md_add_f32(md, ACAMERA_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, (const float[]){2.77f}, 1);
	md_add_f32(md, ACAMERA_LENS_INFO_AVAILABLE_APERTURES, (const float[]){2.0f}, 1);
	md_add_f32(md, ACAMERA_LENS_INFO_MINIMUM_FOCUS_DISTANCE, (const float[]){0.0f}, 1);
	md_add_u8(md, ACAMERA_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
	          (const uint8_t[]){ACAMERA_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED}, 1);
	md_add_u8(md, ACAMERA_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION,
	          (const uint8_t[]){ACAMERA_LENS_OPTICAL_STABILIZATION_MODE_OFF}, 1);

	md_add_u8(md, ACAMERA_REQUEST_AVAILABLE_CAPABILITIES,
	          (const uint8_t[]){ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE,
	                            ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_READ_SENSOR_SETTINGS,
	                            ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_BURST_CAPTURE}, 3);
	md_add_i32(md, ACAMERA_REQUEST_PARTIAL_RESULT_COUNT, (const int32_t[]){1}, 1);
	md_add_u8(md, ACAMERA_REQUEST_PIPELINE_MAX_DEPTH, (const uint8_t[]){4}, 1);
	/* raw, processed-stalling, processed-non-stalling */
	md_add_i32(md, ACAMERA_REQUEST_MAX_NUM_OUTPUT_STREAMS, (const int32_t[]){0, 1, 2}, 3);

	md_add_i32(md, ACAMERA_JPEG_AVAILABLE_THUMBNAIL_SIZES, (const int32_t[]){0, 0, 160, 120}, 4);

	md_add_i32(md, ACAMERA_SYNC_MAX_LATENCY,
	           (const int32_t[]){ACAMERA_SYNC_MAX_LATENCY_PER_FRAME_CONTROL}, 1);

	/* SIMPLE face detection: bounds and a score, no landmarks and no ids */
	md_add_u8(md, ACAMERA_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES,
	          (const uint8_t[]){ACAMERA_STATISTICS_FACE_DETECT_MODE_OFF,
	                            ACAMERA_STATISTICS_FACE_DETECT_MODE_SIMPLE}, 2);
	md_add_i32(md, ACAMERA_STATISTICS_INFO_MAX_FACE_COUNT, (const int32_t[]){1}, 1);
	md_add_u8(md, ACAMERA_STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES,
	          (const uint8_t[]){ACAMERA_STATISTICS_LENS_SHADING_MAP_MODE_ON}, 1);
	md_add_i32(md, ACAMERA_LENS_INFO_SHADING_MAP_SIZE,
	           (const int32_t[]){GST_CAMERA2_SHADING_MAP_COLUMNS, GST_CAMERA2_SHADING_MAP_ROWS}, 2);
	/* four different levels, so an app indexing the pattern by (column, row)
	 * gets a different answer for each corner of the filter block */
	md_add_i32(md, ACAMERA_SENSOR_BLACK_LEVEL_PATTERN, (const int32_t[]){64, 65, 66, 67}, 4);

	md_add_u8(md, ACAMERA_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES,
	          (const uint8_t[]){ACAMERA_NOISE_REDUCTION_MODE_OFF,
	                            ACAMERA_NOISE_REDUCTION_MODE_FAST}, 2);
	md_add_u8(md, ACAMERA_EDGE_AVAILABLE_EDGE_MODES,
	          (const uint8_t[]){ACAMERA_EDGE_MODE_OFF, ACAMERA_EDGE_MODE_FAST}, 2);
	md_add_u8(md, ACAMERA_TONEMAP_AVAILABLE_TONE_MAP_MODES,
	          (const uint8_t[]){ACAMERA_TONEMAP_MODE_CONTRAST_CURVE, ACAMERA_TONEMAP_MODE_FAST}, 2);
	md_add_i32(md, ACAMERA_TONEMAP_MAX_CURVE_POINTS, (const int32_t[]){64}, 1);
	md_add_u8(md, ACAMERA_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES,
	          (const uint8_t[]){ACAMERA_HOT_PIXEL_MODE_OFF}, 1);
	md_add_u8(md, ACAMERA_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
	          (const uint8_t[]){ACAMERA_COLOR_CORRECTION_ABERRATION_MODE_OFF}, 1);
	md_add_u8(md, ACAMERA_SHADING_AVAILABLE_MODES,
	          (const uint8_t[]){ACAMERA_SHADING_MODE_OFF}, 1);

	atl_camera_metadata_add_vendor(md, GST_CAMERA2_VENDOR_TAG, GST_CAMERA2_VENDOR_TAG_NAME,
	                               ATL_CAMERA2_TYPE_INT32, &vendor_value, 1);

	if (!gst_camera2_characteristics_keys) {
		int n = atl_camera_metadata_n_entries(md);
		uint32_t *keys = g_new(uint32_t, n);

		for (int i = 0; i < n; i++)
			keys[i] = atl_camera_metadata_entry_at(md, i)->tag;
		gst_camera2_characteristics_keys = keys;
		gst_camera2_n_characteristics_keys = n;
	}

	/* the key lists are themselves characteristics, but not keys */
	md_add_i32(md, ACAMERA_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
	           (const int32_t *)gst_camera2_characteristics_keys,
	           gst_camera2_n_characteristics_keys);
	md_add_i32(md, ACAMERA_REQUEST_AVAILABLE_REQUEST_KEYS,
	           (const int32_t *)gst_camera2_request_keys,
	           (int)G_N_ELEMENTS(gst_camera2_request_keys));
	md_add_i32(md, ACAMERA_REQUEST_AVAILABLE_RESULT_KEYS,
	           (const int32_t *)gst_camera2_result_keys,
	           (int)G_N_ELEMENTS(gst_camera2_result_keys));

	return md;
}

/* control tags a request may set that the result echoes back unchanged */
static const uint32_t gst_camera2_echo_tags[] = {
	ACAMERA_CONTROL_AE_ANTIBANDING_MODE,
	ACAMERA_CONTROL_AE_MODE,
	ACAMERA_CONTROL_AE_TARGET_FPS_RANGE,
	ACAMERA_CONTROL_AF_MODE,
	ACAMERA_CONTROL_AWB_MODE,
	ACAMERA_CONTROL_CAPTURE_INTENT,
	ACAMERA_CONTROL_MODE,
	ACAMERA_FLASH_MODE,
	ACAMERA_JPEG_ORIENTATION,
	ACAMERA_JPEG_QUALITY,
	ACAMERA_JPEG_THUMBNAIL_SIZE,
	ACAMERA_SCALER_CROP_REGION,
	ACAMERA_STATISTICS_FACE_DETECT_MODE,
};

static void md_default_u8(struct atl_camera_metadata *md, uint32_t tag, uint8_t value)
{
	if (!atl_camera_metadata_find(md, tag))
		md_add_u8(md, tag, &value, 1);
}

/*
 * The frame statistics a FULL camera reports: a shading map, one face when the
 * request asked for face detection, and the colour correction that was applied.
 *
 * The shading map size travels with the map, because a result is read on its
 * own — the app never has the characteristics to hand when it walks the grid.
 */
static void md_add_statistics(struct atl_camera_metadata *md, int32_t width, int32_t height)
{
	const int columns = GST_CAMERA2_SHADING_MAP_COLUMNS;
	const int rows = GST_CAMERA2_SHADING_MAP_ROWS;
	float shading[4 * GST_CAMERA2_SHADING_MAP_COLUMNS * GST_CAMERA2_SHADING_MAP_ROWS];
	const struct atl_camera_metadata_entry *face_mode =
	    atl_camera_metadata_find(md, ACAMERA_STATISTICS_FACE_DETECT_MODE);
	int32_t identity[18];

	/* gains rise towards the corners, the way real vignetting does */
	for (int row = 0; row < rows; row++) {
		for (int column = 0; column < columns; column++) {
			float dx = (column + 0.5f) / columns - 0.5f;
			float dy = (row + 0.5f) / rows - 0.5f;
			float gain = 1.0f + (dx * dx + dy * dy);

			for (int channel = 0; channel < 4; channel++)
				shading[(row * columns + column) * 4 + channel] = gain + channel * 0.01f;
		}
	}
	md_add_f32(md, ACAMERA_STATISTICS_LENS_SHADING_MAP, shading, (int)G_N_ELEMENTS(shading));
	md_add_i32(md, ACAMERA_LENS_INFO_SHADING_MAP_SIZE, (const int32_t[]){columns, rows}, 2);

	if (face_mode && face_mode->count >= 1 &&
	    ((const uint8_t *)face_mode->data)[0] != ACAMERA_STATISTICS_FACE_DETECT_MODE_OFF) {
		/* the middle of the frame is as good a face as a test pattern has */
		const int32_t rect[4] = {width / 4, height / 4, width / 2, height / 2};

		md_add_i32(md, ACAMERA_STATISTICS_FACE_RECTANGLES, rect, 4);
		md_add_u8(md, ACAMERA_STATISTICS_FACE_SCORES, (const uint8_t[]){100}, 1);
	}

	for (int i = 0; i < 9; i++) {
		identity[i * 2] = i % 4 == 0 ? 1 : 0;
		identity[i * 2 + 1] = 1;
	}
	atl_camera_metadata_add(md, ACAMERA_COLOR_CORRECTION_TRANSFORM, ATL_CAMERA2_TYPE_RATIONAL,
	                        identity, 9);
	md_add_f32(md, ACAMERA_COLOR_CORRECTION_GAINS, (const float[]){1.0f, 1.0f, 1.0f, 1.0f}, 4);
}

/*
 * A videotestsrc has no sensor to read, so the result is the request read back
 * plus fixed, plausible readings. Apps use these to drive their exposure/ISO
 * readouts and to wait for AE/AF to converge, which here is immediate.
 */
static struct atl_camera_metadata *gst_camera2_get_result_metadata(struct atl_camera *camera,
                                                                   const struct atl_camera_metadata *request)
{
	struct atl_camera_metadata *md = atl_camera_metadata_new();
	int64_t frame_duration;
	int width, height;

	if (!md)
		return NULL;

	for (unsigned i = 0; i < G_N_ELEMENTS(gst_camera2_echo_tags); i++) {
		const struct atl_camera_metadata_entry *entry =
		    atl_camera_metadata_find(request, gst_camera2_echo_tags[i]);

		if (entry)
			atl_camera_metadata_add(md, entry->tag, entry->type, entry->data, entry->count);
	}

	md_default_u8(md, ACAMERA_CONTROL_MODE, ACAMERA_CONTROL_MODE_AUTO);
	md_default_u8(md, ACAMERA_CONTROL_AE_MODE, ACAMERA_CONTROL_AE_MODE_ON);
	md_default_u8(md, ACAMERA_CONTROL_AF_MODE, ACAMERA_CONTROL_AF_MODE_OFF);
	md_default_u8(md, ACAMERA_CONTROL_AWB_MODE, ACAMERA_CONTROL_AWB_MODE_AUTO);

	/* nothing converges or hunts on a synthetic source: it is always settled */
	md_add_u8(md, ACAMERA_CONTROL_AE_STATE, (const uint8_t[]){ACAMERA_CONTROL_AE_STATE_CONVERGED}, 1);
	md_add_u8(md, ACAMERA_CONTROL_AF_STATE, (const uint8_t[]){ACAMERA_CONTROL_AF_STATE_INACTIVE}, 1);
	md_add_u8(md, ACAMERA_CONTROL_AWB_STATE, (const uint8_t[]){ACAMERA_CONTROL_AWB_STATE_CONVERGED}, 1);
	md_add_u8(md, ACAMERA_FLASH_STATE, (const uint8_t[]){ACAMERA_FLASH_STATE_UNAVAILABLE}, 1);
	md_add_f32(md, ACAMERA_LENS_FOCAL_LENGTH, (const float[]){2.77f}, 1);

	g_mutex_lock(&camera->lock);
	width = camera->width;
	height = camera->height;
	frame_duration = camera->fps_max > 0 ? 1000000000LL / (camera->fps_max / 1000) : 33333333LL;
	g_mutex_unlock(&camera->lock);

	md_add_i64(md, ACAMERA_SENSOR_FRAME_DURATION, &frame_duration, 1);
	/* the whole exposure a frame at this rate could have used */
	md_add_i64(md, ACAMERA_SENSOR_EXPOSURE_TIME, &frame_duration, 1);
	md_add_i32(md, ACAMERA_SENSOR_SENSITIVITY, (const int32_t[]){100}, 1);

	if (!atl_camera_metadata_find(md, ACAMERA_SCALER_CROP_REGION))
		md_add_i32(md, ACAMERA_SCALER_CROP_REGION, (const int32_t[]){0, 0, width, height}, 4);

	md_add_statistics(md, width, height);

	return md;
}

static const char *const *gst_camera2_get_id_list(int *count)
{
	*count = gst_camera_get_count();
	return gst_camera2_ids;
}

static const uint32_t *gst_camera2_get_available_keys(const char *id, int which, int *count)
{
	if (gst_camera2_index_of(id) < 0) {
		*count = 0;
		return NULL;
	}

	switch (which) {
	case ATL_CAMERA2_KEYS_CHARACTERISTICS:
		if (!gst_camera2_characteristics_keys) {
			/* the list is a by-product of building the metadata */
			struct atl_camera_metadata *md = gst_camera2_get_static_metadata(id);
			atl_camera_metadata_free(md);
		}
		*count = gst_camera2_n_characteristics_keys;
		return gst_camera2_characteristics_keys;
	case ATL_CAMERA2_KEYS_REQUEST:
		*count = (int)G_N_ELEMENTS(gst_camera2_request_keys);
		return gst_camera2_request_keys;
	case ATL_CAMERA2_KEYS_RESULT:
		*count = (int)G_N_ELEMENTS(gst_camera2_result_keys);
		return gst_camera2_result_keys;
	default:
		*count = 0;
		return NULL;
	}
}

static const struct atl_camera_backend gst_backend = {
	.name = "gst",
	.get_camera_count = gst_camera_get_count,
	.get_camera_info = gst_camera_get_info,
	.open = gst_camera_open,
	.close = gst_camera_close,
	.get_caps = gst_camera_get_caps,
	.set_preview_size = gst_camera_set_preview_size,
	.set_preview_format = gst_camera_set_preview_format,
	.set_fps_range = gst_camera_set_fps_range,
	.set_frame_callback = gst_camera_set_frame_callback,
	.set_error_callback = gst_camera_set_error_callback,
	.start_preview = gst_camera_start_preview,
	.stop_preview = gst_camera_stop_preview,
	.take_picture = gst_camera_take_picture,
	.autofocus = gst_camera_autofocus,
	.cancel_autofocus = gst_camera_cancel_autofocus,
	.set_display_orientation = gst_camera_set_display_orientation,
	.get_camera2_id_list = gst_camera2_get_id_list,
	.get_static_metadata = gst_camera2_get_static_metadata,
	.get_available_keys = gst_camera2_get_available_keys,
	.get_result_metadata = gst_camera2_get_result_metadata,
};

const struct atl_camera_backend *atl_camera_backend_gst_get(void)
{
	if (!atl_gst_ensure_init())
		return NULL;
	return &gst_backend;
}
