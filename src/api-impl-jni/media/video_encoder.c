/*
 * H.264 encoding for recording Surfaces, see video_encoder.h.
 *
 * Pipeline: appsrc (NV21, the producer's size) ! videoconvert ! videoscale
 *           ! capsfilter (the encoder's size) ! x264enc/openh264enc
 *           ! h264parse ! mp4mux ! filesink   (file mode)
 *           ! capsfilter (byte-stream/au) ! appsink  (sample mode)
 *
 * The producer's thread only pushes buffers; everything else happens on the
 * pipeline's own streaming threads. The bus is polled synchronously in
 * finish(), so no GLib main loop is needed for the MP4 to be finalised.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include "../defines.h"

#include "../camera/camera_frame.h"
#include "atl_gst.h"
#include "video_encoder.h"

#define DEFAULT_FPS     30
#define DEFAULT_BITRATE 4000000

struct atl_video_encoder {
	gint refcount;

	GstElement *pipeline;
	GstElement *appsrc;
	GstElement *appsink; /* sample mode only */

	int width;
	int height;
	int fps;
	int bitrate;
	char *path; /* file mode only */

	GMutex lock;
	bool started;
	bool eos_sent;
	bool finished;
	int src_width; /* the caps the appsrc was set up with */
	int src_height;
	int64_t base_timestamp;
	uint64_t frames;

	uint8_t *pack; /* scratch for a strided frame */
	size_t pack_size;

	uint8_t *csd; /* SPS+PPS of the first access unit (Annex B) */
	size_t csd_size;
};

/* x264 first, since it is what every desktop has; openh264 is the fallback
 * the flatpak-ish installs ship instead */
static GstElement *make_encoder(int fps, int bitrate)
{
	GstElement *encoder = gst_element_factory_make("x264enc", NULL);

	if (encoder) {
		g_object_set(encoder, "bitrate", bitrate / 1000, "speed-preset", 1 /* ultrafast */,
		             "tune", 0x4 /* zerolatency */, "key-int-max", fps, NULL);
		return encoder;
	}
	encoder = gst_element_factory_make("openh264enc", NULL);
	if (encoder) {
		g_object_set(encoder, "bitrate", bitrate, "gop-size", fps, NULL);
		return encoder;
	}
	fprintf(stderr, "video encoder: no H.264 encoder (x264enc or openh264enc) available\n");
	return NULL;
}

static bool build_pipeline(struct atl_video_encoder *encoder)
{
	GstElement *convert, *scale, *scale_caps, *h264, *parse;
	GstCaps *caps;

	encoder->pipeline = gst_pipeline_new("atl-video-encoder");
	encoder->appsrc = gst_element_factory_make("appsrc", NULL);
	convert = gst_element_factory_make("videoconvert", NULL);
	scale = gst_element_factory_make("videoscale", NULL);
	scale_caps = gst_element_factory_make("capsfilter", NULL);
	parse = gst_element_factory_make("h264parse", NULL);
	h264 = make_encoder(encoder->fps, encoder->bitrate);

	if (!encoder->pipeline || !encoder->appsrc || !convert || !scale || !scale_caps || !parse || !h264) {
		fprintf(stderr, "video encoder: missing elements (appsrc/videoconvert/videoscale/h264parse)\n");
		g_clear_object(&encoder->appsrc);
		g_clear_object(&convert);
		g_clear_object(&scale);
		g_clear_object(&scale_caps);
		g_clear_object(&parse);
		g_clear_object(&h264);
		goto fail;
	}

	caps = gst_caps_new_simple("video/x-raw",
	                           "width", G_TYPE_INT, encoder->width,
	                           "height", G_TYPE_INT, encoder->height, NULL);
	g_object_set(scale_caps, "caps", caps, NULL);
	gst_caps_unref(caps);

	g_object_set(encoder->appsrc, "format", GST_FORMAT_TIME, "is-live", TRUE,
	             "do-timestamp", FALSE, "block", FALSE, NULL);

	gst_bin_add_many(GST_BIN(encoder->pipeline), encoder->appsrc, convert, scale, scale_caps,
	                 h264, parse, NULL);
	if (!gst_element_link_many(encoder->appsrc, convert, scale, scale_caps, h264, parse, NULL)) {
		fprintf(stderr, "video encoder: failed to link the encoding chain\n");
		goto fail;
	}

	if (encoder->path) {
		GstElement *mux = gst_element_factory_make("mp4mux", NULL);
		GstElement *sink = gst_element_factory_make("filesink", NULL);

		if (!mux || !sink) {
			fprintf(stderr, "video encoder: missing mp4mux or filesink\n");
			g_clear_object(&mux);
			g_clear_object(&sink);
			goto fail;
		}
		g_object_set(sink, "location", encoder->path, NULL);
		gst_bin_add_many(GST_BIN(encoder->pipeline), mux, sink, NULL);
		if (!gst_element_link_many(parse, mux, sink, NULL)) {
			fprintf(stderr, "video encoder: failed to link the muxing chain\n");
			goto fail;
		}
	} else {
		GstElement *au_caps = gst_element_factory_make("capsfilter", NULL);

		encoder->appsink = gst_element_factory_make("appsink", NULL);
		if (!au_caps || !encoder->appsink) {
			fprintf(stderr, "video encoder: missing capsfilter or appsink\n");
			g_clear_object(&au_caps);
			goto fail;
		}
		/* what MediaCodec hands an app: Annex B access units, SPS/PPS in band */
		caps = gst_caps_new_simple("video/x-h264",
		                           "stream-format", G_TYPE_STRING, "byte-stream",
		                           "alignment", G_TYPE_STRING, "au", NULL);
		g_object_set(au_caps, "caps", caps, NULL);
		gst_caps_unref(caps);
		g_object_set(encoder->appsink, "max-buffers", 8, "drop", FALSE, "sync", FALSE, NULL);

		gst_bin_add_many(GST_BIN(encoder->pipeline), au_caps, encoder->appsink, NULL);
		if (!gst_element_link_many(parse, au_caps, encoder->appsink, NULL)) {
			fprintf(stderr, "video encoder: failed to link the sample chain\n");
			goto fail;
		}
	}
	return true;

fail:
	g_clear_object(&encoder->pipeline);
	encoder->pipeline = NULL;
	encoder->appsrc = NULL;
	encoder->appsink = NULL;
	return false;
}

struct atl_video_encoder *atl_video_encoder_new(int width, int height, int fps, int bitrate,
                                                const char *output_path)
{
	struct atl_video_encoder *encoder;

	if (width < 2 || height < 2)
		return NULL;
	if (!atl_gst_ensure_init())
		return NULL;

	encoder = calloc(1, sizeof(*encoder));
	encoder->refcount = 1;
	/* even sizes only: H.264 chroma is subsampled, and so is NV21 */
	encoder->width = width & ~1;
	encoder->height = height & ~1;
	encoder->fps = fps > 0 ? fps : DEFAULT_FPS;
	encoder->bitrate = bitrate > 0 ? bitrate : DEFAULT_BITRATE;
	encoder->path = output_path ? strdup(output_path) : NULL;
	encoder->base_timestamp = -1;
	g_mutex_init(&encoder->lock);

	if (!build_pipeline(encoder)) {
		atl_video_encoder_unref(encoder);
		return NULL;
	}
	return encoder;
}

void atl_video_encoder_ref(struct atl_video_encoder *encoder)
{
	if (encoder)
		g_atomic_int_inc(&encoder->refcount);
}

void atl_video_encoder_unref(struct atl_video_encoder *encoder)
{
	if (!encoder || !g_atomic_int_dec_and_test(&encoder->refcount))
		return;

	if (encoder->pipeline) {
		gst_element_set_state(encoder->pipeline, GST_STATE_NULL);
		gst_object_unref(encoder->pipeline);
	}
	g_mutex_clear(&encoder->lock);
	free(encoder->pack);
	free(encoder->csd);
	free(encoder->path);
	free(encoder);
}

struct atl_video_encoder *atl_video_encoder_from_surface(JNIEnv *env, jobject surface)
{
	struct atl_video_encoder *encoder;

	if (!surface)
		return NULL;
	encoder = _PTR(_GET_LONG_FIELD(surface, "videoEncoderPtr"));
	if (!encoder)
		return NULL;
	g_atomic_int_inc(&encoder->refcount);
	return encoder;
}

void atl_video_encoder_attach_surface(JNIEnv *env, struct atl_video_encoder *encoder, jobject surface)
{
	if (encoder && surface)
		_SET_LONG_FIELD(surface, "videoEncoderPtr", _INTPTR(encoder));
}

void atl_video_encoder_get_size(struct atl_video_encoder *encoder, int *width, int *height)
{
	*width = encoder->width;
	*height = encoder->height;
}

uint64_t atl_video_encoder_get_frame_count(struct atl_video_encoder *encoder)
{
	uint64_t frames;

	if (!encoder)
		return 0;
	g_mutex_lock(&encoder->lock);
	frames = encoder->frames;
	g_mutex_unlock(&encoder->lock);
	return frames;
}

bool atl_video_encoder_start(struct atl_video_encoder *encoder)
{
	if (!encoder)
		return false;

	g_mutex_lock(&encoder->lock);
	if (encoder->started || encoder->finished) {
		g_mutex_unlock(&encoder->lock);
		return encoder->started;
	}
	if (gst_element_set_state(encoder->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		g_mutex_unlock(&encoder->lock);
		fprintf(stderr, "video encoder: failed to start the pipeline\n");
		return false;
	}
	encoder->started = true;
	g_mutex_unlock(&encoder->lock);
	return true;
}

/* the appsrc caps follow the first frame's size; the pipeline scales from
 * there to the encoder's own size */
static void set_source_caps_locked(struct atl_video_encoder *encoder, int width, int height)
{
	GstCaps *caps;

	if (encoder->src_width == width && encoder->src_height == height)
		return;

	caps = gst_caps_new_simple("video/x-raw",
	                           "format", G_TYPE_STRING, "NV21",
	                           "width", G_TYPE_INT, width,
	                           "height", G_TYPE_INT, height,
	                           "framerate", GST_TYPE_FRACTION, encoder->fps, 1, NULL);
	gst_app_src_set_caps(GST_APP_SRC(encoder->appsrc), caps);
	gst_caps_unref(caps);
	encoder->src_width = width;
	encoder->src_height = height;
}

void atl_video_encoder_submit(struct atl_video_encoder *encoder, const uint8_t *nv21,
                             int width, int height, int stride, int64_t timestamp)
{
	size_t size = (size_t)width * height * 3 / 2;
	GstBuffer *buffer;
	int64_t pts;

	if (!encoder || !nv21)
		return;

	g_mutex_lock(&encoder->lock);
	if (!encoder->started || encoder->finished) {
		g_mutex_unlock(&encoder->lock);
		return;
	}
	set_source_caps_locked(encoder, width, height);
	if (encoder->base_timestamp < 0)
		encoder->base_timestamp = timestamp;
	pts = timestamp - encoder->base_timestamp;

	if (stride != width) {
		if (encoder->pack_size < size) {
			free(encoder->pack);
			encoder->pack = malloc(size);
			encoder->pack_size = encoder->pack ? size : 0;
		}
		if (!encoder->pack) {
			g_mutex_unlock(&encoder->lock);
			return;
		}
		atl_camera_nv21_pack(encoder->pack, nv21, width, height, stride);
		nv21 = encoder->pack;
	}

	buffer = gst_buffer_new_allocate(NULL, size, NULL);
	gst_buffer_fill(buffer, 0, nv21, size);
	GST_BUFFER_PTS(buffer) = pts;
	GST_BUFFER_DTS(buffer) = pts;
	GST_BUFFER_DURATION(buffer) = GST_SECOND / encoder->fps;
	encoder->frames++;
	g_mutex_unlock(&encoder->lock);

	if (gst_app_src_push_buffer(GST_APP_SRC(encoder->appsrc), buffer) != GST_FLOW_OK)
		fprintf(stderr, "video encoder: the pipeline refused a frame\n");
}

/* SPS (type 7) and PPS (type 8) of an Annex B access unit, up to the first
 * slice: what MediaCodec reports as csd-0 */
static void remember_csd(struct atl_video_encoder *encoder, const uint8_t *data, size_t size)
{
	if (encoder->csd || size < 5)
		return;

	for (size_t i = 0; i + 4 < size; i++) {
		size_t start;
		int type;

		if (!data[i] && !data[i + 1] && data[i + 2] == 1)
			start = 3;
		else if (!data[i] && !data[i + 1] && !data[i + 2] && data[i + 3] == 1)
			start = 4;
		else
			continue;

		type = data[i + start] & 0x1f;
		if (type >= 1 && type <= 5) { /* the first slice: the csd is everything before it */
			if (!i)
				return;
			encoder->csd = malloc(i);
			if (encoder->csd) {
				memcpy(encoder->csd, data, i);
				encoder->csd_size = i;
			}
			return;
		}
		i += start - 1; /* the loop's ++ lands on the NAL header */
	}
}

int atl_video_encoder_pull(struct atl_video_encoder *encoder, uint8_t *out, size_t capacity,
                           int64_t *pts_us, bool *keyframe, int64_t timeout_us)
{
	GstSample *sample;
	GstBuffer *buffer;
	GstMapInfo map;
	int ret;

	if (!encoder || !encoder->appsink)
		return ATL_ENCODER_EOS;

	sample = gst_app_sink_try_pull_sample(GST_APP_SINK(encoder->appsink),
	                                      timeout_us > 0 ? timeout_us * GST_USECOND : 0);
	if (!sample)
		return gst_app_sink_is_eos(GST_APP_SINK(encoder->appsink)) ? ATL_ENCODER_EOS
		                                                           : ATL_ENCODER_AGAIN;

	buffer = gst_sample_get_buffer(sample);
	if (!buffer || !gst_buffer_map(buffer, &map, GST_MAP_READ)) {
		gst_sample_unref(sample);
		return ATL_ENCODER_AGAIN;
	}
	if (map.size > capacity) {
		fprintf(stderr, "video encoder: a %zu byte access unit does not fit in %zu bytes\n",
		        map.size, capacity);
		gst_buffer_unmap(buffer, &map);
		gst_sample_unref(sample);
		return ATL_ENCODER_AGAIN;
	}
	memcpy(out, map.data, map.size);
	ret = (int)map.size;
	*pts_us = GST_BUFFER_PTS_IS_VALID(buffer) ? (int64_t)(GST_BUFFER_PTS(buffer) / GST_USECOND) : 0;
	*keyframe = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

	g_mutex_lock(&encoder->lock);
	remember_csd(encoder, map.data, map.size);
	g_mutex_unlock(&encoder->lock);

	gst_buffer_unmap(buffer, &map);
	gst_sample_unref(sample);
	return ret;
}

const uint8_t *atl_video_encoder_get_csd(struct atl_video_encoder *encoder, size_t *size)
{
	const uint8_t *csd;

	if (!encoder)
		return NULL;
	g_mutex_lock(&encoder->lock);
	csd = encoder->csd;
	*size = encoder->csd_size;
	g_mutex_unlock(&encoder->lock);
	return csd;
}

void atl_video_encoder_signal_eos(struct atl_video_encoder *encoder)
{
	bool send;

	if (!encoder)
		return;
	g_mutex_lock(&encoder->lock);
	send = encoder->started && !encoder->finished && !encoder->eos_sent;
	encoder->eos_sent = true;
	g_mutex_unlock(&encoder->lock);

	if (send)
		gst_app_src_end_of_stream(GST_APP_SRC(encoder->appsrc));
}

/*
 * End of stream, then wait for it to come out the other end: in file mode the
 * moov atom is only written when mp4mux sees EOS, so a recording stopped
 * without this is an unplayable file.
 */
bool atl_video_encoder_finish(struct atl_video_encoder *encoder)
{
	GstBus *bus;
	GstMessage *message;
	bool ok = true;

	if (!encoder)
		return false;

	g_mutex_lock(&encoder->lock);
	if (encoder->finished || !encoder->started) {
		bool was_started = encoder->started;

		encoder->finished = true;
		g_mutex_unlock(&encoder->lock);
		if (!was_started && encoder->pipeline)
			gst_element_set_state(encoder->pipeline, GST_STATE_NULL);
		return was_started;
	}
	encoder->finished = true;
	bool eos_sent = encoder->eos_sent;
	encoder->eos_sent = true;
	uint64_t frames = encoder->frames;
	g_mutex_unlock(&encoder->lock);

	if (!eos_sent)
		gst_app_src_end_of_stream(GST_APP_SRC(encoder->appsrc));

	bus = gst_element_get_bus(encoder->pipeline);
	message = gst_bus_timed_pop_filtered(bus, 10 * GST_SECOND, GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
	if (!message) {
		fprintf(stderr, "video encoder: timed out draining the pipeline\n");
		ok = false;
	} else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
		GError *error = NULL;

		gst_message_parse_error(message, &error, NULL);
		fprintf(stderr, "video encoder: pipeline error: %s\n", error ? error->message : "(no detail)");
		g_clear_error(&error);
		ok = false;
	}
	if (message)
		gst_message_unref(message);
	gst_object_unref(bus);

	gst_element_set_state(encoder->pipeline, GST_STATE_NULL);
	fprintf(stderr, "video encoder: %" G_GUINT64_FORMAT " frames encoded at %dx%d -> %s\n",
	        frames, encoder->width, encoder->height, encoder->path ? encoder->path : "(samples)");
	return ok;
}
