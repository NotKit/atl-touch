/*
 * android.media.MediaMuxer, see video_muxer.h.
 *
 * The app's encoded buffers go in through an appsrc; h264parse turns the
 * Annex B stream MediaCodec produced into the AVC form mp4mux wants and works
 * out the track's caps, so an app that never filled in csd-0 still gets a
 * playable file. stop() is what writes the moov.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include "atl_gst.h"
#include "video_muxer.h"

struct atl_video_muxer {
	GstElement *pipeline;
	GstElement *appsrc;
	GstElement *mux;

	GMutex lock;
	bool has_track;
	bool started;
	bool stopped;
	int width;
	int height;
	int fps;
	uint64_t samples;
	char *path;
};

struct atl_video_muxer *atl_video_muxer_new(const char *path)
{
	struct atl_video_muxer *muxer;
	GstElement *parse, *sink;

	if (!path || !atl_gst_ensure_init())
		return NULL;

	muxer = calloc(1, sizeof(*muxer));
	muxer->path = strdup(path);
	muxer->fps = 30;
	g_mutex_init(&muxer->lock);

	muxer->pipeline = gst_pipeline_new("atl-video-muxer");
	muxer->appsrc = gst_element_factory_make("appsrc", NULL);
	muxer->mux = gst_element_factory_make("mp4mux", NULL);
	parse = gst_element_factory_make("h264parse", NULL);
	sink = gst_element_factory_make("filesink", NULL);

	if (!muxer->pipeline || !muxer->appsrc || !muxer->mux || !parse || !sink) {
		fprintf(stderr, "MediaMuxer: missing elements (appsrc/h264parse/mp4mux/filesink)\n");
		g_clear_object(&muxer->appsrc);
		g_clear_object(&muxer->mux);
		g_clear_object(&parse);
		g_clear_object(&sink);
		goto fail;
	}

	g_object_set(muxer->appsrc, "format", GST_FORMAT_TIME, "is-live", FALSE,
	             "do-timestamp", FALSE, "block", TRUE, NULL);
	g_object_set(sink, "location", path, NULL);

	gst_bin_add_many(GST_BIN(muxer->pipeline), muxer->appsrc, parse, muxer->mux, sink, NULL);
	if (!gst_element_link_many(muxer->appsrc, parse, muxer->mux, sink, NULL)) {
		fprintf(stderr, "MediaMuxer: failed to link the muxing chain\n");
		goto fail;
	}
	return muxer;

fail:
	g_clear_object(&muxer->pipeline);
	g_mutex_clear(&muxer->lock);
	free(muxer->path);
	free(muxer);
	return NULL;
}

bool atl_video_muxer_add_track(struct atl_video_muxer *muxer, int width, int height, int fps,
                               const uint8_t *csd, size_t csd_size)
{
	GstCaps *caps;

	if (!muxer)
		return false;

	g_mutex_lock(&muxer->lock);
	if (muxer->has_track || muxer->started) {
		g_mutex_unlock(&muxer->lock);
		fprintf(stderr, "MediaMuxer: only one track, added before start()\n");
		return false;
	}
	muxer->width = width;
	muxer->height = height;
	muxer->fps = fps > 0 ? fps : 30;

	caps = gst_caps_new_simple("video/x-h264",
	                           "stream-format", G_TYPE_STRING, "byte-stream",
	                           "alignment", G_TYPE_STRING, "au",
	                           "framerate", GST_TYPE_FRACTION, muxer->fps, 1, NULL);
	if (width > 0 && height > 0)
		gst_caps_set_simple(caps, "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
	gst_app_src_set_caps(GST_APP_SRC(muxer->appsrc), caps);
	gst_caps_unref(caps);

	/* the csd is in band in every Annex B keyframe, so it is only used to
	 * fail early on a track that carries no parameter sets at all */
	if (csd && csd_size < 4)
		fprintf(stderr, "MediaMuxer: the track's csd-0 is too short to be SPS/PPS\n");

	muxer->has_track = true;
	g_mutex_unlock(&muxer->lock);
	return true;
}

void atl_video_muxer_set_orientation(struct atl_video_muxer *muxer, int degrees)
{
	if (!muxer || !muxer->mux)
		return;
	/* mp4mux writes the rotation into the track's transform matrix */
	g_object_set(muxer->mux, "orientation-hint", (guint)(((degrees % 360) + 360) % 360), NULL);
}

bool atl_video_muxer_start(struct atl_video_muxer *muxer)
{
	if (!muxer)
		return false;

	g_mutex_lock(&muxer->lock);
	if (!muxer->has_track) {
		g_mutex_unlock(&muxer->lock);
		fprintf(stderr, "MediaMuxer: start() without a track\n");
		return false;
	}
	if (muxer->started) {
		g_mutex_unlock(&muxer->lock);
		return true;
	}
	if (gst_element_set_state(muxer->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		g_mutex_unlock(&muxer->lock);
		fprintf(stderr, "MediaMuxer: failed to start the pipeline\n");
		return false;
	}
	muxer->started = true;
	g_mutex_unlock(&muxer->lock);
	return true;
}

bool atl_video_muxer_write(struct atl_video_muxer *muxer, const uint8_t *data, size_t size,
                           int64_t pts_us, bool keyframe)
{
	GstBuffer *buffer;

	if (!muxer || !data || !size)
		return false;

	g_mutex_lock(&muxer->lock);
	if (!muxer->started || muxer->stopped) {
		g_mutex_unlock(&muxer->lock);
		fprintf(stderr, "MediaMuxer: writeSampleData outside start()/stop()\n");
		return false;
	}
	muxer->samples++;
	g_mutex_unlock(&muxer->lock);

	buffer = gst_buffer_new_allocate(NULL, size, NULL);
	gst_buffer_fill(buffer, 0, data, size);
	GST_BUFFER_PTS(buffer) = pts_us * GST_USECOND;
	GST_BUFFER_DTS(buffer) = pts_us * GST_USECOND;
	if (!keyframe)
		GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

	return gst_app_src_push_buffer(GST_APP_SRC(muxer->appsrc), buffer) == GST_FLOW_OK;
}

bool atl_video_muxer_stop(struct atl_video_muxer *muxer)
{
	GstBus *bus;
	GstMessage *message;
	bool ok = true;
	uint64_t samples;

	if (!muxer)
		return false;

	g_mutex_lock(&muxer->lock);
	if (!muxer->started || muxer->stopped) {
		g_mutex_unlock(&muxer->lock);
		return false;
	}
	muxer->stopped = true;
	samples = muxer->samples;
	g_mutex_unlock(&muxer->lock);

	gst_app_src_end_of_stream(GST_APP_SRC(muxer->appsrc));

	bus = gst_element_get_bus(muxer->pipeline);
	message = gst_bus_timed_pop_filtered(bus, 10 * GST_SECOND, GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
	if (!message) {
		fprintf(stderr, "MediaMuxer: timed out writing %s\n", muxer->path);
		ok = false;
	} else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
		GError *error = NULL;

		gst_message_parse_error(message, &error, NULL);
		fprintf(stderr, "MediaMuxer: pipeline error: %s\n", error ? error->message : "(no detail)");
		g_clear_error(&error);
		ok = false;
	}
	if (message)
		gst_message_unref(message);
	gst_object_unref(bus);

	gst_element_set_state(muxer->pipeline, GST_STATE_NULL);
	fprintf(stderr, "MediaMuxer: %" G_GUINT64_FORMAT " samples written to %s\n", samples, muxer->path);
	return ok;
}

void atl_video_muxer_free(struct atl_video_muxer *muxer)
{
	if (!muxer)
		return;
	if (muxer->pipeline) {
		gst_element_set_state(muxer->pipeline, GST_STATE_NULL);
		gst_object_unref(muxer->pipeline);
	}
	g_mutex_clear(&muxer->lock);
	free(muxer->path);
	free(muxer);
}
