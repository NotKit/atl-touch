#ifndef ATL_CAMERA2_METADATA_H
#define ATL_CAMERA2_METADATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * camera2 metadata: a bag of (tag, type, values) entries plus the generated
 * name <-> tag <-> type table (camera2_tags.inc, from the vendored NDK header).
 *
 * Backends build one of these per camera for the static characteristics; the
 * JNI layer hands the entries to android.hardware.camera2. Tags ATL does not
 * know stay in the bag and are read by id, so a Pixel-only vendor key never
 * throws — it just reads back as raw values.
 */

/* value types, same numbering as ACAMERA_TYPE_* */
#define ATL_CAMERA2_TYPE_BYTE     0
#define ATL_CAMERA2_TYPE_INT32    1
#define ATL_CAMERA2_TYPE_FLOAT    2
#define ATL_CAMERA2_TYPE_INT64    3
#define ATL_CAMERA2_TYPE_DOUBLE   4
#define ATL_CAMERA2_TYPE_RATIONAL 5

/* android.colorCorrection.mode is tag 0, so "no such tag" needs its own value */
#define ATL_CAMERA2_TAG_INVALID 0xffffffffu

/* which key set get_available_keys() is asked for */
#define ATL_CAMERA2_KEYS_CHARACTERISTICS 0
#define ATL_CAMERA2_KEYS_REQUEST         1
#define ATL_CAMERA2_KEYS_RESULT          2

struct atl_camera_metadata_entry {
	uint32_t tag;
	int type;  /* ATL_CAMERA2_TYPE_* */
	int count; /* number of values, not bytes */
	/* vendor tags only: the "com.vendor.section.name" the app looks up.
	 * NULL for tags in the generated table. */
	char *name;
	void *data;
};

struct atl_camera_metadata;

struct atl_camera_metadata *atl_camera_metadata_new(void);
/* a deep copy, NULL for a NULL bag */
struct atl_camera_metadata *atl_camera_metadata_copy(const struct atl_camera_metadata *md);
void atl_camera_metadata_free(struct atl_camera_metadata *md);

/* copies count values of the tag's type; replaces an existing entry */
bool atl_camera_metadata_add(struct atl_camera_metadata *md, uint32_t tag,
                             int type, const void *data, int count);
/* same, for a tag outside the generated table (name is copied) */
bool atl_camera_metadata_add_vendor(struct atl_camera_metadata *md, uint32_t tag,
                                    const char *name, int type, const void *data, int count);

/* drops the tag if it is there; entry order is not preserved */
void atl_camera_metadata_remove(struct atl_camera_metadata *md, uint32_t tag);

/*
 * Cuts a real HAL's characteristics down to what ATL's camera2 can actually
 * deliver: the stream-configuration, min-frame-duration and stall-duration
 * lists keep only PRIVATE/YUV_420_888/JPEG, the depth, HEIC and JPEG/R lists
 * go, and the RAW and DEPTH_OUTPUT capabilities go with them. An app that
 * believes the HAL otherwise builds a RAW or depth stream ATL never fills.
 * Returns the number of stream configurations dropped.
 */
int atl_camera_metadata_narrow_streams(struct atl_camera_metadata *md);

int atl_camera_metadata_n_entries(const struct atl_camera_metadata *md);
const struct atl_camera_metadata_entry *atl_camera_metadata_entry_at(const struct atl_camera_metadata *md, int i);
const struct atl_camera_metadata_entry *atl_camera_metadata_find(const struct atl_camera_metadata *md, uint32_t tag);
/* by java name ("android.lens.facing" or a vendor name);
 * ATL_CAMERA2_TAG_INVALID when unknown */
uint32_t atl_camera_metadata_tag_from_name(const struct atl_camera_metadata *md, const char *name);
/* the name of a tag as this camera sees it (generated table, then its own
 * vendor entries); NULL when nothing names it */
const char *atl_camera_metadata_tag_name(const struct atl_camera_metadata *md, uint32_t tag);

/* Appends a readable dump of the bag to the file ATL_CAMERA_DUMP_METADATA
 * names, labelled with what it belongs to; a no-op when that is unset. The
 * device backends' metadata is otherwise only visible through an app. */
void atl_camera_metadata_dump(const struct atl_camera_metadata *md, const char *label);

/* generated table (camera2_tags.inc) */
uint32_t atl_camera2_tag_from_name(const char *name); /* ATL_CAMERA2_TAG_INVALID when unknown */
const char *atl_camera2_tag_name(uint32_t tag);
int atl_camera2_tag_type(uint32_t tag); /* -1 when unknown */

/*
 * Vendor tags a device backend has named, shared by every bag: a process talks
 * to one camera HAL, and its vendor tag set is global there too. The by-name
 * and by-tag lookups above the bag (tag_from_name, tag_name) fall back to this
 * registry, so a vendor Key resolves even against a bag that carries no such
 * entry yet — which is what a CaptureRequest.Builder's bag is.
 */
void atl_camera2_vendor_register(uint32_t tag, const char *name, int type);
const char *atl_camera2_vendor_name(uint32_t tag);    /* NULL when unregistered */
int atl_camera2_vendor_type(uint32_t tag);            /* -1 when unregistered */
uint32_t atl_camera2_vendor_tag_from_name(const char *name);

size_t atl_camera2_type_size(int type);

#endif
