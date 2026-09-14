#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera2_metadata.h"

struct atl_camera2_tag_info {
	uint32_t tag;
	const char *name;
	int type;
};

#include "camera2_tags.inc"

struct atl_camera_metadata {
	struct atl_camera_metadata_entry *entries;
	int n_entries;
	int capacity;
};

size_t atl_camera2_type_size(int type)
{
	switch (type) {
	case ATL_CAMERA2_TYPE_BYTE:
		return 1;
	case ATL_CAMERA2_TYPE_INT32:
	case ATL_CAMERA2_TYPE_FLOAT:
		return 4;
	case ATL_CAMERA2_TYPE_INT64:
	case ATL_CAMERA2_TYPE_DOUBLE:
	case ATL_CAMERA2_TYPE_RATIONAL: /* two int32: numerator, denominator */
		return 8;
	default:
		return 0;
	}
}

static const struct atl_camera2_tag_info *tag_info(uint32_t tag)
{
	int low = 0, high = (int)(sizeof(atl_camera2_tags) / sizeof(atl_camera2_tags[0])) - 1;

	while (low <= high) {
		int mid = low + (high - low) / 2;
		if (atl_camera2_tags[mid].tag == tag)
			return &atl_camera2_tags[mid];
		if (atl_camera2_tags[mid].tag < tag)
			low = mid + 1;
		else
			high = mid - 1;
	}
	return NULL;
}

uint32_t atl_camera2_tag_from_name(const char *name)
{
	size_t i;

	if (!name)
		return ATL_CAMERA2_TAG_INVALID;
	for (i = 0; i < sizeof(atl_camera2_tags) / sizeof(atl_camera2_tags[0]); i++)
		if (!strcmp(atl_camera2_tags[i].name, name))
			return atl_camera2_tags[i].tag;
	return ATL_CAMERA2_TAG_INVALID;
}

const char *atl_camera2_tag_name(uint32_t tag)
{
	const struct atl_camera2_tag_info *info = tag_info(tag);

	return info ? info->name : NULL;
}

int atl_camera2_tag_type(uint32_t tag)
{
	const struct atl_camera2_tag_info *info = tag_info(tag);

	return info ? info->type : -1;
}

/* --- the vendor tag registry --------------------------------------------- */

/* entries are only ever appended; the names are strdup'd and live for the
 * process, so a name pointer handed out stays valid outside the lock */
static struct atl_camera2_tag_info *vendor_tags;
static int n_vendor_tags, vendor_tags_capacity;
static pthread_mutex_t vendor_tags_lock = PTHREAD_MUTEX_INITIALIZER;

void atl_camera2_vendor_register(uint32_t tag, const char *name, int type)
{
	int i;

	if (!name)
		return;
	pthread_mutex_lock(&vendor_tags_lock);
	for (i = 0; i < n_vendor_tags; i++) {
		if (vendor_tags[i].tag == tag) {
			/* a tag first seen in a key list registers without a type; an
			 * entry seen later knows it */
			if (vendor_tags[i].type < 0)
				vendor_tags[i].type = type;
			pthread_mutex_unlock(&vendor_tags_lock);
			return;
		}
	}
	if (n_vendor_tags == vendor_tags_capacity) {
		int capacity = vendor_tags_capacity ? vendor_tags_capacity * 2 : 64;
		struct atl_camera2_tag_info *grown =
		    realloc(vendor_tags, sizeof(*grown) * (size_t)capacity);
		if (!grown) {
			pthread_mutex_unlock(&vendor_tags_lock);
			return;
		}
		vendor_tags = grown;
		vendor_tags_capacity = capacity;
	}
	vendor_tags[n_vendor_tags].tag = tag;
	vendor_tags[n_vendor_tags].name = strdup(name);
	vendor_tags[n_vendor_tags].type = type;
	if (vendor_tags[n_vendor_tags].name)
		n_vendor_tags++;
	pthread_mutex_unlock(&vendor_tags_lock);
}

const char *atl_camera2_vendor_name(uint32_t tag)
{
	const char *name = NULL;
	int i;

	pthread_mutex_lock(&vendor_tags_lock);
	for (i = 0; i < n_vendor_tags; i++)
		if (vendor_tags[i].tag == tag) {
			name = vendor_tags[i].name;
			break;
		}
	pthread_mutex_unlock(&vendor_tags_lock);
	return name;
}

int atl_camera2_vendor_type(uint32_t tag)
{
	int type = -1;
	int i;

	pthread_mutex_lock(&vendor_tags_lock);
	for (i = 0; i < n_vendor_tags; i++)
		if (vendor_tags[i].tag == tag) {
			type = vendor_tags[i].type;
			break;
		}
	pthread_mutex_unlock(&vendor_tags_lock);
	return type;
}

uint32_t atl_camera2_vendor_tag_from_name(const char *name)
{
	uint32_t tag = ATL_CAMERA2_TAG_INVALID;
	int i;

	if (!name)
		return tag;
	pthread_mutex_lock(&vendor_tags_lock);
	for (i = 0; i < n_vendor_tags; i++)
		if (!strcmp(vendor_tags[i].name, name)) {
			tag = vendor_tags[i].tag;
			break;
		}
	pthread_mutex_unlock(&vendor_tags_lock);
	return tag;
}

struct atl_camera_metadata *atl_camera_metadata_new(void)
{
	return calloc(1, sizeof(struct atl_camera_metadata));
}

static bool metadata_add(struct atl_camera_metadata *md, uint32_t tag, const char *name,
                         int type, const void *data, int count);

struct atl_camera_metadata *atl_camera_metadata_copy(const struct atl_camera_metadata *md)
{
	struct atl_camera_metadata *copy;
	int i;

	if (!md)
		return NULL;
	copy = atl_camera_metadata_new();
	if (!copy)
		return NULL;
	for (i = 0; i < md->n_entries; i++) {
		const struct atl_camera_metadata_entry *entry = &md->entries[i];

		metadata_add(copy, entry->tag, entry->name, entry->type, entry->data, entry->count);
	}
	return copy;
}

void atl_camera_metadata_free(struct atl_camera_metadata *md)
{
	int i;

	if (!md)
		return;
	for (i = 0; i < md->n_entries; i++) {
		free(md->entries[i].name);
		free(md->entries[i].data);
	}
	free(md->entries);
	free(md);
}

static bool metadata_add(struct atl_camera_metadata *md, uint32_t tag, const char *name,
                         int type, const void *data, int count)
{
	size_t size = atl_camera2_type_size(type);
	struct atl_camera_metadata_entry *entry = NULL;
	void *copy;
	int i;

	if (!md || !size || count < 0)
		return false;

	copy = count ? malloc(size * (size_t)count) : NULL;
	if (count && !copy)
		return false;
	if (copy)
		memcpy(copy, data, size * (size_t)count);

	for (i = 0; i < md->n_entries; i++) {
		if (md->entries[i].tag == tag) {
			entry = &md->entries[i];
			free(entry->name);
			free(entry->data);
			break;
		}
	}
	if (!entry) {
		if (md->n_entries == md->capacity) {
			int capacity = md->capacity ? md->capacity * 2 : 32;
			struct atl_camera_metadata_entry *entries =
			    realloc(md->entries, sizeof(*entries) * (size_t)capacity);
			if (!entries) {
				free(copy);
				return false;
			}
			md->entries = entries;
			md->capacity = capacity;
		}
		entry = &md->entries[md->n_entries++];
	}

	entry->tag = tag;
	entry->type = type;
	entry->count = count;
	entry->name = name ? strdup(name) : NULL;
	entry->data = copy;
	return true;
}

bool atl_camera_metadata_add(struct atl_camera_metadata *md, uint32_t tag,
                             int type, const void *data, int count)
{
	return metadata_add(md, tag, NULL, type, data, count);
}

bool atl_camera_metadata_add_vendor(struct atl_camera_metadata *md, uint32_t tag,
                                    const char *name, int type, const void *data, int count)
{
	return metadata_add(md, tag, name, type, data, count);
}

void atl_camera_metadata_remove(struct atl_camera_metadata *md, uint32_t tag)
{
	int i;

	if (!md)
		return;
	for (i = 0; i < md->n_entries; i++) {
		if (md->entries[i].tag != tag)
			continue;
		free(md->entries[i].name);
		free(md->entries[i].data);
		md->entries[i] = md->entries[--md->n_entries];
		return;
	}
}

int atl_camera_metadata_n_entries(const struct atl_camera_metadata *md)
{
	return md ? md->n_entries : 0;
}

/* --- narrowing a HAL's characteristics to what ATL can deliver ----------- */

/*
 * ATL's camera2 session produces exactly three image formats — the preview
 * texture (PRIVATE), NV21 planes (YUV_420_888) and an encoded still (JPEG).
 * Everything else in a real HAL's stream-configuration list (RAW10/RAW16,
 * depth, HEIC, JPEG/R) is a stream ATL would accept and never fill.
 */
static bool format_deliverable(int32_t format)
{
	return format == 0x22 /* PRIVATE / IMPLEMENTATION_DEFINED */ ||
	       format == 0x23 /* YUV_420_888 */ ||
	       format == 0x100 /* JPEG */;
}

/* the format is the first value of each (format, width, height, x) tuple */
static int narrow_configs(struct atl_camera_metadata *md, const char *name)
{
	uint32_t tag = atl_camera2_tag_from_name(name);
	struct atl_camera_metadata_entry *entry = NULL;
	int kept = 0, dropped = 0, i;

	for (i = 0; md && tag != ATL_CAMERA2_TAG_INVALID && i < md->n_entries; i++)
		if (md->entries[i].tag == tag)
			entry = &md->entries[i];
	if (!entry || entry->count % 4)
		return 0;

	for (i = 0; i < entry->count; i += 4) {
		int32_t format = entry->type == ATL_CAMERA2_TYPE_INT64
		                     ? (int32_t)((const int64_t *)entry->data)[i]
		                     : ((const int32_t *)entry->data)[i];
		if (!format_deliverable(format)) {
			dropped++;
			continue;
		}
		if (kept != i) {
			size_t size = atl_camera2_type_size(entry->type);
			memmove((char *)entry->data + size * (size_t)kept,
			        (char *)entry->data + size * (size_t)i, size * 4);
		}
		kept += 4;
	}
	entry->count = kept;
	return dropped;
}

static void narrow_capabilities(struct atl_camera_metadata *md)
{
	uint32_t tag = atl_camera2_tag_from_name("android.request.availableCapabilities");
	struct atl_camera_metadata_entry *entry = NULL;
	int kept = 0, i;

	for (i = 0; md && tag != ATL_CAMERA2_TAG_INVALID && i < md->n_entries; i++)
		if (md->entries[i].tag == tag)
			entry = &md->entries[i];
	if (!entry || entry->type != ATL_CAMERA2_TYPE_BYTE)
		return;

	for (i = 0; i < entry->count; i++) {
		uint8_t cap = ((const uint8_t *)entry->data)[i];

		/* RAW and DEPTH_OUTPUT promise stream formats ATL cannot fill */
		if (cap == 3 || cap == 8)
			continue;
		((uint8_t *)entry->data)[kept++] = cap;
	}
	entry->count = kept;
}

int atl_camera_metadata_narrow_streams(struct atl_camera_metadata *md)
{
	static const char *const config_lists[] = {
	    "android.scaler.availableStreamConfigurations",
	    "android.scaler.availableMinFrameDurations",
	    "android.scaler.availableStallDurations",
	};
	static const char *const drop[] = {
	    "android.depth.availableDepthStreamConfigurations",
	    "android.depth.availableDepthMinFrameDurations",
	    "android.depth.availableDepthStallDurations",
	    "android.heic.availableHeicStreamConfigurations",
	    "android.heic.availableHeicMinFrameDurations",
	    "android.heic.availableHeicStallDurations",
	    "android.jpegr.availableJpegRStreamConfigurations",
	    "android.jpegr.availableJpegRMinFrameDurations",
	    "android.jpegr.availableJpegRStallDurations",
	};
	int dropped = 0;
	size_t i;

	if (!md)
		return 0;
	for (i = 0; i < sizeof(config_lists) / sizeof(*config_lists); i++)
		dropped += narrow_configs(md, config_lists[i]);
	for (i = 0; i < sizeof(drop) / sizeof(*drop); i++) {
		uint32_t tag = atl_camera2_tag_from_name(drop[i]);

		if (tag != ATL_CAMERA2_TAG_INVALID)
			atl_camera_metadata_remove(md, tag);
	}
	narrow_capabilities(md);
	return dropped;
}

const struct atl_camera_metadata_entry *atl_camera_metadata_entry_at(const struct atl_camera_metadata *md, int i)
{
	if (!md || i < 0 || i >= md->n_entries)
		return NULL;
	return &md->entries[i];
}

const struct atl_camera_metadata_entry *atl_camera_metadata_find(const struct atl_camera_metadata *md, uint32_t tag)
{
	int i;

	if (!md)
		return NULL;
	for (i = 0; i < md->n_entries; i++)
		if (md->entries[i].tag == tag)
			return &md->entries[i];
	return NULL;
}

uint32_t atl_camera_metadata_tag_from_name(const struct atl_camera_metadata *md, const char *name)
{
	uint32_t tag = atl_camera2_tag_from_name(name);
	int i;

	if (tag != ATL_CAMERA2_TAG_INVALID || !md || !name)
		return tag;
	for (i = 0; i < md->n_entries; i++)
		if (md->entries[i].name && !strcmp(md->entries[i].name, name))
			return md->entries[i].tag;
	return atl_camera2_vendor_tag_from_name(name);
}

const char *atl_camera_metadata_tag_name(const struct atl_camera_metadata *md, uint32_t tag)
{
	const char *name = atl_camera2_tag_name(tag);
	const struct atl_camera_metadata_entry *entry;

	if (name)
		return name;
	entry = atl_camera_metadata_find(md, tag);
	if (entry && entry->name)
		return entry->name;
	return atl_camera2_vendor_name(tag);
}

/* one value of an entry, in the entry's own type */
static void dump_value(FILE *out, const struct atl_camera_metadata_entry *entry, int i)
{
	switch (entry->type) {
	case ATL_CAMERA2_TYPE_BYTE:
		fprintf(out, " %u", ((const uint8_t *)entry->data)[i]);
		break;
	case ATL_CAMERA2_TYPE_INT32:
		fprintf(out, " %d", ((const int32_t *)entry->data)[i]);
		break;
	case ATL_CAMERA2_TYPE_FLOAT:
		fprintf(out, " %g", (double)((const float *)entry->data)[i]);
		break;
	case ATL_CAMERA2_TYPE_INT64:
		fprintf(out, " %lld", (long long)((const int64_t *)entry->data)[i]);
		break;
	case ATL_CAMERA2_TYPE_DOUBLE:
		fprintf(out, " %g", ((const double *)entry->data)[i]);
		break;
	case ATL_CAMERA2_TYPE_RATIONAL: {
		const int32_t *rational = (const int32_t *)entry->data + i * 2;

		fprintf(out, " %d/%d", rational[0], rational[1]);
		break;
	}
	}
}

void atl_camera_metadata_dump(const struct atl_camera_metadata *md, const char *label)
{
	static const char *const type_names[] = {"byte", "int32", "float", "int64", "double", "rational"};
	const char *path = getenv("ATL_CAMERA_DUMP_METADATA");
	FILE *out;
	int i, j;

	if (!path || !*path || !md)
		return;
	out = fopen(path, "a");
	if (!out) {
		fprintf(stderr, "Camera: cannot write metadata dump to %s\n", path);
		return;
	}

	fprintf(out, "== %s: %d entries ==\n", label ? label : "metadata", md->n_entries);
	for (i = 0; i < md->n_entries; i++) {
		const struct atl_camera_metadata_entry *entry = &md->entries[i];
		const char *name = atl_camera_metadata_tag_name(md, entry->tag);
		int type = entry->type;

		fprintf(out, "%s (0x%08x) %s[%d] =", name ? name : "<vendor tag>", entry->tag,
		        type >= 0 && type < (int)(sizeof(type_names) / sizeof(type_names[0]))
		            ? type_names[type] : "?",
		        entry->count);
		/* a lens shading map is thousands of floats; the shape is the point */
		for (j = 0; j < entry->count && j < 32; j++)
			dump_value(out, entry, j);
		if (entry->count > 32)
			fprintf(out, " ...");
		fprintf(out, "\n");
	}
	fclose(out);
}
