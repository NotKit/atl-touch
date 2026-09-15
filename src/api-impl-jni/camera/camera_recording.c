#include <stdlib.h>
#include <string.h>

#include <zstd.h>

#include "camera_recording.h"

/* a chunk bigger than this is a corrupt file, not a frame: the largest thing
 * here is one raw plane of a full-sensor burst frame */
#define REC_CHUNK_MAX (256u * 1024 * 1024)

/* --- writing ------------------------------------------------------------- */

void atl_rec_reserve(struct atl_rec_buf *buf, size_t len)
{
	size_t cap = buf->cap ? buf->cap : 1024;

	if (buf->len + len <= buf->cap)
		return;
	while (cap < buf->len + len)
		cap *= 2;
	buf->data = realloc(buf->data, cap);
	buf->cap = cap;
}

void atl_rec_put(struct atl_rec_buf *buf, const void *data, size_t len)
{
	atl_rec_reserve(buf, len);
	memcpy(buf->data + buf->len, data, len);
	buf->len += len;
}

void atl_rec_put_u32(struct atl_rec_buf *buf, uint32_t value)
{
	uint8_t bytes[4] = {value, value >> 8, value >> 16, value >> 24};

	atl_rec_put(buf, bytes, sizeof(bytes));
}

void atl_rec_put_i64(struct atl_rec_buf *buf, int64_t value)
{
	uint64_t raw = (uint64_t)value;

	atl_rec_put_u32(buf, (uint32_t)raw);
	atl_rec_put_u32(buf, (uint32_t)(raw >> 32));
}

void atl_rec_put_str(struct atl_rec_buf *buf, const char *str)
{
	if (!str) {
		atl_rec_put_u32(buf, 0xffffffffu);
		return;
	}
	atl_rec_put_u32(buf, (uint32_t)strlen(str));
	atl_rec_put(buf, str, strlen(str));
}

void atl_rec_put_bag(struct atl_rec_buf *buf, const struct atl_camera_metadata *md)
{
	int n = md ? atl_camera_metadata_n_entries(md) : 0;

	atl_rec_put_u32(buf, (uint32_t)n);
	for (int i = 0; i < n; i++) {
		const struct atl_camera_metadata_entry *entry = atl_camera_metadata_entry_at(md, i);
		size_t bytes = atl_camera2_type_size(entry->type) * (size_t)entry->count;

		atl_rec_put_u32(buf, entry->tag);
		atl_rec_put_u32(buf, (uint32_t)entry->type);
		atl_rec_put_u32(buf, (uint32_t)entry->count);
		atl_rec_put_str(buf, entry->name);
		atl_rec_put_u32(buf, (uint32_t)bytes);
		if (bytes && entry->data)
			atl_rec_put(buf, entry->data, bytes);
	}
}

void atl_rec_buf_free(struct atl_rec_buf *buf)
{
	free(buf->data);
	buf->data = NULL;
	buf->len = buf->cap = 0;
}

/* --- reading ------------------------------------------------------------- */

bool atl_rec_get(struct atl_rec_cur *cur, void *out, size_t len)
{
	if (cur->bad || cur->pos + len > cur->len) {
		cur->bad = true;
		memset(out, 0, len);
		return false;
	}
	memcpy(out, cur->data + cur->pos, len);
	cur->pos += len;
	return true;
}

uint32_t atl_rec_get_u32(struct atl_rec_cur *cur)
{
	uint8_t bytes[4];

	if (!atl_rec_get(cur, bytes, sizeof(bytes)))
		return 0;
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
	       ((uint32_t)bytes[3] << 24);
}

int64_t atl_rec_get_i64(struct atl_rec_cur *cur)
{
	uint64_t low = atl_rec_get_u32(cur);
	uint64_t high = atl_rec_get_u32(cur);

	return (int64_t)(low | (high << 32));
}

char *atl_rec_get_str(struct atl_rec_cur *cur)
{
	uint32_t len = atl_rec_get_u32(cur);
	char *str;

	if (len == 0xffffffffu)
		return NULL;
	if (cur->bad || cur->pos + len > cur->len) {
		cur->bad = true;
		return NULL;
	}
	str = malloc(len + 1);
	memcpy(str, cur->data + cur->pos, len);
	str[len] = '\0';
	cur->pos += len;
	return str;
}

const uint8_t *atl_rec_get_blob(struct atl_rec_cur *cur, size_t len)
{
	const uint8_t *at = cur->data + cur->pos;

	if (cur->bad || cur->pos + len > cur->len) {
		cur->bad = true;
		return NULL;
	}
	cur->pos += len;
	return at;
}

struct atl_camera_metadata *atl_rec_get_bag(struct atl_rec_cur *cur)
{
	struct atl_camera_metadata *md = atl_camera_metadata_new();
	uint32_t n = atl_rec_get_u32(cur);

	for (uint32_t i = 0; i < n && !cur->bad; i++) {
		uint32_t tag = atl_rec_get_u32(cur);
		int type = (int)atl_rec_get_u32(cur);
		int count = (int)atl_rec_get_u32(cur);
		char *name = atl_rec_get_str(cur);
		uint32_t bytes = atl_rec_get_u32(cur);
		const uint8_t *data = atl_rec_get_blob(cur, bytes);

		if (!cur->bad) {
			if (name)
				atl_camera_metadata_add_vendor(md, tag, name, type, data, count);
			else
				atl_camera_metadata_add(md, tag, type, data, count);
		}
		free(name);
	}
	if (cur->bad) {
		atl_camera_metadata_free(md);
		return NULL;
	}
	return md;
}

/* --- the file itself ----------------------------------------------------- */

bool atl_rec_write_chunk(FILE *file, uint32_t type, const void *data, size_t len, int level)
{
	struct atl_rec_chunk chunk = {.type = type, .size = (uint32_t)len};
	void *packed = NULL;
	const void *payload = data;
	size_t stored = len;

	if (level > 0 && len) {
		size_t bound = ZSTD_compressBound(len);

		packed = malloc(bound);
		if (packed) {
			size_t got = ZSTD_compress(packed, bound, data, len, level);

			if (!ZSTD_isError(got) && got < len) {
				payload = packed;
				stored = got;
				chunk.flags = ATL_REC_CHUNK_ZSTD;
			}
		}
	}
	chunk.stored = (uint32_t)stored;
	if (fwrite(&chunk, sizeof(chunk), 1, file) != 1 ||
	    (stored && fwrite(payload, 1, stored, file) != stored)) {
		free(packed);
		return false;
	}
	free(packed);
	return true;
}

bool atl_rec_read_chunk(FILE *file, uint32_t *type, uint8_t **data, size_t *len)
{
	struct atl_rec_chunk chunk;
	uint8_t *stored, *plain;

	*type = 0;
	*data = NULL;
	*len = 0;
	if (fread(&chunk, sizeof(chunk), 1, file) != 1)
		return false;
	if (chunk.size > REC_CHUNK_MAX || chunk.stored > REC_CHUNK_MAX)
		return false;
	if (!chunk.stored) {
		*type = chunk.type;
		return chunk.size == 0;
	}
	stored = malloc(chunk.stored);
	if (fread(stored, 1, chunk.stored, file) != chunk.stored) {
		free(stored);
		return false;
	}
	if (!(chunk.flags & ATL_REC_CHUNK_ZSTD)) {
		*type = chunk.type;
		*data = stored;
		*len = chunk.stored;
		return true;
	}
	plain = malloc(chunk.size ? chunk.size : 1);
	size_t got = ZSTD_decompress(plain, chunk.size, stored, chunk.stored);

	free(stored);
	if (ZSTD_isError(got) || got != chunk.size) {
		free(plain);
		return false;
	}
	*type = chunk.type;
	*data = plain;
	*len = chunk.size;
	return true;
}
