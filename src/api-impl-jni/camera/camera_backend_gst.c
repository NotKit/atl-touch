/*
 * GStreamer camera backend: a synthetic desktop camera.
 *
 * Pipeline: <ATL_CAMERA_GST_SRC, default "videotestsrc is-live=true">
 *           ! videoconvert ! capsfilter (NV21, WxH, fps) ! appsink
 *
 * One back-facing camera (id 0). ATL_CAMERA_DUMP_FRAMES=<dir> writes every
 * 30th frame as PNG plus a running "frame-count" file — the test loop's eyes.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include "../media/atl_gst.h"
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
	.zoom_supported = false,
	.max_zoom = 0,
};

static GstCaps *make_preview_caps(struct atl_camera *camera)
{
	return gst_caps_new_simple("video/x-raw",
	                           "format", G_TYPE_STRING, "NV21",
	                           "width", G_TYPE_INT, camera->width,
	                           "height", G_TYPE_INT, camera->height,
	                           "framerate", GST_TYPE_FRACTION, camera->fps_max / 1000, 1,
	                           NULL);
}

static void maybe_dump_frame(struct atl_camera *camera, const uint8_t *nv21, int width, int height, int stride)
{
	char path[512];
	FILE *f;

	if (!camera->dump_dir)
		return;

	snprintf(path, sizeof(path), "%s/frame-count", camera->dump_dir);
	f = fopen(path, "w");
	if (f) {
		fprintf(f, "%" G_GUINT64_FORMAT "\n", camera->frame_count);
		fclose(f);
	}

	if (camera->frame_count % 30 != 1)
		return;

	uint8_t *rgba = malloc((size_t)width * height * 4);
	if (!rgba)
		return;
	atl_camera_nv21_to_rgba(nv21, width, height, stride, rgba);
	snprintf(path, sizeof(path), "%s/frame-%06" G_GUINT64_FORMAT ".png", camera->dump_dir, camera->frame_count);
	if (!atl_camera_write_png(path, rgba, width, height))
		fprintf(stderr, "Camera gst: failed to write %s\n", path);
	free(rgba);
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
	atl_camera_frame_cb cb = camera->frame_cb;
	void *cb_user = camera->frame_user;
	g_mutex_unlock(&camera->lock);
	if (cb)
		cb(nv21, width, height, stride, cb_user);

	maybe_dump_frame(camera, nv21, width, height, stride);

	gst_video_frame_unmap(&frame);
	gst_sample_unref(sample);
	return GST_FLOW_OK;
}

/* sync handler: a GLib main loop is not guaranteed to be running */
static GstBusSyncReply on_bus_message(GstBus *bus, GstMessage *message, gpointer user)
{
	struct atl_camera *camera = user;

	if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
		GError *error = NULL;
		gst_message_parse_error(message, &error, NULL);
		fprintf(stderr, "Camera gst: pipeline error: %s\n", error ? error->message : "(no detail)");
		g_clear_error(&error);

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

static int gst_camera_get_count(void)
{
	return 1;
}

static bool gst_camera_get_info(int id, int *facing, int *orientation)
{
	if (id != 0)
		return false;
	*facing = ATL_CAMERA_FACING_BACK;
	*orientation = 0;
	return true;
}

static void gst_camera_close(struct atl_camera *camera);

static struct atl_camera *gst_camera_open(int id)
{
	const char *src_desc = getenv("ATL_CAMERA_GST_SRC");
	GError *error = NULL;

	if (id != 0)
		return NULL;
	if (!src_desc || !*src_desc)
		src_desc = "videotestsrc is-live=true";

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

	GstCaps *caps = make_preview_caps(camera);
	g_object_set(camera->capsfilter, "caps", caps, NULL);
	gst_caps_unref(caps);
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

static void gst_camera_close(struct atl_camera *camera)
{
	if (camera->pipeline) {
		gst_element_set_state(camera->pipeline, GST_STATE_NULL);
		gst_object_unref(camera->pipeline);
	}
	g_mutex_clear(&camera->lock);
	g_free(camera->dump_dir);
	free(camera->pack_buf);
	free(camera);
}

static const struct atl_camera_caps *gst_camera_get_caps(struct atl_camera *camera)
{
	return &gst_camera_caps;
}

/* push current width/height/fps to the capsfilter; live pipelines renegotiate */
static bool gst_camera_apply_caps(struct atl_camera *camera)
{
	GstCaps *caps = make_preview_caps(camera);
	g_object_set(camera->capsfilter, "caps", caps, NULL);
	gst_caps_unref(caps);
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

static bool gst_camera_take_picture(struct atl_camera *camera, int width, int height,
                                    int jpeg_quality, atl_camera_jpeg_cb cb, void *user)
{
	fprintf(stderr, "Camera gst: take_picture not implemented yet (US-007)\n");
	return false;
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
};

const struct atl_camera_backend *atl_camera_backend_gst_get(void)
{
	if (!atl_gst_ensure_init())
		return NULL;
	return &gst_backend;
}
