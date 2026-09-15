/*
 * The camera2 stream session, see camera_streams.h.
 *
 * Two shapes behind one interface:
 *
 *  - a backend with configure_streams (camera2ndk) is a real camera2 device:
 *    every stream is a HAL stream, the buffers are the HAL's own and the
 *    events are the HAL's frame numbers and timestamps. This file only keeps
 *    the list of one-shot requests in flight, so a flush can name them.
 *
 *  - a backend with one NV21 preview stream (gst, hybris) is driven the way
 *    Camera1 drives it, at the size of the largest stream, and every frame is
 *    converted into each targeted stream: YUV_420_888 and PRIVATE keep the
 *    NV21 layout, JPEG is encoded at the request's quality and orientation,
 *    and RAW10/RAW_SENSOR get the frame's luma on a Bayer grid, because a
 *    backend like that has no raw data (see raw_fill). One frame is one
 *    capture: a started event, the buffers, then the result, with a frame
 *    number and a timestamp of this file's making.
 *
 * Buffers of the emulation come from a pool per stream, max_buffers deep: the
 * consumer holding all of them means the stream drops frames, which is what a
 * full BufferQueue does.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <third_party/android-headers/camera/NdkCameraMetadataTags.h>

#include "camera2_metadata.h"
#include "camera_frame.h"
#include "camera_record.h"
#include "camera_streams.h"

#define JPEG_QUALITY_DEFAULT 90

#define COST_PERIOD 120

struct emu_buffer {
	struct atl_camera_buffer pub;
	struct emu_buffer *next;
	struct atl_camera_streams *owner; /* holds a reference while out */
	int generation;                   /* the configuration it was made for */
	uint8_t *data;
	size_t capacity;
};

struct emu_stream {
	struct atl_camera_stream config;
	struct emu_buffer *free_list;
	int outstanding; /* buffers the consumer holds */
	/* which source column each output column samples, built once per frame:
	 * the raw path does that division 12 million times otherwise */
	int *cols;
	size_t cols_capacity;
	uint64_t delivered;
	uint64_t dropped;
	int64_t micros; /* what filling it cost since the last report */
};

struct pending_request {
	struct atl_camera_metadata *settings;
	int id;
	uint32_t targets;
};

struct atl_camera_streams {
	gint refcount;
	const struct atl_camera_backend *backend;
	struct atl_camera *camera;
	struct atl_camera_stream_callbacks callbacks;
	void *user;

	GMutex lock;
	bool real;       /* the backend serves the streams itself */
	bool has_input;  /* and it was given a reprocessing input stream */
	bool configured;
	int generation;

	/* the one-shot requests not completed yet, oldest first: both modes keep
	 * it, so a flush can say which ids will never complete */
	GArray *oneshots;

	/* --- the emulation --- */
	struct emu_stream streams[ATL_CAMERA_MAX_STREAMS];
	int n_streams;
	struct atl_camera_metadata *repeating;
	int repeating_id;
	uint32_t repeating_targets;
	GQueue *pending; /* struct pending_request *, oldest first */
	int64_t frame_number;
	bool previewing;

	GMutex fill_lock; /* the scratch buffers, held across a JPEG encode */
	uint8_t *scratch_nv21;
	size_t scratch_nv21_capacity;
	uint8_t *scratch_rgba;
	size_t scratch_rgba_capacity;
	uint8_t *scratch_rotated;
	size_t scratch_rotated_capacity;

	uint64_t frames;
	int64_t cost_since;
};

#define REQUEST_NONE (-1)

static void streams_unref(struct atl_camera_streams *s);

/* --- the one-shot list ---------------------------------------------------- */

/* call with the lock held */
static void oneshot_add_locked(struct atl_camera_streams *s, int id)
{
	g_array_append_val(s->oneshots, id);
}

static void oneshot_done_locked(struct atl_camera_streams *s, int id)
{
	for (guint i = 0; i < s->oneshots->len; i++) {
		if (g_array_index(s->oneshots, int, i) == id) {
			g_array_remove_index(s->oneshots, i);
			return;
		}
	}
}

/* the backend's events, with the one-shot bookkeeping on the way through */
static void on_started(void *user, int request_id, int64_t frame_number, int64_t timestamp)
{
	struct atl_camera_streams *s = user;

	atl_camera_record_started(s, request_id, frame_number, timestamp);
	if (s->callbacks.started)
		s->callbacks.started(s->user, request_id, frame_number, timestamp);
}

static void on_result(void *user, int request_id, int64_t frame_number,
                      struct atl_camera_metadata *result)
{
	struct atl_camera_streams *s = user;

	atl_camera_record_result(s, request_id, frame_number, result);
	g_mutex_lock(&s->lock);
	oneshot_done_locked(s, request_id);
	g_mutex_unlock(&s->lock);
	if (s->callbacks.result)
		s->callbacks.result(s->user, request_id, frame_number, result);
	else
		atl_camera_metadata_free(result);
}

static void on_failed(void *user, int request_id, int64_t frame_number)
{
	struct atl_camera_streams *s = user;

	atl_camera_record_failed(s, request_id, frame_number);
	g_mutex_lock(&s->lock);
	oneshot_done_locked(s, request_id);
	g_mutex_unlock(&s->lock);
	if (s->callbacks.failed)
		s->callbacks.failed(s->user, request_id, frame_number);
}

static void on_buffer(void *user, struct atl_camera_buffer *buffer)
{
	struct atl_camera_streams *s = user;

	atl_camera_record_buffer(s, buffer);
	if (s->callbacks.buffer)
		s->callbacks.buffer(s->user, buffer);
	else
		atl_camera_buffer_release(buffer);
}

static void on_buffer_lost(void *user, int request_id, int64_t frame_number, int stream)
{
	struct atl_camera_streams *s = user;

	atl_camera_record_lost(s, request_id, frame_number, stream);
	if (s->callbacks.buffer_lost)
		s->callbacks.buffer_lost(s->user, request_id, frame_number, stream);
}

static const struct atl_camera_stream_callbacks relay = {
	.started = on_started,
	.result = on_result,
	.failed = on_failed,
	.buffer = on_buffer,
	.buffer_lost = on_buffer_lost,
};

/* --- the emulation: buffers --------------------------------------------- */

static void emu_buffer_free(struct emu_buffer *buffer)
{
	free(buffer->data);
	free(buffer);
}

static void emu_buffer_release(struct atl_camera_buffer *pub)
{
	struct emu_buffer *buffer = (struct emu_buffer *)pub;
	struct atl_camera_streams *s = buffer->owner;
	bool pooled = false;

	g_mutex_lock(&s->lock);
	if (s->configured && buffer->generation == s->generation && pub->stream < s->n_streams) {
		struct emu_stream *stream = &s->streams[pub->stream];

		buffer->next = stream->free_list;
		stream->free_list = buffer;
		stream->outstanding--;
		pooled = true;
	}
	g_mutex_unlock(&s->lock);

	if (!pooled)
		emu_buffer_free(buffer);
	streams_unref(s);
}

/* a buffer of the stream, or NULL when the consumer holds them all; call with
 * the lock held */
static struct emu_buffer *emu_buffer_take_locked(struct atl_camera_streams *s, int index)
{
	struct emu_stream *stream = &s->streams[index];
	struct emu_buffer *buffer = stream->free_list;

	/* one more than the consumer may hold, so the drop is the consumer's:
	 * a reader full of images releases the new one at once and counts it,
	 * the way a BufferQueue's consumer end does */
	if (stream->outstanding > stream->config.max_buffers) {
		stream->dropped++;
		return NULL;
	}
	if (buffer)
		stream->free_list = buffer->next;
	else
		buffer = calloc(1, sizeof(*buffer));
	buffer->next = NULL;
	buffer->owner = s;
	buffer->generation = s->generation;
	stream->outstanding++;
	g_atomic_int_inc(&s->refcount);

	memset(&buffer->pub, 0, sizeof(buffer->pub));
	buffer->pub.stream = index;
	buffer->pub.width = stream->config.width;
	buffer->pub.height = stream->config.height;
	buffer->pub.format = stream->config.format;
	buffer->pub.release = emu_buffer_release;
	buffer->pub.owner = s;
	return buffer;
}

static bool emu_buffer_reserve(struct emu_buffer *buffer, size_t size)
{
	if (buffer->capacity >= size)
		return true;
	free(buffer->data);
	buffer->data = malloc(size);
	buffer->capacity = buffer->data ? size : 0;
	return buffer->data != NULL;
}

static uint8_t *scratch(uint8_t **buffer, size_t *capacity, size_t size)
{
	if (*capacity < size) {
		free(*buffer);
		*buffer = malloc(size);
		*capacity = *buffer ? size : 0;
	}
	return *buffer;
}

static void plane_set(struct atl_camera_plane *plane, uint8_t *data, int len, int row_stride,
                      int pixel_stride)
{
	plane->data = data;
	plane->len = len;
	plane->row_stride = row_stride;
	plane->pixel_stride = pixel_stride;
}

/*
 * RAW10 and RAW_SENSOR are NOT sensor data here: a one-stream backend has no
 * raw path out of its camera, so the raw plane carries the frame's luma
 * written onto the Bayer grid (10 bits per pixel, low bits zero). An app that
 * insists on a raw stream gets a real frame in the right layout rather than a
 * stream that is configured and never filled. The real thing comes from a
 * backend with streams of its own.
 */
static bool raw_fill(struct emu_stream *stream, struct emu_buffer *buffer, const uint8_t *nv21,
                     int width, int height, int stride)
{
	int out_width = stream->config.width;
	int out_height = stream->config.height;
	bool ten = stream->config.format == ATL_CAMERA_FORMAT_RAW10;
	size_t row_stride = ten ? ((size_t)out_width * 10 + 7) / 8 : (size_t)out_width * 2;
	size_t size = row_stride * (size_t)out_height;
	int *cols;
	uint8_t *out;

	if (stream->cols_capacity < (size_t)out_width) {
		free(stream->cols);
		stream->cols = malloc((size_t)out_width * sizeof(int));
		stream->cols_capacity = stream->cols ? (size_t)out_width : 0;
	}
	cols = stream->cols;
	if (!cols || !emu_buffer_reserve(buffer, size))
		return false;
	/* the mapping atl_camera_nv21_scale() uses, so the pixels are the same
	 * ones a YUV stream of this size would carry */
	for (int x = 0; x < out_width; x++)
		cols[x] = x * width / out_width;

	out = buffer->data;
	for (int y = 0; y < out_height; y++) {
		const uint8_t *luma = nv21 + (size_t)(y * height / out_height) * stride;
		uint8_t *row = out + (size_t)y * row_stride;

		if (!ten) {
			for (int x = 0; x < out_width; x++) {
				uint16_t v = (uint16_t)luma[cols[x]] << 2;

				row[x * 2] = (uint8_t)(v & 0xff);
				row[x * 2 + 1] = (uint8_t)(v >> 8);
			}
			continue;
		}
		/* RAW10 packs four pixels into five bytes: the four high bytes,
		 * then their low two bits. An 8-bit luma is the high byte and the
		 * low bits are zero. */
		for (int x = 0; x < out_width; x += 4) {
			uint8_t *group = row + (size_t)(x / 4) * 5;
			int n = out_width - x < 4 ? out_width - x : 4;

			group[4] = 0;
			for (int i = 0; i < 4; i++)
				group[i] = i < n ? luma[cols[x + i]] : 0;
		}
	}
	buffer->pub.size = size;
	buffer->pub.n_planes = 1;
	/* a packed raw plane has no pixel stride of its own */
	plane_set(&buffer->pub.planes[0], out, (int)size, (int)row_stride, ten ? 0 : 2);
	return true;
}

/* the JPEG at the request's quality, rotated by its orientation, so a 90 or 270
 * degree still decodes with its sides swapped; the buffer keeps the stream's
 * geometry either way, which for a blob format is all AOSP promises */
static bool jpeg_fill(struct atl_camera_streams *s, struct emu_stream *stream,
                      struct emu_buffer *buffer, const uint8_t *nv21, int width, int height,
                      int stride, const struct atl_camera_metadata *settings)
{
	int out_width = stream->config.width;
	int out_height = stream->config.height;
	size_t luma = (size_t)out_width * out_height;
	uint8_t *packed = scratch(&s->scratch_nv21, &s->scratch_nv21_capacity, luma * 3 / 2);
	uint8_t *rgba = scratch(&s->scratch_rgba, &s->scratch_rgba_capacity, luma * 4);
	const struct atl_camera_metadata_entry *entry;
	int quality = JPEG_QUALITY_DEFAULT, orientation = 0;
	int jpeg_width = out_width, jpeg_height = out_height;
	uint8_t *jpeg = NULL;
	size_t jpeg_size = 0;

	if (!packed || !rgba)
		return false;
	entry = atl_camera_metadata_find(settings, ACAMERA_JPEG_QUALITY);
	if (entry && entry->type == ATL_CAMERA2_TYPE_BYTE && entry->count >= 1 &&
	    ((const uint8_t *)entry->data)[0] > 0)
		quality = ((const uint8_t *)entry->data)[0];
	entry = atl_camera_metadata_find(settings, ACAMERA_JPEG_ORIENTATION);
	if (entry && entry->type == ATL_CAMERA2_TYPE_INT32 && entry->count >= 1)
		orientation = ((const int32_t *)entry->data)[0];

	atl_camera_nv21_scale(packed, out_width, out_height, nv21, width, height, stride);
	atl_camera_nv21_to_rgba(packed, out_width, out_height, out_width, rgba);
	if (orientation % 360) {
		uint8_t *rotated = scratch(&s->scratch_rotated, &s->scratch_rotated_capacity, luma * 4);

		if (!rotated)
			return false;
		atl_camera_rgba_rotate(rotated, rgba, out_width, out_height, orientation);
		if (orientation % 180) {
			jpeg_width = out_height;
			jpeg_height = out_width;
		}
		rgba = rotated;
	}
	if (!atl_camera_encode_jpeg(rgba, jpeg_width, jpeg_height, quality, &jpeg, &jpeg_size))
		return false;

	free(buffer->data);
	buffer->data = jpeg;
	buffer->capacity = jpeg_size;
	buffer->pub.size = jpeg_size;
	buffer->pub.n_planes = 1;
	/* a blob plane: the bytes are the whole image, so there are no rows */
	plane_set(&buffer->pub.planes[0], jpeg, (int)jpeg_size, 0, 1);
	return true;
}

/* NV21 scaled to the stream: YUV_420_888 reports it as three planes, the two
 * chroma ones pixelStride 2 and one byte apart, which is what a real device
 * reports too; PRIVATE keeps the bytes and reports no planes */
static bool yuv_fill(struct emu_stream *stream, struct emu_buffer *buffer, const uint8_t *nv21,
                     int width, int height, int stride)
{
	int out_width = stream->config.width;
	int out_height = stream->config.height;
	size_t luma = (size_t)out_width * out_height;
	size_t size = luma * 3 / 2;

	if (!emu_buffer_reserve(buffer, size))
		return false;
	atl_camera_nv21_scale(buffer->data, out_width, out_height, nv21, width, height, stride);
	buffer->pub.size = size;
	/* an opaque image reports no planes, but the bytes are still there for a
	 * HardwareBuffer or an ImageWriter to take */
	plane_set(&buffer->pub.planes[0], buffer->data, (int)luma, out_width, 1);
	if (stream->config.format == ATL_CAMERA_FORMAT_YUV_420_888) {
		buffer->pub.n_planes = 3;
		/* NV21 chroma is V,U: U is the odd byte, V the even one */
		plane_set(&buffer->pub.planes[1], buffer->data + luma + 1, (int)(luma / 2 - 1), out_width, 2);
		plane_set(&buffer->pub.planes[2], buffer->data + luma, (int)(luma / 2), out_width, 2);
	}
	return true;
}

static bool emu_fill(struct atl_camera_streams *s, int index, struct emu_buffer *buffer,
                     const uint8_t *nv21, int width, int height, int stride,
                     const struct atl_camera_metadata *settings)
{
	struct emu_stream *stream = &s->streams[index];
	bool ok;

	g_mutex_lock(&s->fill_lock);
	switch (stream->config.format) {
	case ATL_CAMERA_FORMAT_YUV_420_888:
	case ATL_CAMERA_FORMAT_PRIVATE:
		ok = yuv_fill(stream, buffer, nv21, width, height, stride);
		break;
	case ATL_CAMERA_FORMAT_JPEG:
		ok = jpeg_fill(s, stream, buffer, nv21, width, height, stride, settings);
		break;
	case ATL_CAMERA_FORMAT_RAW10:
	case ATL_CAMERA_FORMAT_RAW_SENSOR:
		ok = raw_fill(stream, buffer, nv21, width, height, stride);
		break;
	default:
		ok = false;
	}
	g_mutex_unlock(&s->fill_lock);
	return ok;
}

/* --- the emulation: frames ------------------------------------------------ */

static const char *format_name(int format)
{
	switch (format) {
	case ATL_CAMERA_FORMAT_RAW_SENSOR:  return "raw-sensor";
	case ATL_CAMERA_FORMAT_PRIVATE:     return "private";
	case ATL_CAMERA_FORMAT_YUV_420_888: return "yuv";
	case ATL_CAMERA_FORMAT_RAW10:       return "raw10";
	case ATL_CAMERA_FORMAT_JPEG:        return "jpeg";
	default:                            return "reader";
	}
}

/* ATL_DEBUG_PRESENT: what each stream costs to fill, every COST_PERIOD frames,
 * to sit beside the backend's own rate line. A stream at 0.0 ms is not a fast
 * one, it is one whose consumer holds every buffer. Call with the lock held. */
static void cost_report_locked(struct atl_camera_streams *s)
{
	static int on = -1;
	int64_t now;
	GString *line;

	if (on < 0)
		on = getenv("ATL_DEBUG_PRESENT") != NULL;
	if (!on || ++s->frames % COST_PERIOD)
		return;
	now = g_get_monotonic_time();
	if (!s->cost_since) {
		for (int i = 0; i < s->n_streams; i++)
			s->streams[i].micros = s->streams[i].delivered = 0;
		s->cost_since = now;
		return;
	}
	line = g_string_new(NULL);
	g_string_append_printf(line, "camera2: %d frames delivered in %.1fs (emulated streams),",
	                       COST_PERIOD, (now - s->cost_since) / 1e6);
	for (int i = 0; i < s->n_streams; i++) {
		struct emu_stream *stream = &s->streams[i];

		g_string_append_printf(line, " %s(0x%x) %dx%d %.1f ms x%" G_GUINT64_FORMAT ",",
		                       format_name(stream->config.format), stream->config.format,
		                       stream->config.width, stream->config.height,
		                       stream->delivered ? stream->micros / 1000.0 / stream->delivered : 0.0,
		                       stream->delivered);
		stream->micros = 0;
		stream->delivered = 0;
	}
	line->str[line->len - 1] = '\n';
	fputs(line->str, stderr);
	g_string_free(line, TRUE);
	s->cost_since = now;
}

/* the largest frame the requests in flight can need, so the backend repacks
 * that much of its stream and no more; call with the lock held */
static void size_hint_locked(struct atl_camera_streams *s)
{
	uint32_t targets = s->repeating_id != REQUEST_NONE ? s->repeating_targets : 0;
	int width = 0, height = 0;

	if (!s->backend->set_frame_size_needed)
		return;
	for (GList *l = s->pending->head; l; l = l->next)
		targets |= ((struct pending_request *)l->data)->targets;
	for (int i = 0; i < s->n_streams; i++) {
		if (!(targets & (1u << i)))
			continue;
		if (s->streams[i].config.width > width)
			width = s->streams[i].config.width;
		if (s->streams[i].config.height > height)
			height = s->streams[i].config.height;
	}
	s->backend->set_frame_size_needed(s->camera, width & ~1, height & ~1);
}

static void pending_free(struct pending_request *pending)
{
	if (!pending)
		return;
	atl_camera_metadata_free(pending->settings);
	free(pending);
}

/*
 * One capture through the emulated streams: a started event, a buffer per
 * targeted stream, then the result. Call with the lock held; it is dropped
 * for the delivery and taken again before returning.
 */
static void emu_deliver_locked(struct atl_camera_streams *s, int request_id,
                               const struct atl_camera_metadata *settings, uint32_t targets,
                               const uint8_t *nv21, int width, int height, int stride,
                               int64_t timestamp)
{
	struct atl_camera_metadata *result = NULL;
	struct emu_buffer *buffers[ATL_CAMERA_MAX_STREAMS] = {0};
	int n_streams = s->n_streams;
	int64_t frame_number = s->frame_number++;

	settings = atl_camera_metadata_copy(settings);
	for (int i = 0; i < n_streams; i++)
		if (targets & (1u << i))
			buffers[i] = emu_buffer_take_locked(s, i);
	g_mutex_unlock(&s->lock);

	on_started(s, request_id, frame_number, timestamp);

	for (int i = 0; i < n_streams; i++) {
		struct emu_buffer *buffer = buffers[i];
		int64_t entered;

		if (!buffer)
			continue;
		entered = g_get_monotonic_time();
		if (!emu_fill(s, i, buffer, nv21, width, height, stride, settings)) {
			atl_camera_buffer_release(&buffer->pub);
			continue;
		}
		buffer->pub.timestamp = timestamp;
		g_mutex_lock(&s->lock);
		s->streams[i].micros += g_get_monotonic_time() - entered;
		s->streams[i].delivered++;
		g_mutex_unlock(&s->lock);
		on_buffer(s, &buffer->pub);
	}

	if (s->backend->get_result_metadata)
		result = s->backend->get_result_metadata(s->camera, settings);
	if (!result)
		result = atl_camera_metadata_new();
	/* the frame's own timestamp, whatever the backend claimed */
	atl_camera_metadata_add(result, ACAMERA_SENSOR_TIMESTAMP, ATL_CAMERA2_TYPE_INT64,
	                        &timestamp, 1);
	atl_camera_metadata_free((struct atl_camera_metadata *)settings);
	on_result(s, request_id, frame_number, result);
	g_mutex_lock(&s->lock);
}

/*
 * The backend's frame thread. The oldest one-shot capture owns this frame;
 * otherwise the repeating request does, and with neither there is nothing to
 * report. A frame with no pixels (a backend whose texture path took it) is no
 * use to a one-shot, so that one waits for the next.
 */
static void on_frame(const uint8_t *nv21, int width, int height, int stride, void *user)
{
	struct atl_camera_streams *s = user;
	struct pending_request *pending;
	struct atl_camera_metadata *settings;
	uint32_t targets;
	int request_id;
	int64_t timestamp = (int64_t)g_get_monotonic_time() * 1000;

	g_mutex_lock(&s->lock);
	if (!s->configured || !nv21) {
		g_mutex_unlock(&s->lock);
		return;
	}
	pending = g_queue_pop_head(s->pending);
	settings = pending ? pending->settings : s->repeating;
	request_id = pending ? pending->id : s->repeating_id;
	targets = pending ? pending->targets : s->repeating_targets;
	if (request_id == REQUEST_NONE) {
		g_mutex_unlock(&s->lock);
		return;
	}
	if (pending)
		size_hint_locked(s); /* the queue may have emptied */
	cost_report_locked(s);
	emu_deliver_locked(s, request_id, settings, targets, nv21, width, height, stride, timestamp);
	g_mutex_unlock(&s->lock);
	pending_free(pending);
}

bool atl_camera_streams_reprocess(struct atl_camera_streams *s, int request_id,
                                  const struct atl_camera_metadata *settings, uint32_t targets,
                                  const uint8_t *nv21, int width, int height, int64_t timestamp)
{
	if (!s || !nv21)
		return false;
	g_mutex_lock(&s->lock);
	if (!s->configured || s->real) {
		g_mutex_unlock(&s->lock);
		return false;
	}
	if (!timestamp)
		timestamp = (int64_t)g_get_monotonic_time() * 1000;
	atl_camera_record_request(s, request_id, settings, targets, false, true, timestamp);
	emu_deliver_locked(s, request_id, settings, targets, nv21, width, height, width, timestamp);
	g_mutex_unlock(&s->lock);
	return true;
}

bool atl_camera_streams_can_reprocess(struct atl_camera_streams *s)
{
	bool can;

	if (!s || !s->backend->queue_input || !s->backend->submit_reprocess)
		return false;
	g_mutex_lock(&s->lock);
	can = s->configured && s->real && s->has_input;
	g_mutex_unlock(&s->lock);
	return can;
}

bool atl_camera_streams_queue_input(struct atl_camera_streams *s, struct atl_camera_buffer *buffer,
                                    atl_camera_input_released_cb released, void *user)
{
	if (!buffer || !atl_camera_streams_can_reprocess(s))
		return false;
	return s->backend->queue_input(s->camera, buffer, released, user);
}

bool atl_camera_streams_submit_reprocess(struct atl_camera_streams *s, int request_id,
                                         const struct atl_camera_metadata *settings,
                                         uint32_t targets, int64_t input_timestamp)
{
	if (!atl_camera_streams_can_reprocess(s))
		return false;

	/* a reprocess is a one-shot like any other: a flush has to be able to
	 * name it */
	g_mutex_lock(&s->lock);
	oneshot_add_locked(s, request_id);
	g_mutex_unlock(&s->lock);
	atl_camera_record_request(s, request_id, settings, targets, false, true, input_timestamp);
	if (s->backend->submit_reprocess(s->camera, request_id, settings, targets, input_timestamp))
		return true;
	g_mutex_lock(&s->lock);
	oneshot_done_locked(s, request_id);
	g_mutex_unlock(&s->lock);
	return false;
}

/* the settings a one-stream backend can act on; the rest is metadata */
static void emu_apply(struct atl_camera_streams *s, const struct atl_camera_metadata *settings)
{
	const struct atl_camera_metadata_entry *fps;

	if (s->backend->set_request_metadata) {
		s->backend->set_request_metadata(s->camera, settings);
		return;
	}
	fps = atl_camera_metadata_find(settings, ACAMERA_CONTROL_AE_TARGET_FPS_RANGE);
	if (fps && fps->type == ATL_CAMERA2_TYPE_INT32 && fps->count >= 2) {
		const int32_t *range = fps->data;

		s->backend->set_fps_range(s->camera, range[0] * 1000, range[1] * 1000);
	}
}

/* call with the lock held */
static void emu_teardown_locked(struct atl_camera_streams *s)
{
	for (int i = 0; i < s->n_streams; i++) {
		struct emu_stream *stream = &s->streams[i];
		struct emu_buffer *buffer;

		while ((buffer = stream->free_list)) {
			stream->free_list = buffer->next;
			emu_buffer_free(buffer);
		}
		free(stream->cols);
		memset(stream, 0, sizeof(*stream));
	}
	s->n_streams = 0;
	atl_camera_metadata_free(s->repeating);
	s->repeating = NULL;
	s->repeating_id = REQUEST_NONE;
	s->repeating_targets = 0;
	while (!g_queue_is_empty(s->pending))
		pending_free(g_queue_pop_head(s->pending));
	s->oneshots->len = 0;
}

static bool emu_configure(struct atl_camera_streams *s, const struct atl_camera_stream *configs,
                          int n_streams)
{
	int width = 0, height = 0;

	for (int i = 0; i < n_streams; i++) {
		switch (configs[i].format) {
		case ATL_CAMERA_FORMAT_YUV_420_888:
		case ATL_CAMERA_FORMAT_PRIVATE:
		case ATL_CAMERA_FORMAT_JPEG:
		case ATL_CAMERA_FORMAT_RAW10:
		case ATL_CAMERA_FORMAT_RAW_SENSOR:
			break;
		default:
			fprintf(stderr, "camera2: no emulated stream of format 0x%x\n", configs[i].format);
			return false;
		}
		/* the one stream is sized for the largest output by area; a smaller
		 * one scales the frame down as it converts it */
		if (configs[i].width * configs[i].height > width * height) {
			width = configs[i].width;
			height = configs[i].height;
		}
	}
	if (!s->backend->set_preview_size(s->camera, width, height) ||
	    !s->backend->set_preview_format(s->camera, ATL_CAMERA_FORMAT_NV21)) {
		fprintf(stderr, "camera2: backend '%s' rejected a %dx%d stream\n", s->backend->name,
		        width, height);
		return false;
	}

	g_mutex_lock(&s->lock);
	for (int i = 0; i < n_streams; i++) {
		s->streams[i].config = configs[i];
		s->streams[i].config.physical_id = NULL;
		if (s->streams[i].config.max_buffers < 1)
			s->streams[i].config.max_buffers = 1;
	}
	s->n_streams = n_streams;
	g_mutex_unlock(&s->lock);

	s->backend->set_frame_callback(s->camera, on_frame, s);
	fprintf(stderr, "camera2: session configured (%d emulated stream(s) off a %dx%d stream)\n",
	        n_streams, width, height);
	return true;
}

/* --- the interface -------------------------------------------------------- */

struct atl_camera_streams *atl_camera_streams_new(const struct atl_camera_backend *backend,
                                                  struct atl_camera *camera,
                                                  const struct atl_camera_stream_callbacks *callbacks,
                                                  void *user)
{
	struct atl_camera_streams *s = calloc(1, sizeof(*s));

	s->refcount = 1;
	s->backend = backend;
	s->camera = camera;
	s->callbacks = *callbacks;
	s->user = user;
	s->repeating_id = REQUEST_NONE;
	s->pending = g_queue_new();
	s->oneshots = g_array_new(FALSE, FALSE, sizeof(int));
	g_mutex_init(&s->lock);
	g_mutex_init(&s->fill_lock);
	return s;
}

static void streams_unref(struct atl_camera_streams *s)
{
	if (!g_atomic_int_dec_and_test(&s->refcount))
		return;
	atl_camera_record_end(s);
	g_queue_free_full(s->pending, (GDestroyNotify)pending_free);
	g_array_free(s->oneshots, TRUE);
	free(s->scratch_nv21);
	free(s->scratch_rgba);
	free(s->scratch_rotated);
	g_mutex_clear(&s->lock);
	g_mutex_clear(&s->fill_lock);
	free(s);
}

static bool real_streams_enabled(void)
{
	static int on = -1;

	if (on < 0) {
		const char *set = getenv("ATL_CAMERA2_STREAMS");

		/* ATL_CAMERA2_STREAMS=0: the emulation even on a backend with streams
		 * of its own, which is what every camera2 session was before there
		 * were any - a bisect lever */
		on = !set || atoi(set);
		if (!on)
			fprintf(stderr, "camera2: real streams off, every session is emulated\n");
	}
	return on;
}

bool atl_camera_streams_configure(struct atl_camera_streams *s,
                                  const struct atl_camera_stream *configs, int n_streams,
                                  const struct atl_camera_stream_input *input)
{
	bool was_real;

	if (!s || n_streams < 0 || n_streams > ATL_CAMERA_MAX_STREAMS)
		return false;

	/* down with the old session first, whichever kind it was */
	g_mutex_lock(&s->lock);
	was_real = s->real;
	s->configured = false;
	s->generation++;
	g_mutex_unlock(&s->lock);
	if (was_real) {
		s->backend->configure_streams(s->camera, NULL, 0, NULL, NULL, NULL);
	} else {
		s->backend->stop_preview(s->camera);
		s->backend->set_frame_callback(s->camera, NULL, NULL);
	}
	g_mutex_lock(&s->lock);
	s->previewing = false;
	emu_teardown_locked(s);
	s->real = false;
	s->has_input = false;
	g_mutex_unlock(&s->lock);
	atl_camera_record_end(s);
	if (!n_streams)
		return true;

	if (s->backend->configure_streams && real_streams_enabled()) {
		/* an input the backend cannot serve is not a session it can serve:
		 * falling back to the emulation would drop the app's raw frames */
		if (input && !s->backend->queue_input) {
			fprintf(stderr, "camera2: backend '%s' has no reprocessing input\n", s->backend->name);
			return false;
		}
		if (s->backend->configure_streams(s->camera, configs, n_streams, input, &relay, s)) {
			g_mutex_lock(&s->lock);
			s->real = true;
			s->has_input = input != NULL;
			s->configured = true;
			g_mutex_unlock(&s->lock);
			atl_camera_record_session(s, s->backend, configs, n_streams, input);
			return true;
		}
		/* an input is the app's raw frames going back to the camera; the
		 * emulation has none of them, so falling back would leave a session
		 * whose every reprocess capture fails */
		if (input) {
			fprintf(stderr, "camera2: backend '%s' refused the %d stream(s) and the %dx%d "
			                "format 0x%x input\n", s->backend->name, n_streams, input->width,
			        input->height, input->format);
			return false;
		}
		fprintf(stderr, "camera2: backend '%s' refused the %d stream(s), emulating them "
		                "off one stream instead\n", s->backend->name, n_streams);
	}
	if (!emu_configure(s, configs, n_streams))
		return false;
	g_mutex_lock(&s->lock);
	s->configured = true;
	g_mutex_unlock(&s->lock);
	atl_camera_record_session(s, s->backend, configs, n_streams, input);
	return true;
}

bool atl_camera_streams_are_real(struct atl_camera_streams *s)
{
	bool real;

	g_mutex_lock(&s->lock);
	real = s->configured && s->real;
	g_mutex_unlock(&s->lock);
	return real;
}

bool atl_camera_streams_would_be_real(struct atl_camera_streams *s)
{
	return s && s->backend->configure_streams && real_streams_enabled();
}

bool atl_camera_streams_submit(struct atl_camera_streams *s, int request_id,
                               const struct atl_camera_metadata *settings, uint32_t targets,
                               bool repeating)
{
	bool real, start;

	g_mutex_lock(&s->lock);
	real = s->real;
	if (!s->configured) {
		g_mutex_unlock(&s->lock);
		return false;
	}
	if (!repeating)
		oneshot_add_locked(s, request_id);
	atl_camera_record_request(s, request_id, settings, targets, repeating, false, 0);
	if (real) {
		g_mutex_unlock(&s->lock);
		if (s->backend->submit_request(s->camera, request_id, settings, targets, repeating))
			return true;
		g_mutex_lock(&s->lock);
		oneshot_done_locked(s, request_id);
		g_mutex_unlock(&s->lock);
		return false;
	}

	if (repeating) {
		atl_camera_metadata_free(s->repeating);
		s->repeating = atl_camera_metadata_copy(settings);
		s->repeating_id = request_id;
		s->repeating_targets = targets;
	} else {
		struct pending_request *pending = calloc(1, sizeof(*pending));

		pending->settings = atl_camera_metadata_copy(settings);
		pending->id = request_id;
		pending->targets = targets;
		g_queue_push_tail(s->pending, pending);
	}
	size_hint_locked(s);
	start = !s->previewing;
	s->previewing = true;
	g_mutex_unlock(&s->lock);

	/* no lock: a backend that speaks camera2 turns this into a session call
	 * that waits for the thread its frames come from, and that thread is
	 * about to want the lock */
	emu_apply(s, settings);
	if (start && !s->backend->start_preview(s->camera)) {
		g_mutex_lock(&s->lock);
		s->previewing = false;
		oneshot_done_locked(s, request_id);
		g_mutex_unlock(&s->lock);
		return false;
	}
	return true;
}

void atl_camera_streams_cancel_repeating(struct atl_camera_streams *s)
{
	bool stop;

	g_mutex_lock(&s->lock);
	if (s->real) {
		g_mutex_unlock(&s->lock);
		s->backend->cancel_repeating(s->camera);
		return;
	}
	atl_camera_metadata_free(s->repeating);
	s->repeating = NULL;
	s->repeating_id = REQUEST_NONE;
	s->repeating_targets = 0;
	size_hint_locked(s);
	stop = s->previewing && g_queue_is_empty(s->pending);
	if (stop)
		s->previewing = false;
	g_mutex_unlock(&s->lock);
	if (stop)
		s->backend->stop_preview(s->camera);
}

int atl_camera_streams_flush(struct atl_camera_streams *s, int *ids, int max)
{
	int n = 0;
	bool real;

	g_mutex_lock(&s->lock);
	real = s->real;
	for (guint i = 0; i < s->oneshots->len && n < max; i++)
		ids[n++] = g_array_index(s->oneshots, int, i);
	s->oneshots->len = 0;
	if (!real)
		while (!g_queue_is_empty(s->pending))
			pending_free(g_queue_pop_head(s->pending));
	size_hint_locked(s);
	g_mutex_unlock(&s->lock);

	if (real)
		s->backend->flush_requests(s->camera);
	return n;
}

void atl_camera_streams_free(struct atl_camera_streams *s)
{
	if (!s)
		return;
	atl_camera_streams_configure(s, NULL, 0, NULL);
	/* the callbacks are gone with the session; a buffer still out there only
	 * needs the pool, which stays until it is back */
	g_mutex_lock(&s->lock);
	memset(&s->callbacks, 0, sizeof(s->callbacks));
	g_mutex_unlock(&s->lock);
	streams_unref(s);
}

void atl_camera_buffer_release(struct atl_camera_buffer *buffer)
{
	if (buffer && buffer->release)
		buffer->release(buffer);
}
