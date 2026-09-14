/*
 * libcamera2ndk.so for an app's own native code, forwarded to the device's
 * Android library through libhybris. See hybris_ndk.h.
 *
 * The symbols here are the ones Google Camera's libgcastartup.so imports: it
 * enumerates cameras and reads static characteristics from native code, and
 * drives the session itself from Java. Anything else an app asks for shows up
 * as a link error naming the symbol, which is the signal to add it here.
 *
 * ACameraMetadata_fromCameraMetadata is the one call that cannot be forwarded:
 * it reads the native pointer out of a Java camera2 metadata object, and under
 * ATL that pointer is one of ATL's own bags (struct atl_camera_metadata), not
 * an Android camera_metadata_t. Handing it to the Android implementation would
 * be a type confusion. So it builds a handle of ATL's own instead, copied out
 * of the Java object over JNI, and the read calls below serve ATL's handles
 * themselves and forward everything else - see "ATL's own handles" below.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <third_party/android-headers/android_compat.h>

#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>

#include "hybris_ndk.h"

#define SONAME "libcamera2ndk.so"

/*
 * Creating the manager is the app's first camera2 call, and on this port it is
 * also the last moment at which the binder thread pool can still be started in
 * time: the HAL dequeues buffers by calling back into this process.
 */
ACameraManager *ACameraManager_create(void)
{
	static ACameraManager *(*real)(void);

	atl_hybris_ndk_binder_pool();
	if (!real) {
		real = atl_hybris_ndk_sym(SONAME, "ACameraManager_create");
		if (!real)
			return NULL;
	}
	return real();
}

ATL_HYBRIS_FORWARD_VOID(SONAME, ACameraManager_delete,
                        (ACameraManager * manager), (manager))

ATL_HYBRIS_FORWARD(SONAME, camera_status_t, ACameraManager_getCameraIdList,
                   (ACameraManager * manager, ACameraIdList **cameraIdList),
                   (manager, cameraIdList), ACAMERA_ERROR_CAMERA_DISCONNECTED)

ATL_HYBRIS_FORWARD_VOID(SONAME, ACameraManager_deleteCameraIdList,
                        (ACameraIdList * cameraIdList), (cameraIdList))

ATL_HYBRIS_FORWARD(SONAME, camera_status_t, ACameraManager_getCameraCharacteristics,
                   (ACameraManager * manager, const char *cameraId, ACameraMetadata **characteristics),
                   (manager, cameraId, characteristics), ACAMERA_ERROR_CAMERA_DISCONNECTED)

/*
 * ---- ATL's own handles ----
 *
 * The one handle ATL makes itself is the one ACameraMetadata_fromCameraMetadata
 * returns: a flat copy of a Java camera2 metadata object, read out over JNI.
 * Google Camera's HDR+ pipeline hands its native half the TotalCaptureResult of
 * every payload frame and reads the frame's tags back through the NDK, so
 * without this it adds no frames to a shot and every picture ends "One or more
 * payload frames must be provided".
 *
 * The read calls are shared with the real Android handles the rest of this file
 * forwards, so each one asks the registry first. A registry rather than a magic
 * word in the object, because a foreign handle must never be dereferenced to
 * find out whose it is.
 */

struct atl_ndk_metadata {
	int32_t n_entries;
	uint32_t *tags;                     /* one per entry, for getAllTags */
	ACameraMetadata_const_entry *entries;
	char **names;                       /* the tag's own name, or NULL */
};

static struct atl_ndk_metadata **atl_handles;
static int atl_n_handles;
static pthread_mutex_t atl_handles_lock = PTHREAD_MUTEX_INITIALIZER;

/* the handle if it is one of ours, NULL otherwise */
static struct atl_ndk_metadata *atl_metadata_of(const ACameraMetadata *metadata)
{
	struct atl_ndk_metadata *found = NULL;

	if (!metadata)
		return NULL;
	pthread_mutex_lock(&atl_handles_lock);
	for (int i = 0; i < atl_n_handles; i++)
		if ((const void *)atl_handles[i] == (const void *)metadata) {
			found = atl_handles[i];
			break;
		}
	pthread_mutex_unlock(&atl_handles_lock);
	return found;
}

static void atl_metadata_free(struct atl_ndk_metadata *md)
{
	for (int i = 0; i < md->n_entries; i++) {
		free((void *)md->entries[i].data.u8);
		free(md->names[i]);
	}
	free(md->entries);
	free(md->names);
	free(md->tags);
	free(md);
}

/* drops it from the registry; true when it was ours */
static bool atl_metadata_forget(const ACameraMetadata *metadata)
{
	struct atl_ndk_metadata *md = NULL;

	pthread_mutex_lock(&atl_handles_lock);
	for (int i = 0; i < atl_n_handles; i++)
		if ((const void *)atl_handles[i] == (const void *)metadata) {
			md = atl_handles[i];
			atl_handles[i] = atl_handles[--atl_n_handles];
			break;
		}
	pthread_mutex_unlock(&atl_handles_lock);
	if (!md)
		return false;
	atl_metadata_free(md);
	return true;
}

static bool atl_metadata_remember(struct atl_ndk_metadata *md)
{
	struct atl_ndk_metadata **grown;

	pthread_mutex_lock(&atl_handles_lock);
	grown = realloc(atl_handles, (size_t)(atl_n_handles + 1) * sizeof(*atl_handles));
	if (grown) {
		atl_handles = grown;
		atl_handles[atl_n_handles++] = md;
	}
	pthread_mutex_unlock(&atl_handles_lock);
	return grown != NULL;
}

camera_status_t ACameraMetadata_getConstEntry(const ACameraMetadata *metadata, uint32_t tag,
                                             ACameraMetadata_const_entry *entry)
{
	static camera_status_t (*real)(const ACameraMetadata *, uint32_t,
	                               ACameraMetadata_const_entry *);
	struct atl_ndk_metadata *ours = atl_metadata_of(metadata);

	if (ours) {
		for (int i = 0; i < ours->n_entries; i++)
			if (ours->entries[i].tag == tag) {
				*entry = ours->entries[i];
				return ACAMERA_OK;
			}
		return ACAMERA_ERROR_METADATA_NOT_FOUND;
	}
	if (!real) {
		real = atl_hybris_ndk_sym(SONAME, "ACameraMetadata_getConstEntry");
		if (!real)
			return ACAMERA_ERROR_METADATA_NOT_FOUND;
	}
	return real(metadata, tag, entry);
}

camera_status_t ACameraMetadata_getAllTags(const ACameraMetadata *metadata, int32_t *numEntries,
                                           const uint32_t **tags)
{
	static camera_status_t (*real)(const ACameraMetadata *, int32_t *, const uint32_t **);
	struct atl_ndk_metadata *ours = atl_metadata_of(metadata);

	if (ours) {
		*numEntries = ours->n_entries;
		*tags = ours->tags;
		return ACAMERA_OK;
	}
	if (!real) {
		real = atl_hybris_ndk_sym(SONAME, "ACameraMetadata_getAllTags");
		if (!real)
			return ACAMERA_ERROR_UNKNOWN;
	}
	return real(metadata, numEntries, tags);
}

void ACameraMetadata_free(ACameraMetadata *metadata)
{
	static void (*real)(ACameraMetadata *);

	if (atl_metadata_forget(metadata))
		return;
	if (!real) {
		real = atl_hybris_ndk_sym(SONAME, "ACameraMetadata_free");
		if (!real)
			return;
	}
	real(metadata);
}

bool ACameraMetadata_isLogicalMultiCamera(const ACameraMetadata *staticMetadata,
                                          size_t *numPhysicalCameras,
                                          const char *const **physicalCameraIds)
{
	static bool (*real)(const ACameraMetadata *, size_t *, const char *const **);

	/* ours is a frame's result, never a camera's characteristics */
	if (atl_metadata_of(staticMetadata))
		return false;
	if (!real) {
		real = atl_hybris_ndk_sym(SONAME, "ACameraMetadata_isLogicalMultiCamera");
		if (!real)
			return false;
	}
	return real(staticMetadata, numPhysicalCameras, physicalCameraIds);
}

/*
 * Android 16's own libcamera2ndk exports this (measured on caiman); the app
 * imports it weakly, so its absence on an older device is not an error.
 */
camera_status_t ACameraMetadata_getTagFromName(const ACameraMetadata *metadata, const char *name,
                                               uint32_t *tag)
{
	static camera_status_t (*real)(const ACameraMetadata *, const char *, uint32_t *);
	struct atl_ndk_metadata *ours = atl_metadata_of(metadata);

	if (ours) {
		for (int i = 0; i < ours->n_entries; i++)
			if (ours->names[i] && !strcmp(ours->names[i], name)) {
				*tag = ours->entries[i].tag;
				return ACAMERA_OK;
			}
		return ACAMERA_ERROR_METADATA_NOT_FOUND;
	}
	if (!real) {
		real = atl_hybris_ndk_sym(SONAME, "ACameraMetadata_getTagFromName");
		if (!real)
			return ACAMERA_ERROR_METADATA_NOT_FOUND;
	}
	return real(metadata, name, tag);
}

/* the value types are numbered the same on both sides; see camera2_metadata.h */
static bool atl_read_entry(JNIEnv *env, jobject bag, jclass bag_class,
                           ACameraMetadata_const_entry *entry)
{
	static const struct {
		const char *method;
		const char *signature;
		size_t width;
	} readers[] = {
		[ACAMERA_TYPE_BYTE] = {"readBytes", "(I)[B", 1},
		[ACAMERA_TYPE_INT32] = {"readInts", "(I)[I", 4},
		[ACAMERA_TYPE_FLOAT] = {"readFloats", "(I)[F", 4},
		[ACAMERA_TYPE_INT64] = {"readLongs", "(I)[J", 8},
		[ACAMERA_TYPE_DOUBLE] = {"readDoubles", "(I)[D", 8},
		/* a rational reads as its (numerator, denominator) int pair */
		[ACAMERA_TYPE_RATIONAL] = {"readInts", "(I)[I", 4},
	};
	jmethodID read;
	jarray values;
	jsize n;
	void *data;

	if (entry->type > ACAMERA_TYPE_RATIONAL || !readers[entry->type].method)
		return false;
	read = (*env)->GetMethodID(env, bag_class, readers[entry->type].method,
	                           readers[entry->type].signature);
	if (!read)
		return false;
	values = (*env)->CallObjectMethod(env, bag, read, (jint)entry->tag);
	if (!values)
		return false;

	n = (*env)->GetArrayLength(env, values);
	data = n > 0 ? malloc((size_t)n * readers[entry->type].width) : NULL;
	if (n > 0 && !data) {
		(*env)->DeleteLocalRef(env, values);
		return false;
	}
	switch (entry->type) {
	case ACAMERA_TYPE_BYTE:
		(*env)->GetByteArrayRegion(env, values, 0, n, data);
		break;
	case ACAMERA_TYPE_INT32:
	case ACAMERA_TYPE_RATIONAL:
		(*env)->GetIntArrayRegion(env, values, 0, n, data);
		break;
	case ACAMERA_TYPE_FLOAT:
		(*env)->GetFloatArrayRegion(env, values, 0, n, data);
		break;
	case ACAMERA_TYPE_INT64:
		(*env)->GetLongArrayRegion(env, values, 0, n, data);
		break;
	case ACAMERA_TYPE_DOUBLE:
		(*env)->GetDoubleArrayRegion(env, values, 0, n, data);
		break;
	}
	(*env)->DeleteLocalRef(env, values);
	entry->count = entry->type == ACAMERA_TYPE_RATIONAL ? (uint32_t)n / 2 : (uint32_t)n;
	entry->data.u8 = data;
	return true;
}

ACameraMetadata *ACameraMetadata_fromCameraMetadata(JNIEnv *env, jobject cameraMetadata)
{
	struct atl_ndk_metadata *md;
	jclass object_class, bag_class;
	jmethodID accessor, get_type, get_name;
	jobject bag;
	jintArray tags;
	jsize n;
	jint *tag_values;

	if (!env || !cameraMetadata)
		return NULL;
	object_class = (*env)->GetObjectClass(env, cameraMetadata);
	accessor = (*env)->GetMethodID(env, object_class, "getAtlNativeMetadata",
	                               "()Landroid/hardware/camera2/impl/CameraMetadataNative;");
	if (!accessor) {
		(*env)->ExceptionClear(env);
		fprintf(stderr, "atl-ndk: ACameraMetadata_fromCameraMetadata: "
		                "not an ATL camera2 metadata object\n");
		return NULL;
	}
	bag = (*env)->CallObjectMethod(env, cameraMetadata, accessor);
	if (!bag)
		return NULL;

	bag_class = (*env)->GetObjectClass(env, bag);
	get_type = (*env)->GetMethodID(env, bag_class, "getType", "(I)I");
	get_name = (*env)->GetMethodID(env, bag_class, "getTagName", "(I)Ljava/lang/String;");
	tags = (*env)->CallObjectMethod(env, bag,
	                                (*env)->GetMethodID(env, bag_class, "getTags", "()[I"));
	if (!get_type || !get_name || !tags)
		return NULL;

	n = (*env)->GetArrayLength(env, tags);
	md = calloc(1, sizeof(*md));
	tag_values = n > 0 ? malloc((size_t)n * sizeof(*tag_values)) : NULL;
	if (md) {
		md->tags = n > 0 ? malloc((size_t)n * sizeof(*md->tags)) : NULL;
		md->entries = n > 0 ? calloc((size_t)n, sizeof(*md->entries)) : NULL;
		md->names = n > 0 ? calloc((size_t)n, sizeof(*md->names)) : NULL;
	}
	if (!md || (n > 0 && (!tag_values || !md->tags || !md->entries || !md->names))) {
		free(tag_values);
		if (md)
			atl_metadata_free(md);
		return NULL;
	}
	(*env)->GetIntArrayRegion(env, tags, 0, n, tag_values);

	for (jsize i = 0; i < n; i++) {
		ACameraMetadata_const_entry entry = {.tag = (uint32_t)tag_values[i]};
		jstring name;

		entry.type = (uint8_t)(*env)->CallIntMethod(env, bag, get_type, tag_values[i]);
		if (!atl_read_entry(env, bag, bag_class, &entry))
			continue;

		name = (*env)->CallObjectMethod(env, bag, get_name, tag_values[i]);
		if (name) {
			const char *utf = (*env)->GetStringUTFChars(env, name, NULL);

			md->names[md->n_entries] = utf ? strdup(utf) : NULL;
			if (utf)
				(*env)->ReleaseStringUTFChars(env, name, utf);
			(*env)->DeleteLocalRef(env, name);
		}
		md->tags[md->n_entries] = entry.tag;
		md->entries[md->n_entries++] = entry;
	}
	free(tag_values);

	if (!atl_metadata_remember(md)) {
		atl_metadata_free(md);
		return NULL;
	}
	return (ACameraMetadata *)md;
}
