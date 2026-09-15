#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "camera_backend.h"
#include "camera_frame.h"
#include "camera_recording.h"

/*
 * ATL_CAMERA_REPLAY: a camera2 backend whose sensor is a file
 * (camera_record.c wrote it on the device, camera_recording.h is the format).
 *
 * It serves the recorded camera's own characteristics, and a session whose
 * streams match the recorded ones is fed the recorded buffers and results at
 * the intervals they arrived at: the pre-roll on a loop while the repeating
 * request runs, and one frame of the recorded burst for each still capture.
 * The same frames every run is the whole point - two merges of one scene can
 * then be compared without a phone in the loop.
 *
 * Every request is answered with one whole recorded frame, matched to it by
 * the recorded request's id. The events of a recorded request nothing replays
 * are dropped rather than handed to whatever request is in flight: the frames
 * a device abandons when its own shutter is pressed are recorded as failures
 * of the preview request, and reporting those as the still capture tells the
 * app the picture it just asked for has failed before it began.
 *
 * Replay is open-loop. The app's own AE/AF decisions are recorded and readable
 * (every request is in the file), but they cannot change the pixels that come
 * back, so 3A convergence is the one thing this cannot debug.
 */

#define REPLAY_MAX_CAMERAS 8

struct replay_camera_info {
	char *id;
	int facing;
	int orientation;
	struct atl_camera_metadata *characteristics;
	uint32_t *keys[3];
	int n_keys[3];
};

/* one recorded event, in the order it happened */
struct replay_event {
	uint32_t type;
	int request_id;
	int64_t frame_number;
	int64_t timestamp;
	int stream;
	struct atl_camera_metadata *bag; /* a result's */
	/* a buffer's */
	int width, height, format, n_planes;
	uint32_t flags;
	struct atl_camera_plane planes[ATL_CAMERA_MAX_PLANES];
	uint8_t *pixels; /* the planes point into this */
};

/*
 * One recorded frame: the sensor event, the buffers carrying its timestamp and
 * the result that closed it. This is the unit a request is answered with.
 */
struct replay_frame {
	int request_id;      /* the recorded request it answered */
	int64_t frame_number;
	int64_t timestamp;   /* the recorded sensor timestamp */
	int64_t gap;         /* nanoseconds since the frame before it */
	int result;          /* event index, -1 when the recording ended first */
	int buffers[ATL_CAMERA_MAX_STREAMS];
	int n_buffers;
};

/* a queued one-shot: which request, and whether it is taking a picture */
struct replay_oneshot {
	int id;
	bool still;
};

static struct {
	bool loaded;
	char backend[16];
	struct replay_camera_info cameras[REPLAY_MAX_CAMERAS];
	int n_cameras;
	const char *id_list[REPLAY_MAX_CAMERAS];
	struct atl_camera_stream streams[ATL_CAMERA_MAX_STREAMS];
	int n_buffers[ATL_CAMERA_MAX_STREAMS]; /* how many frames each one has */
	int n_streams;
	struct replay_event *events;
	int n_events;
	int burst_at; /* the first event of the burst, -1 when nothing triggered */
	int *still_ids; /* the one-shot requests the trigger opened */
	int n_still_ids;
	struct replay_frame *frames; /* the pre-roll's first, then the burst's */
	int n_frames;
	int n_pre;
	double speed;
} rep;

struct atl_camera {
	int index;
	/* the session */
	struct atl_camera_stream_callbacks cbs;
	void *user;
	int map[ATL_CAMERA_MAX_STREAMS];  /* app stream -> recorded stream */
	bool scaled[ATL_CAMERA_MAX_STREAMS]; /* and it wants a different size */
	struct atl_camera_stream config[ATL_CAMERA_MAX_STREAMS];
	int n_streams;
	bool configured;
	/* the playback thread */
	GThread *thread;
	GMutex lock;
	GCond cond;
	bool running;
	int repeating_id;
	GQueue *oneshots; /* pending struct replay_oneshot */
	int64_t frame_number;
	int64_t clock; /* the sensor timestamp the last frame went out at */
	int64_t due;   /* the wall clock the next frame is owed at */
	int next_pre;   /* where the pre-roll loop stands */
	int next_still; /* and which recorded still the next picture gets */
	/* Camera1 */
	atl_camera_frame_cb frame_cb;
	void *frame_user;
	atl_camera_error_cb error_cb;
	void *error_user;
	bool previewing;
	struct atl_camera_caps caps;
	struct atl_camera_size sizes[ATL_CAMERA_MAX_STREAMS];
	struct atl_camera_fps_range fps;
};

/* --- loading ------------------------------------------------------------- */

static void load_camera(struct atl_rec_cur *cur)
{
	struct replay_camera_info *info;

	if (rep.n_cameras == REPLAY_MAX_CAMERAS)
		return;
	info = &rep.cameras[rep.n_cameras];
	info->id = atl_rec_get_str(cur);
	info->facing = (int)atl_rec_get_u32(cur);
	info->orientation = (int)atl_rec_get_u32(cur);
	info->characteristics = atl_rec_get_bag(cur);
	for (int which = 0; which < 3; which++) {
		int n = (int)atl_rec_get_u32(cur);

		if (cur->bad || n < 0)
			break;
		info->keys[which] = n ? calloc((size_t)n, sizeof(uint32_t)) : NULL;
		for (int i = 0; i < n; i++)
			info->keys[which][i] = atl_rec_get_u32(cur);
		info->n_keys[which] = n;
	}
	if (cur->bad || !info->id || !info->characteristics) {
		free(info->id);
		atl_camera_metadata_free(info->characteristics);
		memset(info, 0, sizeof(*info));
		return;
	}
	/* the vendor tags are the recorded device's, and a Key resolves against
	 * the registry rather than the bag it came from */
	for (int i = 0; i < atl_camera_metadata_n_entries(info->characteristics); i++) {
		const struct atl_camera_metadata_entry *entry =
		    atl_camera_metadata_entry_at(info->characteristics, i);

		if (entry->name)
			atl_camera2_vendor_register(entry->tag, entry->name, entry->type);
	}
	rep.id_list[rep.n_cameras] = info->id;
	rep.n_cameras++;
}

static void load_session(struct atl_rec_cur *cur)
{
	int n = (int)atl_rec_get_u32(cur);

	rep.n_streams = 0;
	for (int i = 0; i < n && i < ATL_CAMERA_MAX_STREAMS; i++) {
		struct atl_camera_stream *stream = &rep.streams[i];

		stream->width = (int)atl_rec_get_u32(cur);
		stream->height = (int)atl_rec_get_u32(cur);
		stream->format = (int)atl_rec_get_u32(cur);
		stream->max_buffers = (int)atl_rec_get_u32(cur);
		stream->usage = (uint64_t)atl_rec_get_i64(cur);
		stream->physical_id = atl_rec_get_str(cur);
		if (cur->bad)
			return;
		rep.n_streams++;
	}
}

static void load_buffer(struct atl_rec_cur *cur, struct replay_event *event)
{
	size_t total = 0;
	const uint8_t *pixels;

	event->stream = (int)atl_rec_get_u32(cur);
	event->width = (int)atl_rec_get_u32(cur);
	event->height = (int)atl_rec_get_u32(cur);
	event->format = (int)atl_rec_get_u32(cur);
	event->timestamp = atl_rec_get_i64(cur);
	event->flags = atl_rec_get_u32(cur);
	atl_rec_get_u32(cur); /* the stream's own width */
	atl_rec_get_u32(cur); /* and height */
	event->n_planes = (int)atl_rec_get_u32(cur);
	if (cur->bad || event->n_planes < 0 || event->n_planes > ATL_CAMERA_MAX_PLANES) {
		cur->bad = true;
		return;
	}
	for (int i = 0; i < event->n_planes; i++) {
		event->planes[i].len = (int)atl_rec_get_u32(cur);
		event->planes[i].row_stride = (int)atl_rec_get_u32(cur);
		event->planes[i].pixel_stride = (int)atl_rec_get_u32(cur);
		total += (size_t)event->planes[i].len;
	}
	pixels = atl_rec_get_blob(cur, total);
	if (cur->bad)
		return;
	/* the planes stay ours for the life of the process: a consumer only ever
	 * reads them, so every replayed buffer can point at the same bytes */
	event->pixels = malloc(total ? total : 1);
	memcpy(event->pixels, pixels, total);
	total = 0;
	for (int i = 0; i < event->n_planes; i++) {
		event->planes[i].data = event->pixels + total;
		total += (size_t)event->planes[i].len;
	}
}

/* a recorded request the trigger opened, and so a member of the burst */
static void still_ids_push(int id)
{
	static int capacity;

	for (int i = 0; i < rep.n_still_ids; i++)
		if (rep.still_ids[i] == id)
			return;
	if (rep.n_still_ids == capacity) {
		capacity = capacity ? capacity * 2 : 16;
		rep.still_ids = realloc(rep.still_ids, sizeof(*rep.still_ids) * (size_t)capacity);
	}
	rep.still_ids[rep.n_still_ids++] = id;
}

static bool still_request(int id)
{
	for (int i = 0; i < rep.n_still_ids; i++)
		if (rep.still_ids[i] == id)
			return true;
	return false;
}

static void frames_push(const struct replay_frame *frame)
{
	static int capacity;

	if (rep.n_frames == capacity) {
		capacity = capacity ? capacity * 2 : 64;
		rep.frames = realloc(rep.frames, sizeof(*rep.frames) * (size_t)capacity);
	}
	rep.frames[rep.n_frames++] = *frame;
}

/*
 * The recorded events, gathered into the frames they belong to. A result is
 * the one carrying this frame's number - which is also where its sensor
 * timestamp comes from, since a result chunk holds no timestamp of its own -
 * and a buffer is one stamped with the same time.
 *
 * A frame before the trigger belongs to the preview, one after it to the
 * burst, and anything else (the preview frames still in flight when the
 * device's shutter was pressed) belongs to neither. That, and the frame the
 * recording stopped in the middle of, is dropped: a request answered with a
 * started and no result is one the app waits on for ever. Both counts are
 * returned so the banner can say so.
 */
static int index_frames(int *partial)
{
	int64_t previous = 0;
	int dropped = 0;

	*partial = 0;
	for (int i = 0; i < rep.n_events; i++) {
		const struct replay_event *started = &rep.events[i];
		struct replay_frame frame = {.result = -1};
		bool burst = rep.burst_at >= 0 && i >= rep.burst_at;

		if (started->type != ATL_REC_STARTED)
			continue;
		if (burst != still_request(started->request_id)) {
			dropped++;
			continue;
		}
		frame.request_id = started->request_id;
		frame.frame_number = started->frame_number;
		frame.timestamp = started->timestamp;
		for (int j = 0; j < rep.n_events; j++) {
			const struct replay_event *event = &rep.events[j];

			if (event->type == ATL_REC_RESULT && frame.result < 0 &&
			    event->request_id == frame.request_id &&
			    event->frame_number == frame.frame_number)
				frame.result = j;
			else if (event->type == ATL_REC_BUFFER && event->timestamp == frame.timestamp &&
			         frame.n_buffers < ATL_CAMERA_MAX_STREAMS)
				frame.buffers[frame.n_buffers++] = j;
		}
		if (frame.result < 0) {
			(*partial)++;
			continue;
		}
		/* the recorded interval, so a burst replays at the rate it was taken */
		frame.gap = previous && started->timestamp > previous ? started->timestamp - previous : 0;
		previous = started->timestamp;
		if (!burst)
			rep.n_pre++;
		frames_push(&frame);
	}
	/* the first frame of the loop follows the last one, at the same rate */
	if (rep.n_frames > 1 && !rep.frames[0].gap)
		rep.frames[0].gap = rep.frames[1].gap;
	return dropped;
}

static void events_push(const struct replay_event *event)
{
	static int capacity;

	if (rep.n_events == capacity) {
		capacity = capacity ? capacity * 2 : 256;
		rep.events = realloc(rep.events, sizeof(*rep.events) * (size_t)capacity);
	}
	rep.events[rep.n_events++] = *event;
}

static bool load_file(const char *path)
{
	struct atl_rec_header header;
	FILE *file = fopen(path, "rb");
	uint32_t type;
	uint8_t *data;
	size_t len;
	int frames = 0, dropped, partial;

	if (!file) {
		fprintf(stderr, "Camera replay: cannot read %s (%s)\n", path, strerror(errno));
		return false;
	}
	if (fread(&header, sizeof(header), 1, file) != 1 ||
	    memcmp(header.magic, ATL_REC_MAGIC, sizeof(header.magic))) {
		fprintf(stderr, "Camera replay: %s is not a camera recording\n", path);
		fclose(file);
		return false;
	}
	if (header.version != ATL_REC_VERSION) {
		fprintf(stderr, "Camera replay: %s is version %u, this build reads %d\n", path,
		        header.version, ATL_REC_VERSION);
		fclose(file);
		return false;
	}
	memcpy(rep.backend, header.backend, sizeof(rep.backend));
	rep.backend[sizeof(rep.backend) - 1] = '\0';
	rep.burst_at = -1;

	while (atl_rec_read_chunk(file, &type, &data, &len)) {
		struct atl_rec_cur cur = {.data = data, .len = len};
		struct replay_event event = {.type = type};

		switch (type) {
		case ATL_REC_CAMERA:
			load_camera(&cur);
			break;
		case ATL_REC_SESSION:
			load_session(&cur);
			break;
		case ATL_REC_TRIGGER:
			if (rep.burst_at < 0)
				rep.burst_at = rep.n_events;
			break;
		case ATL_REC_REQUEST: {
			/* the settings are kept in the file for the eye, but which ids the
			 * trigger opened is how a replayed frame finds its request */
			int id = (int)atl_rec_get_u32(&cur);
			bool repeating = atl_rec_get_u32(&cur) != 0;

			if (!cur.bad && !repeating && rep.burst_at >= 0)
				still_ids_push(id);
			break;
		}
		case ATL_REC_STARTED:
			event.request_id = (int)atl_rec_get_u32(&cur);
			event.frame_number = atl_rec_get_i64(&cur);
			event.timestamp = atl_rec_get_i64(&cur);
			if (!cur.bad) {
				events_push(&event);
				frames++;
			}
			break;
		case ATL_REC_RESULT:
			event.request_id = (int)atl_rec_get_u32(&cur);
			event.frame_number = atl_rec_get_i64(&cur);
			event.bag = atl_rec_get_bag(&cur);
			if (!cur.bad)
				events_push(&event);
			break;
		case ATL_REC_FAILED:
			event.request_id = (int)atl_rec_get_u32(&cur);
			event.frame_number = atl_rec_get_i64(&cur);
			if (!cur.bad)
				events_push(&event);
			break;
		case ATL_REC_LOST:
			event.request_id = (int)atl_rec_get_u32(&cur);
			event.frame_number = atl_rec_get_i64(&cur);
			event.stream = (int)atl_rec_get_u32(&cur);
			if (!cur.bad)
				events_push(&event);
			break;
		case ATL_REC_BUFFER:
			load_buffer(&cur, &event);
			if (!cur.bad) {
				if (event.stream >= 0 && event.stream < ATL_CAMERA_MAX_STREAMS)
					rep.n_buffers[event.stream]++;
				events_push(&event);
			}
			break;
		}
		free(data);
	}
	fclose(file);

	dropped = index_frames(&partial);
	if (!rep.n_cameras || !rep.n_streams || !rep.n_frames) {
		fprintf(stderr, "Camera replay: %s holds %d camera(s), %d stream(s) and %d frame(s) - "
		                "not a session\n", path, rep.n_cameras, rep.n_streams, frames);
		return false;
	}
	fprintf(stderr, "Camera replay: %s - %d camera(s) off '%s', %d stream(s), %d frame(s), "
	                "%d event(s)%s\n", path, rep.n_cameras, rep.backend, rep.n_streams, frames,
	        rep.n_events, rep.burst_at >= 0 ? ", a burst at the trigger" : ", no trigger");
	fprintf(stderr, "Camera replay:   %d pre-roll frame(s), %d still frame(s) off %d request(s)",
	        rep.n_pre, rep.n_frames - rep.n_pre, rep.n_still_ids);
	if (dropped)
		fprintf(stderr, ", %d frame(s) of the interrupted preview dropped", dropped);
	if (partial)
		fprintf(stderr, ", %d the recording stopped inside", partial);
	fprintf(stderr, "\n");
	for (int i = 0; i < rep.n_streams; i++)
		fprintf(stderr, "Camera replay:   stream %d: %dx%d format 0x%x%s%s, %d frame(s)\n", i,
		        rep.streams[i].width, rep.streams[i].height, rep.streams[i].format,
		        rep.streams[i].physical_id ? ", physical camera " : "",
		        rep.streams[i].physical_id ? rep.streams[i].physical_id : "",
		        rep.n_buffers[i]);
	return true;
}

/* --- serving the recorded buffers ---------------------------------------- */

/* a buffer of recorded pixels: the planes point into the event, which lives
 * as long as the process, so there is nothing to free but the wrapper */
static void replay_buffer_release(struct atl_camera_buffer *pub)
{
	free(pub);
}

/* one whose pixels were scaled for this consumer, and are its own */
static void replay_scaled_release(struct atl_camera_buffer *pub)
{
	free(pub->planes[0].data);
	free(pub);
}

/* the recorded frame as NV21 at its own size; NULL when it has no pixels */
static uint8_t *frame_nv21(const struct replay_event *event, int *width, int *height)
{
	uint8_t *nv21;

	*width = event->width & ~1;
	*height = event->height & ~1;
	if (*width <= 0 || *height <= 0 || event->n_planes < 1)
		return NULL;
	nv21 = malloc((size_t)*width * *height * 3 / 2);
	if (event->n_planes >= 3) {
		struct atl_camera_yuv420 yuv = {
		    .y = event->planes[0].data,
		    .u = event->planes[1].data,
		    .v = event->planes[2].data,
		    .y_stride = event->planes[0].row_stride,
		    .u_stride = event->planes[1].row_stride,
		    .v_stride = event->planes[2].row_stride,
		    .u_pixel = event->planes[1].pixel_stride,
		    .v_pixel = event->planes[2].pixel_stride,
		    .y_len = event->planes[0].len,
		    .u_len = event->planes[1].len,
		    .v_len = event->planes[2].len,
		};

		atl_camera_yuv420_to_nv21(nv21, *width, *height, &yuv, 1);
	} else {
		/* the recorded PRIVATE reference is NV21 already */
		atl_camera_nv21_pack(nv21, event->planes[0].data, *width, *height,
		                     event->planes[0].row_stride);
	}
	return nv21;
}

/*
 * The recorded frame at the size this consumer configured. A viewfinder on a
 * desktop is whatever size the app's window made it, never the phone's, and a
 * session refused over that would fall back to the emulation's fabricated
 * frames - so a YUV or PRIVATE stream is scaled, and only those two are.
 */
static void deliver_scaled(struct atl_camera *camera, const struct replay_event *event,
                           int app_stream, int64_t timestamp)
{
	const struct atl_camera_stream *config = &camera->config[app_stream];
	struct atl_camera_buffer *buffer;
	int out_width = config->width, out_height = config->height;
	size_t luma = (size_t)out_width * out_height;
	int src_width, src_height;
	uint8_t *src, *dst;

	src = frame_nv21(event, &src_width, &src_height);
	if (!src)
		return;
	dst = malloc(luma * 3 / 2);
	atl_camera_nv21_scale(dst, out_width, out_height, src, src_width, src_height, src_width);
	free(src);

	buffer = calloc(1, sizeof(*buffer));
	buffer->stream = app_stream;
	buffer->width = out_width;
	buffer->height = out_height;
	buffer->format = config->format;
	buffer->timestamp = timestamp;
	buffer->size = luma * 3 / 2;
	buffer->planes[0] = (struct atl_camera_plane){dst, (int)luma, out_width, 1};
	buffer->n_planes = 1;
	if (config->format == ATL_CAMERA_FORMAT_YUV_420_888) {
		buffer->n_planes = 3;
		/* NV21 chroma is V,U: U is the odd byte, V the even one */
		buffer->planes[1] = (struct atl_camera_plane){dst + luma + 1, (int)(luma / 2 - 1),
		                                              out_width, 2};
		buffer->planes[2] = (struct atl_camera_plane){dst + luma, (int)(luma / 2), out_width, 2};
	}
	buffer->release = replay_scaled_release;
	buffer->owner = camera;
	camera->cbs.buffer(camera->user, buffer);
}

/* the recorded event's pixels, as a buffer of the app's stream */
static void deliver_buffer(struct atl_camera *camera, const struct replay_event *event,
                           int app_stream, int64_t timestamp)
{
	struct atl_camera_buffer *buffer;

	if (camera->scaled[app_stream]) {
		deliver_scaled(camera, event, app_stream, timestamp);
		return;
	}
	buffer = calloc(1, sizeof(*buffer));

	buffer->stream = app_stream;
	buffer->width = event->width;
	buffer->height = event->height;
	buffer->format = event->format;
	buffer->timestamp = timestamp;
	buffer->n_planes = event->n_planes;
	for (int i = 0; i < event->n_planes; i++)
		buffer->planes[i] = event->planes[i];
	buffer->release = replay_buffer_release;
	buffer->owner = camera;
	camera->cbs.buffer(camera->user, buffer);
}

/*
 * A Camera1 preview frame, for the app that drives the older API and for the
 * emulated session camera_streams.c stands up when a stream cannot be matched.
 * The callback is promised NV21, so a recorded YUV frame is converted.
 */
static void deliver_preview(struct atl_camera *camera, const struct replay_event *event)
{
	uint8_t *nv21;
	int width, height;

	if (!camera->frame_cb || event->n_planes < 1)
		return;
	if (event->format != ATL_CAMERA_FORMAT_PRIVATE && event->format != ATL_CAMERA_FORMAT_NV21 &&
	    event->format != ATL_CAMERA_FORMAT_YUV_420_888)
		return;
	nv21 = frame_nv21(event, &width, &height);
	if (!nv21)
		return;
	camera->frame_cb(nv21, width, height, width, camera->frame_user);
	free(nv21);
}

static int app_stream_of(struct atl_camera *camera, int recorded)
{
	for (int i = 0; i < camera->n_streams; i++)
		if (camera->map[i] == recorded)
			return i;
	return -1;
}

/*
 * A result's SENSOR_TIMESTAMP is what the app pairs its frames by, so it has
 * to be the timestamp this pass hands out and not the recorded one - the same
 * frame comes round again on the next loop. A result chunk carries no
 * timestamp of its own, so the frame it belongs to supplies it.
 */
static struct atl_camera_metadata *result_at(const struct replay_event *event, int64_t timestamp)
{
	struct atl_camera_metadata *bag = atl_camera_metadata_copy(event->bag);
	uint32_t tag = atl_camera2_tag_from_name("android.sensor.timestamp");

	if (bag && tag != ATL_CAMERA2_TAG_INVALID)
		atl_camera_metadata_add(bag, tag, ATL_CAMERA2_TYPE_INT64, &timestamp, 1);
	return bag;
}

static bool still_running(struct atl_camera *camera)
{
	bool running;

	g_mutex_lock(&camera->lock);
	running = camera->running;
	g_mutex_unlock(&camera->lock);
	return running;
}

/* sleeps until this frame's recorded interval has passed; false if it stopped */
static bool frame_due(struct atl_camera *camera, const struct replay_frame *frame)
{
	int64_t now = g_get_monotonic_time();

	camera->due += (int64_t)((double)frame->gap / 1000 / rep.speed);
	/* behind, because the consumer was: play on rather than sprint to catch up */
	if (camera->due < now)
		camera->due = now;
	while (g_get_monotonic_time() < camera->due) {
		g_usleep(1000);
		if (!still_running(camera))
			return false;
	}
	return still_running(camera);
}

/* one recorded frame, as this request's answer: a started, its buffers, its result */
static void play_frame(struct atl_camera *camera, const struct replay_frame *frame, int request_id)
{
	/* no session: the Camera1 preview, or the emulation driving one of its
	 * own off these frames. Only the frame callback may be called then. */
	bool session = camera->configured && camera->cbs.started;
	int64_t timestamp;

	camera->clock += frame->gap ? frame->gap : 1;
	timestamp = camera->clock;
	camera->frame_number++;
	if (session)
		camera->cbs.started(camera->user, request_id, camera->frame_number, timestamp);
	for (int i = 0; i < frame->n_buffers; i++) {
		const struct replay_event *event = &rep.events[frame->buffers[i]];
		int app_stream = app_stream_of(camera, event->stream);

		if (session && app_stream >= 0)
			deliver_buffer(camera, event, app_stream, timestamp);
		deliver_preview(camera, event);
	}
	if (session && frame->result >= 0)
		camera->cbs.result(camera->user, request_id, camera->frame_number,
		                   result_at(&rep.events[frame->result], timestamp));
}

/*
 * The frame a request is answered with: a picture takes the next of the
 * recorded burst, anything else the next of the pre-roll. Both wrap, so an app
 * asking for more frames than the recording holds gets the scene again rather
 * than nothing.
 */
static const struct replay_frame *next_frame(struct atl_camera *camera, bool still)
{
	int n_still = rep.n_frames - rep.n_pre;
	const struct replay_frame *frame;

	if (still && n_still) {
		frame = &rep.frames[rep.n_pre + camera->next_still];
		camera->next_still = (camera->next_still + 1) % n_still;
		return frame;
	}
	if (!rep.n_pre)
		return NULL;
	frame = &rep.frames[camera->next_pre];
	camera->next_pre = (camera->next_pre + 1) % rep.n_pre;
	return frame;
}

/*
 * One frame per turn, a queued request first: a shutter is then answered at
 * the next frame boundary rather than after a whole loop of the pre-roll.
 */
static gpointer play_thread(gpointer data)
{
	struct atl_camera *camera = data;

	for (;;) {
		struct replay_oneshot *job;
		const struct replay_frame *frame;
		int repeating;

		g_mutex_lock(&camera->lock);
		while (camera->running && camera->repeating_id < 0 && g_queue_is_empty(camera->oneshots))
			g_cond_wait(&camera->cond, &camera->lock);
		if (!camera->running) {
			g_mutex_unlock(&camera->lock);
			return NULL;
		}
		job = g_queue_pop_head(camera->oneshots);
		repeating = camera->repeating_id;
		g_mutex_unlock(&camera->lock);

		if (!job && !rep.n_pre) {
			/* a recording of nothing but a burst: no pre-roll to keep running */
			g_usleep(10000);
			continue;
		}
		frame = next_frame(camera, job && job->still);
		if (frame && frame_due(camera, frame))
			play_frame(camera, frame, job ? job->id : repeating);
		g_free(job);
	}
}

/* --- the vtable ---------------------------------------------------------- */

static bool replay_load(void);

static int replay_get_count(void)
{
	return replay_load() ? rep.n_cameras : 0;
}

static bool replay_get_info(int index, int *facing, int *orientation)
{
	if (!replay_load())
		return false;
	for (int i = 0; i < rep.n_cameras; i++) {
		if (atoi(rep.cameras[i].id) == index) {
			*facing = rep.cameras[i].facing;
			*orientation = rep.cameras[i].orientation;
			return true;
		}
	}
	return false;
}

static struct atl_camera *replay_open(int index)
{
	struct atl_camera *camera;

	if (!replay_load())
		return NULL;
	camera = calloc(1, sizeof(*camera));
	camera->index = index;
	camera->repeating_id = -1;
	camera->frame_number = 0;
	camera->clock = g_get_monotonic_time() * 1000;
	camera->due = g_get_monotonic_time();
	camera->oneshots = g_queue_new();
	g_mutex_init(&camera->lock);
	g_cond_init(&camera->cond);
	return camera;
}

static void play_stop(struct atl_camera *camera)
{
	GThread *thread;

	g_mutex_lock(&camera->lock);
	thread = camera->thread;
	camera->thread = NULL;
	camera->running = false;
	g_cond_broadcast(&camera->cond);
	g_mutex_unlock(&camera->lock);
	if (thread)
		g_thread_join(thread);
}

/* the lock is held */
static void oneshots_clear(struct atl_camera *camera)
{
	while (!g_queue_is_empty(camera->oneshots))
		g_free(g_queue_pop_head(camera->oneshots));
}

static void play_start(struct atl_camera *camera)
{
	g_mutex_lock(&camera->lock);
	if (!camera->thread) {
		camera->running = true;
		camera->due = g_get_monotonic_time();
		camera->thread = g_thread_new("atl-camera-replay", play_thread, camera);
	}
	g_cond_broadcast(&camera->cond);
	g_mutex_unlock(&camera->lock);
}

static void replay_close(struct atl_camera *camera)
{
	play_stop(camera);
	g_queue_free_full(camera->oneshots, g_free);
	g_mutex_clear(&camera->lock);
	g_cond_clear(&camera->cond);
	free(camera);
}

static const struct atl_camera_caps *replay_get_caps(struct atl_camera *camera)
{
	int n = 0;

	for (int i = 0; i < rep.n_streams && n < ATL_CAMERA_MAX_STREAMS; i++) {
		bool seen = false;

		for (int j = 0; j < n; j++)
			seen |= camera->sizes[j].width == rep.streams[i].width &&
			        camera->sizes[j].height == rep.streams[i].height;
		if (!seen)
			camera->sizes[n++] = (struct atl_camera_size){rep.streams[i].width,
			                                              rep.streams[i].height};
	}
	camera->fps = (struct atl_camera_fps_range){30000, 30000};
	camera->caps = (struct atl_camera_caps){
	    .preview_sizes = camera->sizes,
	    .n_preview_sizes = n,
	    .picture_sizes = camera->sizes,
	    .n_picture_sizes = n,
	    .fps_ranges = &camera->fps,
	    .n_fps_ranges = 1,
	    .focus_modes = "fixed",
	    .flash_modes = "off",
	};
	return &camera->caps;
}

static bool replay_set_preview_size(struct atl_camera *camera, int width, int height)
{
	(void)camera;
	(void)width;
	(void)height;
	return true;
}

static bool replay_set_preview_format(struct atl_camera *camera, int format)
{
	(void)camera;
	return format == ATL_CAMERA_FORMAT_NV21;
}

static bool replay_set_fps_range(struct atl_camera *camera, int min, int max)
{
	(void)camera;
	(void)min;
	(void)max;
	return true;
}

static void replay_set_frame_callback(struct atl_camera *camera, atl_camera_frame_cb cb, void *user)
{
	camera->frame_cb = cb;
	camera->frame_user = user;
}

static void replay_set_error_callback(struct atl_camera *camera, atl_camera_error_cb cb, void *user)
{
	camera->error_cb = cb;
	camera->error_user = user;
}

/* Camera1 and the emulated session: the pre-roll on a loop, no capture events */
static bool replay_start_preview(struct atl_camera *camera)
{
	g_mutex_lock(&camera->lock);
	camera->previewing = true;
	if (camera->repeating_id < 0)
		camera->repeating_id = 0;
	g_mutex_unlock(&camera->lock);
	play_start(camera);
	return true;
}

static void replay_stop_preview(struct atl_camera *camera)
{
	g_mutex_lock(&camera->lock);
	camera->previewing = false;
	g_mutex_unlock(&camera->lock);
	if (!camera->configured)
		play_stop(camera);
}

static bool replay_take_picture(struct atl_camera *camera, int width, int height, int quality,
                                atl_camera_jpeg_cb cb, void *user)
{
	(void)camera;
	(void)width;
	(void)height;
	(void)quality;
	(void)cb;
	(void)user;
	/* a Camera1 still off a recording would be the preview reference blown
	 * back up, which is not a picture anybody should compare */
	return false;
}

static void replay_autofocus(struct atl_camera *camera, atl_camera_autofocus_cb cb, void *user)
{
	(void)camera;
	if (cb)
		cb(true, user);
}

static void replay_cancel_autofocus(struct atl_camera *camera)
{
	(void)camera;
}

static void replay_set_display_orientation(struct atl_camera *camera, int degrees)
{
	(void)camera;
	(void)degrees;
}

static const char *const *replay_get_id_list(int *count)
{
	if (!replay_load()) {
		*count = 0;
		return NULL;
	}
	*count = rep.n_cameras;
	return rep.id_list;
}

static struct replay_camera_info *info_of(const char *id)
{
	for (int i = 0; i < rep.n_cameras; i++)
		if (!strcmp(rep.cameras[i].id, id))
			return &rep.cameras[i];
	return NULL;
}

static struct atl_camera_metadata *replay_get_static_metadata(const char *id)
{
	struct replay_camera_info *info;

	if (!replay_load())
		return NULL;
	info = info_of(id);
	return info ? atl_camera_metadata_copy(info->characteristics) : NULL;
}

static const uint32_t *replay_get_available_keys(const char *id, int which, int *count)
{
	struct replay_camera_info *info;

	*count = 0;
	if (!replay_load() || which < 0 || which > 2)
		return NULL;
	info = info_of(id);
	if (!info || !info->n_keys[which])
		return NULL;
	*count = info->n_keys[which];
	return info->keys[which];
}

/*
 * The recorded stream an app stream is served from: the same format and size,
 * because a raw frame is not something to scale. A session can hold several
 * that match - a Pixel's four RAW10 streams, one per physical camera - and
 * only the active lens' stream has frames in it, so an empty one is the last
 * resort rather than the first hit.
 */
static bool scalable(int format)
{
	return format == ATL_CAMERA_FORMAT_YUV_420_888 || format == ATL_CAMERA_FORMAT_PRIVATE ||
	       format == ATL_CAMERA_FORMAT_NV21;
}

static int match_stream(const struct atl_camera_stream *want, bool *scaled)
{
	int empty = -1, nearest = -1;
	long nearest_area = 0, want_area = (long)want->width * want->height;

	*scaled = false;
	for (int i = 0; i < rep.n_streams; i++) {
		if (rep.streams[i].format != want->format || rep.streams[i].width != want->width ||
		    rep.streams[i].height != want->height)
			continue;
		if (rep.n_buffers[i])
			return i;
		if (empty < 0)
			empty = i;
	}
	if (empty >= 0)
		return empty;

	/*
	 * A viewfinder is whatever size the app's window made it, and on a desktop
	 * that is never the size the phone recorded. Scaling one is honest - it is
	 * the recorded scene, coarser - while scaling a raw frame is not, so only
	 * the 4:2:0 formats get this. The smallest recorded frame that is still
	 * big enough wins, and a smaller one only if nothing else has pixels.
	 */
	if (!scalable(want->format))
		return -1;
	for (int i = 0; i < rep.n_streams; i++) {
		long area = (long)rep.streams[i].width * rep.streams[i].height;

		if (!rep.n_buffers[i] || !scalable(rep.streams[i].format))
			continue;
		if (nearest < 0 || (area >= want_area && (nearest_area < want_area || area < nearest_area)) ||
		    (area < want_area && nearest_area < area && nearest_area < want_area)) {
			nearest = i;
			nearest_area = area;
		}
	}
	*scaled = nearest >= 0;
	return nearest;
}

static bool replay_configure_streams(struct atl_camera *camera,
                                     const struct atl_camera_stream *streams, int n_streams,
                                     const struct atl_camera_stream_input *input,
                                     const struct atl_camera_stream_callbacks *callbacks,
                                     void *user)
{
	if (!n_streams) {
		play_stop(camera);
		g_mutex_lock(&camera->lock);
		camera->configured = false;
		camera->repeating_id = -1;
		oneshots_clear(camera);
		g_mutex_unlock(&camera->lock);
		return true;
	}
	if (input) {
		fprintf(stderr, "Camera replay: no reprocessing input off a recording\n");
		return false;
	}
	for (int i = 0; i < n_streams; i++) {
		camera->map[i] = match_stream(&streams[i], &camera->scaled[i]);
		camera->config[i] = streams[i];
		if (camera->map[i] < 0) {
			/* name what the recording does hold in that format: the app asked
			 * for a size the phone never recorded, and the size it did record
			 * is the one to point the app at */
			fprintf(stderr, "Camera replay: nothing recorded for a %dx%d format 0x%x stream",
			        streams[i].width, streams[i].height, streams[i].format);
			for (int j = 0; j < rep.n_streams; j++)
				if (rep.streams[j].format == streams[i].format)
					fprintf(stderr, "; the recording has %dx%d", rep.streams[j].width,
					        rep.streams[j].height);
			fprintf(stderr, "\n");
			return false;
		}
	}
	g_mutex_lock(&camera->lock);
	camera->cbs = *callbacks;
	camera->user = user;
	camera->n_streams = n_streams;
	camera->configured = true;
	camera->repeating_id = -1;
	oneshots_clear(camera);
	g_mutex_unlock(&camera->lock);
	for (int i = 0; i < n_streams; i++) {
		const struct atl_camera_stream *from = &rep.streams[camera->map[i]];

		fprintf(stderr, "Camera replay: stream %d (%dx%d format 0x%x) served from recorded "
		                "stream %d (%dx%d)%s%s, %d frame(s)%s\n", i, streams[i].width,
		        streams[i].height, streams[i].format, camera->map[i], from->width, from->height,
		        from->physical_id ? ", physical camera " : "", from->physical_id ?: "",
		        rep.n_buffers[camera->map[i]], camera->scaled[i] ? ", scaled" : "");
	}
	return true;
}

/* the app's own still capture, the test the recorder centred the burst on */
static bool wants_still(const struct atl_camera_metadata *settings)
{
	uint32_t tag = atl_camera2_tag_from_name("android.control.captureIntent");
	const struct atl_camera_metadata_entry *intent =
	    settings && tag != ATL_CAMERA2_TAG_INVALID ? atl_camera_metadata_find(settings, tag) : NULL;

	/* ACAMERA_CONTROL_CAPTURE_INTENT_STILL_CAPTURE */
	return intent && intent->count > 0 && ((const uint8_t *)intent->data)[0] == 2;
}

static bool replay_submit_request(struct atl_camera *camera, int request_id,
                                  const struct atl_camera_metadata *settings, uint32_t streams,
                                  bool repeating)
{
	/* open-loop: the recorded frames answer every request, whatever it asks
	 * for. Only the capture intent is read, to tell a picture from a preview
	 * frame the app asked for one at a time (a pre-capture sequence does) */
	(void)streams;
	g_mutex_lock(&camera->lock);
	if (repeating) {
		camera->repeating_id = request_id;
	} else {
		struct replay_oneshot *job = g_new0(struct replay_oneshot, 1);

		job->id = request_id;
		job->still = wants_still(settings);
		g_queue_push_tail(camera->oneshots, job);
	}
	g_mutex_unlock(&camera->lock);
	play_start(camera);
	return true;
}

static void replay_cancel_repeating(struct atl_camera *camera)
{
	g_mutex_lock(&camera->lock);
	camera->repeating_id = -1;
	g_mutex_unlock(&camera->lock);
}

static void replay_flush_requests(struct atl_camera *camera)
{
	g_mutex_lock(&camera->lock);
	oneshots_clear(camera);
	g_mutex_unlock(&camera->lock);
}

static const struct atl_camera_backend replay_backend = {
    .name = "replay",
    .get_camera_count = replay_get_count,
    .get_camera_info = replay_get_info,
    .open = replay_open,
    .close = replay_close,
    .get_caps = replay_get_caps,
    .set_preview_size = replay_set_preview_size,
    .set_preview_format = replay_set_preview_format,
    .set_fps_range = replay_set_fps_range,
    .set_frame_callback = replay_set_frame_callback,
    .set_error_callback = replay_set_error_callback,
    .start_preview = replay_start_preview,
    .stop_preview = replay_stop_preview,
    .take_picture = replay_take_picture,
    .autofocus = replay_autofocus,
    .cancel_autofocus = replay_cancel_autofocus,
    .set_display_orientation = replay_set_display_orientation,
    .get_camera2_id_list = replay_get_id_list,
    .get_static_metadata = replay_get_static_metadata,
    .get_available_keys = replay_get_available_keys,
    .configure_streams = replay_configure_streams,
    .submit_request = replay_submit_request,
    .cancel_repeating = replay_cancel_repeating,
    .flush_requests = replay_flush_requests,
};

static bool replay_load(void)
{
	static bool tried;
	const char *path = getenv("ATL_CAMERA_REPLAY");
	const char *speed = getenv("ATL_CAMERA_REPLAY_SPEED");

	if (tried)
		return rep.loaded;
	tried = true;
	rep.speed = speed && atof(speed) > 0 ? atof(speed) : 1.0;
	if (!path || !*path) {
		fprintf(stderr, "Camera replay: set ATL_CAMERA_REPLAY to a recording\n");
		return false;
	}
	rep.loaded = load_file(path);
	return rep.loaded;
}

const struct atl_camera_backend *atl_camera_backend_replay_get(void)
{
	return replay_load() ? &replay_backend : NULL;
}
