#ifndef ATL_CAMERA_RECORDING_H
#define ATL_CAMERA_RECORDING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "camera_backend.h"
#include "camera2_metadata.h"

/*
 * The on-disk shape of a camera2 recording: everything that crossed the
 * backend vtable during a capture, so the same frames can be replayed into an
 * app on a desktop (camera_record.c writes it, camera_replay.c reads it).
 *
 * A file is a header followed by chunks, each one length-prefixed and
 * individually zstd'd, so a reader can stream through it and skip what it does
 * not care about. Both ends of this are little-endian machines (an arm64 phone
 * and an x86-64 desktop) and the encoders below are explicit about it anyway.
 */

#define ATL_REC_MAGIC   "ATLCAM2R"
#define ATL_REC_VERSION 1

/* chunk types, in the order a session produces them */
#define ATL_REC_CAMERA  1 /* a camera's static characteristics and key lists */
#define ATL_REC_SESSION 2 /* the streams the app configured */
#define ATL_REC_REQUEST 3 /* a request as submitted, settings and all */
#define ATL_REC_STARTED 4
#define ATL_REC_RESULT  5
#define ATL_REC_FAILED  6
#define ATL_REC_LOST    7
#define ATL_REC_BUFFER  8
#define ATL_REC_TRIGGER 9 /* the capture the burst is centred on */

/* chunk flags */
#define ATL_REC_CHUNK_ZSTD 1

/* buffer flags */
#define ATL_REC_BUFFER_REFERENCE 1 /* downsampled: for the eye, not for a merge */

struct atl_rec_header {
	char magic[8];
	uint32_t version;
	uint32_t flags;
	int64_t wall_clock_us; /* when the recording was written */
	char backend[16];      /* the backend the frames came from */
};

struct atl_rec_chunk {
	uint32_t type;
	uint32_t flags;
	uint32_t stored; /* bytes in the file after this header */
	uint32_t size;   /* bytes once decompressed */
};

/* --- writing ------------------------------------------------------------- */

/* a growing byte buffer; data is malloc'd and owned by the caller */
struct atl_rec_buf {
	uint8_t *data;
	size_t len;
	size_t cap;
};

/* room for len more bytes, so a frame-sized append is one allocation */
void atl_rec_reserve(struct atl_rec_buf *buf, size_t len);
void atl_rec_put(struct atl_rec_buf *buf, const void *data, size_t len);
void atl_rec_put_u32(struct atl_rec_buf *buf, uint32_t value);
void atl_rec_put_i64(struct atl_rec_buf *buf, int64_t value);
/* a length-prefixed string; NULL and "" are distinct */
void atl_rec_put_str(struct atl_rec_buf *buf, const char *str);
void atl_rec_put_bag(struct atl_rec_buf *buf, const struct atl_camera_metadata *md);
void atl_rec_buf_free(struct atl_rec_buf *buf);

/* --- reading ------------------------------------------------------------- */

/* a cursor over one decompressed chunk; every read past the end sets bad and
 * returns zero, so a truncated file is caught once at the end of the parse */
struct atl_rec_cur {
	const uint8_t *data;
	size_t len;
	size_t pos;
	bool bad;
};

bool atl_rec_get(struct atl_rec_cur *cur, void *out, size_t len);
uint32_t atl_rec_get_u32(struct atl_rec_cur *cur);
int64_t atl_rec_get_i64(struct atl_rec_cur *cur);
/* a copy the caller frees, or NULL */
char *atl_rec_get_str(struct atl_rec_cur *cur);
/* the bytes stay the cursor's; NULL when the run does not fit */
const uint8_t *atl_rec_get_blob(struct atl_rec_cur *cur, size_t len);
struct atl_camera_metadata *atl_rec_get_bag(struct atl_rec_cur *cur);

/* --- the file itself ----------------------------------------------------- */

/* compress and write one chunk; level 0 stores it as it is */
bool atl_rec_write_chunk(FILE *file, uint32_t type, const void *data, size_t len, int level);
/* read the next chunk, decompressed into a buffer the caller frees. Returns
 * false at the end of the file and on a broken one, with *type left 0. */
bool atl_rec_read_chunk(FILE *file, uint32_t *type, uint8_t **data, size_t *len);

#endif
