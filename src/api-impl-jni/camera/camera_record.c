#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <glib.h>

#include "camera_backend.h"
#include "camera_frame.h"
#include "camera_record.h"
#include "camera_recording.h"
#include "hardware_buffer.h"

/*
 * ATL_CAMERA_RECORD: everything that crosses the camera2 session, kept in a
 * ring in memory, and written out as one zstd'd file around a capture
 * (camera_recording.h is the format, camera_replay.c plays it back).
 *
 * The tap is camera_streams.c: every event of a session goes through it,
 * whether the backend serves the streams itself or the emulation does, and
 * what comes past is exactly what a replay has to produce.
 *
 * The ring holds pixels, so a frame is memcpy'd out of the HAL buffer on the
 * delivery thread (the buffer has to go back to its AImageReader or the stream
 * stalls) and compressed on the writer thread.
 */

#define REC_DEFAULT_MB      512
#define REC_DEFAULT_AFTER   12
#define REC_DEFAULT_LEVEL   1
#define REC_DEFAULT_PREVIEW 640

/* how a burst is triggered */
#define REC_TRIGGER_ONESHOT 0 /* any non-repeating request: the shutter */
#define REC_TRIGGER_INTENT  1 /* only one whose intent is STILL_CAPTURE */
#define REC_TRIGGER_MANUAL  2 /* only the trigger file */

/* one serialized chunk waiting to be written; it owns its bytes, which are the
 * buffer they were built in - a frame is big enough that a second copy of it
 * on the delivery thread is a frame rate */
struct rec_event {
	struct rec_event *next;
	uint32_t type;
	size_t len;
	bool keep; /* the burst: the ring may not push this one out */
	uint8_t *data;
};

struct rec_list {
	struct rec_event *head, *tail;
	size_t bytes;
	int n;
};

/* one capture handed to the writer thread */
struct rec_job {
	struct rec_list preamble;
	struct rec_list events;
};

/* per configured session */
struct rec_session {
	struct atl_camera_streams *streams;
	struct rec_list preamble; /* the camera and session chunks */
	/* the newest repeating request, kept out of the ring: it is submitted once
	 * and then ages out, and it is the 3A the app asked for on every frame */
	struct rec_event *repeating;
	struct rec_list ring;
	bool armed;    /* a trigger fired, the burst is being kept */
	int remaining; /* frames still to record after it */
	int64_t trigger_us;
	int refs; /* callbacks inside this session right now */
	bool ended;
	uint64_t frames;
	uint64_t dropped; /* events the ring budget pushed out */
};

/* the backend the recorded frames came from, for the file's header */
static const struct atl_camera_backend *rec_backend;

static const char *rec_dir;
static size_t rec_budget;
static int rec_after, rec_level, rec_preview, rec_trigger;

static GMutex rec_lock;
static GCond rec_cond; /* a job to write */
static GCond rec_done; /* one was written */
static bool rec_writing;
static GThread *rec_writer;
static GQueue *rec_jobs;    /* struct rec_job *, owned by the queue */
static bool rec_stop;
static bool rec_manual;     /* the trigger file appeared */
static int rec_written;

/* configured sessions; an app has one, and one more while it reconfigures */
static struct rec_session *rec_sessions[4];

static int env_int(const char *name, int fallback)
{
	const char *value = getenv(name);

	return value && *value ? atoi(value) : fallback;
}

/* call with rec_lock held */
static struct rec_session *session_of(struct atl_camera_streams *streams)
{
	for (unsigned i = 0; i < G_N_ELEMENTS(rec_sessions); i++)
		if (rec_sessions[i] && rec_sessions[i]->streams == streams)
			return rec_sessions[i];
	return NULL;
}

/*
 * A callback holds the session while it uses it: the app thread can end a
 * session (a reconfigure, a close) while a delivery thread is in the middle of
 * recording a frame of it, and the last one out does the freeing.
 */
static struct rec_session *session_acquire(struct atl_camera_streams *streams)
{
	struct rec_session *session;

	if (!rec_dir)
		return NULL;
	g_mutex_lock(&rec_lock);
	session = session_of(streams);
	if (session)
		session->refs++;
	g_mutex_unlock(&rec_lock);
	return session;
}

static void session_free(struct rec_session *session)
{
	if (session->repeating) {
		free(session->repeating->data);
		free(session->repeating);
	}
	fprintf(stderr, "Camera record: %" G_GUINT64_FORMAT " frame(s) seen, "
	                "%" G_GUINT64_FORMAT " event(s) aged out of the ring\n",
	        session->frames, session->dropped);
	free(session);
}

static void session_release(struct rec_session *session)
{
	bool last;

	g_mutex_lock(&rec_lock);
	last = --session->refs == 0 && session->ended;
	g_mutex_unlock(&rec_lock);
	if (last)
		session_free(session);
}

/* --- the ring ------------------------------------------------------------ */

/* takes the buffer's bytes; buf is empty afterwards */
static struct rec_event *event_new(uint32_t type, struct atl_rec_buf *buf)
{
	struct rec_event *event = malloc(sizeof(*event));

	event->next = NULL;
	event->type = type;
	event->len = buf->len;
	event->data = buf->data;
	*buf = (struct atl_rec_buf){0};
	return event;
}

static void list_push(struct rec_list *list, struct rec_event *event)
{
	if (list->tail)
		list->tail->next = event;
	else
		list->head = event;
	list->tail = event;
	list->bytes += event->len;
	list->n++;
}

static void list_clear(struct rec_list *list)
{
	struct rec_event *event = list->head;

	while (event) {
		struct rec_event *next = event->next;

		free(event->data);
		free(event);
		event = next;
	}
	list->head = list->tail = NULL;
	list->bytes = 0;
	list->n = 0;
}

/* call with rec_lock held */
static void ring_push_locked(struct rec_session *session, struct rec_event *event)
{
	list_push(&session->ring, event);
	/* the oldest events go first: a replay that starts a few frames late is
	 * still a replay, one that runs out of memory is not. The burst itself is
	 * never dropped - it is what was asked for - so a ring smaller than one
	 * holds the burst and nothing else */
	while (session->ring.bytes > rec_budget && session->ring.head != session->ring.tail &&
	       !session->ring.head->keep) {
		struct rec_event *old = session->ring.head;

		session->ring.head = old->next;
		session->ring.bytes -= old->len;
		session->ring.n--;
		session->dropped++;
		free(old->data);
		free(old);
	}
	if (!session->ring.head)
		session->ring.tail = NULL;
}

static void record(struct rec_session *session, uint32_t type, struct atl_rec_buf *buf)
{
	struct rec_event *event = event_new(type, buf);

	g_mutex_lock(&rec_lock);
	event->keep = session->armed || type == ATL_REC_TRIGGER;
	ring_push_locked(session, event);
	g_mutex_unlock(&rec_lock);
}

/* --- the writer thread --------------------------------------------------- */

static void job_write(struct rec_job *job)
{
	struct atl_rec_header header = {0};
	char stamp[32], *path;
	time_t now = time(NULL);
	struct tm tm;
	FILE *file;
	size_t bytes = 0;
	int chunks = 0;

	strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", localtime_r(&now, &tm));
	path = g_strdup_printf("%s/capture-%s-%d.atlcam", rec_dir, stamp, rec_written++);
	file = fopen(path, "wb");
	if (!file) {
		fprintf(stderr, "Camera record: cannot write %s (%s)\n", path, strerror(errno));
		g_free(path);
		return;
	}
	memcpy(header.magic, ATL_REC_MAGIC, sizeof(header.magic));
	header.version = ATL_REC_VERSION;
	header.wall_clock_us = g_get_real_time();
	g_strlcpy(header.backend, rec_backend->name, sizeof(header.backend));
	fwrite(&header, sizeof(header), 1, file);

	for (int pass = 0; pass < 2; pass++) {
		struct rec_event *event = pass ? job->events.head : job->preamble.head;

		for (; event; event = event->next) {
			if (!atl_rec_write_chunk(file, event->type, event->data, event->len, rec_level)) {
				fprintf(stderr, "Camera record: %s is short (%s)\n", path, strerror(errno));
				break;
			}
			bytes += event->len;
			chunks++;
		}
	}
	fprintf(stderr, "Camera record: %s - %d chunks, %.1f MiB of frames in %.1f MiB on disk\n",
	        path, chunks, bytes / 1048576.0, ftell(file) / 1048576.0);
	fclose(file);
	g_free(path);
}

/* also the manual trigger's poll: <dir>/trigger, taken away once it is seen */
static gpointer writer_thread(gpointer data)
{
	char *trigger = g_strdup_printf("%s/trigger", rec_dir);

	(void)data;
	g_mutex_lock(&rec_lock);
	while (!rec_stop) {
		struct rec_job *job = g_queue_pop_head(rec_jobs);

		if (job) {
			rec_writing = true;
			g_mutex_unlock(&rec_lock);
			job_write(job);
			list_clear(&job->preamble);
			list_clear(&job->events);
			free(job);
			g_mutex_lock(&rec_lock);
			rec_writing = false;
			g_cond_broadcast(&rec_done);
			continue;
		}
		g_cond_wait_until(&rec_cond, &rec_lock, g_get_monotonic_time() + 500 * G_TIME_SPAN_MILLISECOND);
		if (!rec_manual && g_file_test(trigger, G_FILE_TEST_EXISTS)) {
			remove(trigger);
			rec_manual = true;
		}
	}
	g_mutex_unlock(&rec_lock);
	g_free(trigger);
	return NULL;
}

/* call with rec_lock held; the job takes the ring's events */
static void flush_locked(struct rec_session *session)
{
	struct rec_job *job = calloc(1, sizeof(*job));
	struct rec_event *event;

	/* the preamble is written with every capture, so it is copied, not moved */
	for (event = session->preamble.head; event; event = event->next) {
		struct rec_event *copy = malloc(sizeof(*copy));

		*copy = *event;
		copy->next = NULL;
		copy->data = malloc(event->len);
		memcpy(copy->data, event->data, event->len);
		list_push(&job->preamble, copy);
	}
	if (session->repeating) {
		struct rec_event *copy = malloc(sizeof(*copy));

		*copy = *session->repeating;
		copy->next = NULL;
		copy->data = malloc(copy->len);
		memcpy(copy->data, session->repeating->data, copy->len);
		list_push(&job->preamble, copy);
	}
	job->events = session->ring;
	session->ring = (struct rec_list){0};
	session->armed = false;
	session->remaining = 0;
	g_queue_push_tail(rec_jobs, job);
	g_cond_signal(&rec_cond);
}

/* --- what goes into the ring --------------------------------------------- */

static void record_cameras(struct rec_session *session)
{
	const char *const *ids;
	int count = 0;

	if (!rec_backend->get_camera2_id_list)
		return;
	ids = rec_backend->get_camera2_id_list(&count);
	for (int i = 0; i < count; i++) {
		struct atl_camera_metadata *md = rec_backend->get_static_metadata(ids[i]);
		struct atl_rec_buf buf = {0};
		int facing = 0, orientation = 0;

		if (rec_backend->get_camera_info)
			rec_backend->get_camera_info(atoi(ids[i]), &facing, &orientation);
		atl_rec_put_str(&buf, ids[i]);
		atl_rec_put_u32(&buf, (uint32_t)facing);
		atl_rec_put_u32(&buf, (uint32_t)orientation);
		atl_rec_put_bag(&buf, md);
		for (int which = 0; which < 3; which++) {
			const uint32_t *keys = NULL;
			int n = 0;

			if (rec_backend->get_available_keys)
				keys = rec_backend->get_available_keys(ids[i], which, &n);
			atl_rec_put_u32(&buf, (uint32_t)(keys ? n : 0));
			for (int k = 0; keys && k < n; k++)
				atl_rec_put_u32(&buf, keys[k]);
		}
		atl_camera_metadata_free(md);
		list_push(&session->preamble, event_new(ATL_REC_CAMERA, &buf));
	}
}

static void record_session(struct rec_session *session, const struct atl_camera_stream *streams,
                           int n_streams, const struct atl_camera_stream_input *input)
{
	struct atl_rec_buf buf = {0};

	atl_rec_put_u32(&buf, (uint32_t)n_streams);
	for (int i = 0; i < n_streams; i++) {
		atl_rec_put_u32(&buf, (uint32_t)streams[i].width);
		atl_rec_put_u32(&buf, (uint32_t)streams[i].height);
		atl_rec_put_u32(&buf, (uint32_t)streams[i].format);
		atl_rec_put_u32(&buf, (uint32_t)streams[i].max_buffers);
		atl_rec_put_i64(&buf, (int64_t)streams[i].usage);
		atl_rec_put_str(&buf, streams[i].physical_id);
	}
	atl_rec_put_u32(&buf, input ? 1 : 0);
	if (input) {
		atl_rec_put_u32(&buf, (uint32_t)input->width);
		atl_rec_put_u32(&buf, (uint32_t)input->height);
		atl_rec_put_u32(&buf, (uint32_t)input->format);
		atl_rec_put_u32(&buf, (uint32_t)input->max_buffers);
		atl_rec_put_u32(&buf, input->multi_resolution ? 1 : 0);
	}
	list_push(&session->preamble, event_new(ATL_REC_SESSION, &buf));
}

/*
 * A PRIVATE buffer is the app's viewfinder and has no planes a CPU may read,
 * only its gralloc handle. It is kept as a small NV21 reference - enough to see
 * what the camera was pointed at beside a burst, not enough to merge - because
 * a full-size copy of a stream nobody compares would cost the burst its ring.
 * NULL when there is no reading it; the caller frees what comes back.
 */
static uint8_t *record_reference(const struct atl_camera_buffer *buffer, int *out_width,
                                 int *out_height)
{
	struct atl_window_frame frame = {0};
	struct atl_camera_yuv420 yuv;
	int step, width, height, want;
	uint8_t *nv21;

	if (!buffer->native || rec_preview <= 0 || buffer->width <= 0 || buffer->height <= 0)
		return NULL;
	if (!atl_gralloc_lock_planes(buffer->native, &frame))
		return NULL;

	yuv = (struct atl_camera_yuv420){
	    .y = frame.planes[0].data,
	    .u = frame.planes[1].data,
	    .v = frame.planes[2].data,
	    .y_stride = (int)frame.planes[0].row_stride,
	    .u_stride = (int)frame.planes[1].row_stride,
	    .v_stride = (int)frame.planes[2].row_stride,
	    .u_pixel = (int)frame.planes[1].pixel_stride,
	    .v_pixel = (int)frame.planes[2].pixel_stride,
	    .y_len = (int)frame.planes[0].row_stride * buffer->height,
	    .u_len = (int)frame.planes[1].row_stride * buffer->height / 2,
	    .v_len = (int)frame.planes[2].row_stride * buffer->height / 2,
	};
	want = rec_preview * buffer->height / buffer->width;
	step = atl_camera_yuv420_step(buffer->width, buffer->height, rec_preview, want);
	atl_camera_yuv420_size(buffer->width, buffer->height, step, &width, &height);
	nv21 = malloc((size_t)width * height * 3 / 2);
	atl_camera_yuv420_to_nv21(nv21, width, height, &yuv, step);
	atl_gralloc_unlock(buffer->native);
	*out_width = width;
	*out_height = height;
	return nv21;
}

static void record_buffer(struct rec_session *session, const struct atl_camera_buffer *buffer)
{
	struct atl_rec_buf buf = {0};
	int width = buffer->width, height = buffer->height;
	uint8_t *reference = NULL;
	uint32_t flags = 0;
	size_t bytes = 0;

	if (buffer->n_planes > 0) {
		for (int i = 0; i < buffer->n_planes; i++)
			bytes += (size_t)buffer->planes[i].len;
	} else {
		reference = record_reference(buffer, &width, &height);
		if (reference) {
			bytes = (size_t)width * height * 3 / 2;
			flags = ATL_REC_BUFFER_REFERENCE;
		}
	}
	atl_rec_reserve(&buf, bytes + 128);

	atl_rec_put_u32(&buf, (uint32_t)buffer->stream);
	atl_rec_put_u32(&buf, (uint32_t)width);
	atl_rec_put_u32(&buf, (uint32_t)height);
	atl_rec_put_u32(&buf, (uint32_t)buffer->format);
	atl_rec_put_i64(&buf, buffer->timestamp);
	atl_rec_put_u32(&buf, flags);
	atl_rec_put_u32(&buf, (uint32_t)buffer->width); /* the stream's own size */
	atl_rec_put_u32(&buf, (uint32_t)buffer->height);
	if (reference) {
		atl_rec_put_u32(&buf, 1);
		atl_rec_put_u32(&buf, (uint32_t)bytes);
		atl_rec_put_u32(&buf, (uint32_t)width); /* row stride */
		atl_rec_put_u32(&buf, 1);               /* pixel stride */
		atl_rec_put(&buf, reference, bytes);
		free(reference);
	} else {
		atl_rec_put_u32(&buf, (uint32_t)buffer->n_planes);
		for (int i = 0; i < buffer->n_planes; i++) {
			atl_rec_put_u32(&buf, (uint32_t)buffer->planes[i].len);
			atl_rec_put_u32(&buf, (uint32_t)buffer->planes[i].row_stride);
			atl_rec_put_u32(&buf, (uint32_t)buffer->planes[i].pixel_stride);
		}
		for (int i = 0; i < buffer->n_planes; i++)
			atl_rec_put(&buf, buffer->planes[i].data, (size_t)buffer->planes[i].len);
	}
	record(session, ATL_REC_BUFFER, &buf);
}

static void trigger(struct rec_session *session, int request_id)
{
	struct atl_rec_buf buf = {0};

	g_mutex_lock(&rec_lock);
	if (session->armed) {
		g_mutex_unlock(&rec_lock);
		return;
	}
	session->armed = true;
	session->remaining = rec_after;
	session->trigger_us = g_get_monotonic_time();
	rec_manual = false;
	g_mutex_unlock(&rec_lock);

	atl_rec_put_u32(&buf, (uint32_t)request_id);
	atl_rec_put_i64(&buf, session->trigger_us);
	record(session, ATL_REC_TRIGGER, &buf);
	fprintf(stderr, "Camera record: triggered on request %d, keeping %d more frame(s)\n",
	        request_id, rec_after);
}

/* --- the tap ------------------------------------------------------------- */

static void record_once(void);

bool atl_camera_record_enabled(void)
{
	record_once();
	return rec_dir != NULL;
}

void atl_camera_record_started(struct atl_camera_streams *streams, int request_id,
                               int64_t frame_number, int64_t timestamp)
{
	struct rec_session *session = session_acquire(streams);
	struct atl_rec_buf buf = {0};

	if (!session)
		return;
	atl_rec_put_u32(&buf, (uint32_t)request_id);
	atl_rec_put_i64(&buf, frame_number);
	atl_rec_put_i64(&buf, timestamp);
	record(session, ATL_REC_STARTED, &buf);

	session->frames++;
	/* the trigger file is answered here as well as at a request: an app that
	 * only ever previews still has a burst worth keeping */
	if (rec_manual && !session->armed)
		trigger(session, request_id);
	g_mutex_lock(&rec_lock);
	if (session->armed && --session->remaining <= 0)
		flush_locked(session);
	g_mutex_unlock(&rec_lock);
	session_release(session);
}

void atl_camera_record_result(struct atl_camera_streams *streams, int request_id,
                              int64_t frame_number, const struct atl_camera_metadata *result)
{
	struct rec_session *session = session_acquire(streams);
	struct atl_rec_buf buf = {0};

	if (!session)
		return;
	atl_rec_put_u32(&buf, (uint32_t)request_id);
	atl_rec_put_i64(&buf, frame_number);
	atl_rec_put_bag(&buf, result);
	record(session, ATL_REC_RESULT, &buf);
	session_release(session);
}

void atl_camera_record_failed(struct atl_camera_streams *streams, int request_id,
                              int64_t frame_number)
{
	struct rec_session *session = session_acquire(streams);
	struct atl_rec_buf buf = {0};

	if (!session)
		return;
	atl_rec_put_u32(&buf, (uint32_t)request_id);
	atl_rec_put_i64(&buf, frame_number);
	record(session, ATL_REC_FAILED, &buf);
	session_release(session);
}

void atl_camera_record_buffer(struct atl_camera_streams *streams,
                              const struct atl_camera_buffer *buffer)
{
	struct rec_session *session = session_acquire(streams);

	if (!session)
		return;
	record_buffer(session, buffer);
	session_release(session);
}

void atl_camera_record_lost(struct atl_camera_streams *streams, int request_id,
                            int64_t frame_number, int stream)
{
	struct rec_session *session = session_acquire(streams);
	struct atl_rec_buf buf = {0};

	if (!session)
		return;
	atl_rec_put_u32(&buf, (uint32_t)request_id);
	atl_rec_put_i64(&buf, frame_number);
	atl_rec_put_u32(&buf, (uint32_t)stream);
	record(session, ATL_REC_LOST, &buf);
	session_release(session);
}

/* the app's own still capture, which is what a burst is centred on */
static bool is_still(const struct atl_camera_metadata *settings, bool repeating)
{
	const struct atl_camera_metadata_entry *intent;
	uint32_t tag;

	if (rec_manual)
		return true;
	if (rec_trigger == REC_TRIGGER_MANUAL || repeating)
		return false;
	if (rec_trigger == REC_TRIGGER_ONESHOT)
		return true;
	tag = atl_camera2_tag_from_name("android.control.captureIntent");
	intent = settings ? atl_camera_metadata_find(settings, tag) : NULL;
	/* ACAMERA_CONTROL_CAPTURE_INTENT_STILL_CAPTURE */
	return intent && intent->count > 0 && ((const uint8_t *)intent->data)[0] == 2;
}

void atl_camera_record_request(struct atl_camera_streams *streams, int request_id,
                               const struct atl_camera_metadata *settings, uint32_t targets,
                               bool repeating, bool reprocess, int64_t input_timestamp)
{
	struct rec_session *session = session_acquire(streams);
	struct atl_rec_buf buf = {0};

	if (!session)
		return;
	/* the trigger goes first, so the request that took the picture is part of
	 * the burst and not something the ring may drop */
	if (reprocess || is_still(settings, repeating))
		trigger(session, request_id);

	atl_rec_put_u32(&buf, (uint32_t)request_id);
	atl_rec_put_u32(&buf, repeating ? 1 : 0);
	atl_rec_put_u32(&buf, targets);
	atl_rec_put_u32(&buf, reprocess ? 1 : 0);
	atl_rec_put_i64(&buf, input_timestamp);
	atl_rec_put_i64(&buf, g_get_monotonic_time());
	atl_rec_put_bag(&buf, settings);
	if (repeating) {
		/* one request stands for every frame it produced, so only the newest
		 * is kept - and it is kept whole, outside the ring */
		struct rec_event *event = event_new(ATL_REC_REQUEST, &buf);

		g_mutex_lock(&rec_lock);
		if (session->repeating) {
			free(session->repeating->data);
			free(session->repeating);
		}
		session->repeating = event;
		g_mutex_unlock(&rec_lock);
	} else {
		record(session, ATL_REC_REQUEST, &buf);
	}
	session_release(session);
}

void atl_camera_record_session(struct atl_camera_streams *streams,
                               const struct atl_camera_backend *backend,
                               const struct atl_camera_stream *configs, int n_streams,
                               const struct atl_camera_stream_input *input)
{
	struct rec_session *session;

	record_once();
	if (!rec_dir)
		return;
	if (!n_streams) {
		atl_camera_record_end(streams);
		return;
	}
	rec_backend = backend;

	g_mutex_lock(&rec_lock);
	session = session_of(streams);
	for (unsigned i = 0; i < G_N_ELEMENTS(rec_sessions) && !session; i++) {
		if (!rec_sessions[i]) {
			session = calloc(1, sizeof(*session));
			session->streams = streams;
			rec_sessions[i] = session;
		}
	}
	if (!session) {
		g_mutex_unlock(&rec_lock);
		fprintf(stderr, "Camera record: too many sessions at once, this one is not recorded\n");
		return;
	}
	list_clear(&session->preamble);
	list_clear(&session->ring);
	session->armed = false;
	g_mutex_unlock(&rec_lock);

	record_cameras(session);
	record_session(session, configs, n_streams, input);
	fprintf(stderr, "Camera record: recording %d stream(s) into a %zu MiB ring, %d frame(s) "
	                "after a trigger\n", n_streams, rec_budget / 1048576, rec_after);
}

void atl_camera_record_end(struct atl_camera_streams *streams)
{
	struct rec_session *session;

	if (!rec_dir)
		return;
	g_mutex_lock(&rec_lock);
	session = session_of(streams);
	if (session) {
		if (session->armed)
			flush_locked(session);
		for (unsigned i = 0; i < G_N_ELEMENTS(rec_sessions); i++)
			if (rec_sessions[i] == session)
				rec_sessions[i] = NULL;
		list_clear(&session->preamble);
		list_clear(&session->ring);
		session->ended = true;
	}
	/* the burst has to reach the disk before the app that took it exits */
	while (!g_queue_is_empty(rec_jobs) || rec_writing)
		g_cond_wait(&rec_done, &rec_lock);
	if (session && session->refs) {
		g_mutex_unlock(&rec_lock);
		return; /* a callback still has it; the last one out frees it */
	}
	g_mutex_unlock(&rec_lock);
	if (session)
		session_free(session);
}

/* --- setup --------------------------------------------------------------- */

/* the writer thread and the ring's budget, once, from the environment */
static void record_setup(void)
{
	const char *trigger_name = getenv("ATL_CAMERA_RECORD_TRIGGER");
	const char *dir = getenv("ATL_CAMERA_RECORD");

	if (!dir || !*dir)
		return;
	if (g_mkdir_with_parents(dir, 0755) != 0) {
		fprintf(stderr, "Camera record: cannot use %s (%s)\n", dir, strerror(errno));
		return;
	}
	rec_budget = (size_t)env_int("ATL_CAMERA_RECORD_MB", REC_DEFAULT_MB) * 1048576;
	rec_after = env_int("ATL_CAMERA_RECORD_AFTER", REC_DEFAULT_AFTER);
	rec_level = env_int("ATL_CAMERA_RECORD_LEVEL", REC_DEFAULT_LEVEL);
	rec_preview = env_int("ATL_CAMERA_RECORD_PREVIEW", REC_DEFAULT_PREVIEW);
	if (trigger_name && !strcmp(trigger_name, "intent"))
		rec_trigger = REC_TRIGGER_INTENT;
	else if (trigger_name && !strcmp(trigger_name, "manual"))
		rec_trigger = REC_TRIGGER_MANUAL;

	g_mutex_init(&rec_lock);
	g_cond_init(&rec_cond);
	g_cond_init(&rec_done);
	rec_jobs = g_queue_new();
	rec_dir = dir;
	rec_writer = g_thread_new("atl-camera-record", writer_thread, NULL);
	fprintf(stderr, "Camera record: writing captures to %s (%zu MiB ring, trigger '%s')\n",
	        rec_dir, rec_budget / 1048576, trigger_name ? trigger_name : "oneshot");
}

/* the first session sets the recorder up; a process with no camera pays for
 * none of it, and neither does one that never configures a session */
static void record_once(void)
{
	static gsize once;

	if (g_once_init_enter(&once)) {
		record_setup();
		g_once_init_leave(&once, 1);
	}
}
