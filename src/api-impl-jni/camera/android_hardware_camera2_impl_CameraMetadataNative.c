#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"

#include "camera2_metadata.h"
#include "camera_backend.h"

#include "../generated_headers/android_hardware_camera2_impl_CameraMetadataNative.h"

/* the jlong the Java side carries around */
static struct atl_camera_metadata *metadata_of(jlong ptr)
{
	return (struct atl_camera_metadata *)_PTR(ptr);
}

static const struct atl_camera_metadata_entry *entry_of(jlong ptr, jint tag)
{
	return atl_camera_metadata_find(metadata_of(ptr), (uint32_t)tag);
}

JNIEXPORT jobjectArray JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1getCameraIdList(JNIEnv *env, jclass class)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();
	const char *const *ids;
	jobjectArray array;
	int count = 0;

	/* no camera2 ops at all: null, and CameraManager turns that into a
	 * CameraAccessException rather than an empty list */
	if (!backend || !backend->get_camera2_id_list)
		return NULL;

	ids = backend->get_camera2_id_list(&count);
	if (!ids)
		count = 0;

	array = (*env)->NewObjectArray(env, count, (*env)->FindClass(env, "java/lang/String"), NULL);
	for (int i = 0; i < count; i++) {
		jstring id = (*env)->NewStringUTF(env, ids[i]);

		(*env)->SetObjectArrayElement(env, array, i, id);
		(*env)->DeleteLocalRef(env, id);
	}
	return array;
}

/* an app reads the characteristics again and again; ATL_CAMERA_DUMP_METADATA
 * wants one dump per camera, not one per call */
static bool dump_wanted(const char *id)
{
	static char *dumped[16];
	static int n_dumped;
	int i;

	if (!getenv("ATL_CAMERA_DUMP_METADATA"))
		return false;
	for (i = 0; i < n_dumped; i++)
		if (!strcmp(dumped[i], id))
			return false;
	if (n_dumped < (int)(sizeof(dumped) / sizeof(dumped[0])))
		dumped[n_dumped++] = strdup(id);
	return true;
}

JNIEXPORT jlong JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1getStaticMetadata(JNIEnv *env, jclass class, jstring id_str)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();
	struct atl_camera_metadata *md;
	const char *id;

	if (!backend || !backend->get_static_metadata || !id_str)
		return 0;

	id = (*env)->GetStringUTFChars(env, id_str, NULL);
	md = backend->get_static_metadata(id);
	if (md && dump_wanted(id)) {
		char label[64];

		snprintf(label, sizeof(label), "camera '%s' characteristics", id);
		atl_camera_metadata_dump(md, label);
	}
	(*env)->ReleaseStringUTFChars(env, id_str, id);
	return _INTPTR(md);
}

JNIEXPORT jintArray JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1getAvailableKeys(JNIEnv *env, jclass class, jstring id_str, jint which)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();
	const uint32_t *keys;
	jintArray array;
	const char *id;
	int count = 0;

	if (!backend || !backend->get_available_keys || !id_str)
		return NULL;

	id = (*env)->GetStringUTFChars(env, id_str, NULL);
	keys = backend->get_available_keys(id, which, &count);
	(*env)->ReleaseStringUTFChars(env, id_str, id);
	if (!keys)
		return NULL;

	array = (*env)->NewIntArray(env, count);
	(*env)->SetIntArrayRegion(env, array, 0, count, (const jint *)keys);
	return array;
}

JNIEXPORT jlong JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1create(JNIEnv *env, jclass class)
{
	return _INTPTR(atl_camera_metadata_new());
}

JNIEXPORT jlong JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1copy(JNIEnv *env, jclass class, jlong ptr)
{
	return _INTPTR(atl_camera_metadata_copy(metadata_of(ptr)));
}

JNIEXPORT void JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1free(JNIEnv *env, jclass class, jlong ptr)
{
	atl_camera_metadata_free(metadata_of(ptr));
}

/*
 * The write side. A value arrives as the widest Java shape (long[] for anything
 * integral, double[] for anything fractional) and is narrowed here to the type
 * the tag is declared with, so the Java side never has to know the HAL types.
 */
static int write_type(jlong ptr, jint tag, int fallback)
{
	const struct atl_camera_metadata_entry *entry = entry_of(ptr, tag);
	int type;

	if (entry)
		return entry->type; /* keep an existing entry's type */
	type = atl_camera2_tag_type((uint32_t)tag);
	if (type < 0)
		type = atl_camera2_vendor_type((uint32_t)tag);
	return type >= 0 ? type : fallback;
}

JNIEXPORT void JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1writeLongs(JNIEnv *env, jclass class, jlong ptr,
                                                                                                  jint tag, jlongArray values)
{
	int count = values ? (*env)->GetArrayLength(env, values) : 0;
	int type = write_type(ptr, tag, ATL_CAMERA2_TYPE_INT32);
	jlong *in;
	void *out;

	if (!count)
		return;
	in = calloc(count, sizeof(jlong));
	(*env)->GetLongArrayRegion(env, values, 0, count, in);

	out = calloc(count, sizeof(int64_t));
	switch (type) {
	case ATL_CAMERA2_TYPE_BYTE:
		for (int i = 0; i < count; i++)
			((uint8_t *)out)[i] = (uint8_t)in[i];
		break;
	case ATL_CAMERA2_TYPE_INT64:
		for (int i = 0; i < count; i++)
			((int64_t *)out)[i] = in[i];
		break;
	case ATL_CAMERA2_TYPE_FLOAT:
		for (int i = 0; i < count; i++)
			((float *)out)[i] = (float)in[i];
		break;
	case ATL_CAMERA2_TYPE_DOUBLE:
		for (int i = 0; i < count; i++)
			((double *)out)[i] = (double)in[i];
		break;
	default: /* int32, and a rational's (numerator, denominator) pairs */
		for (int i = 0; i < count; i++)
			((int32_t *)out)[i] = (int32_t)in[i];
		break;
	}
	/* a rational entry counts pairs, not values */
	atl_camera_metadata_add(metadata_of(ptr), (uint32_t)tag, type, out,
	                        type == ATL_CAMERA2_TYPE_RATIONAL ? count / 2 : count);
	free(in);
	free(out);
}

JNIEXPORT void JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1writeDoubles(JNIEnv *env, jclass class, jlong ptr,
                                                                                                    jint tag, jdoubleArray values)
{
	int count = values ? (*env)->GetArrayLength(env, values) : 0;
	int type = write_type(ptr, tag, ATL_CAMERA2_TYPE_FLOAT);
	jdouble *in;
	void *out;

	if (!count)
		return;
	in = calloc(count, sizeof(jdouble));
	(*env)->GetDoubleArrayRegion(env, values, 0, count, in);

	out = calloc(count, sizeof(int64_t));
	switch (type) {
	case ATL_CAMERA2_TYPE_BYTE:
		for (int i = 0; i < count; i++)
			((uint8_t *)out)[i] = (uint8_t)in[i];
		break;
	case ATL_CAMERA2_TYPE_INT32:
	case ATL_CAMERA2_TYPE_RATIONAL:
		for (int i = 0; i < count; i++)
			((int32_t *)out)[i] = (int32_t)in[i];
		break;
	case ATL_CAMERA2_TYPE_INT64:
		for (int i = 0; i < count; i++)
			((int64_t *)out)[i] = (int64_t)in[i];
		break;
	case ATL_CAMERA2_TYPE_DOUBLE:
		for (int i = 0; i < count; i++)
			((double *)out)[i] = in[i];
		break;
	default:
		for (int i = 0; i < count; i++)
			((float *)out)[i] = (float)in[i];
		break;
	}
	atl_camera_metadata_add(metadata_of(ptr), (uint32_t)tag, type, out,
	                        type == ATL_CAMERA2_TYPE_RATIONAL ? count / 2 : count);
	free(in);
	free(out);
}

JNIEXPORT void JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1erase(JNIEnv *env, jclass class, jlong ptr, jint tag)
{
	atl_camera_metadata_remove(metadata_of(ptr), (uint32_t)tag);
}

JNIEXPORT void JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1writeBytes(JNIEnv *env, jclass class, jlong ptr,
                                                                                                  jint tag, jbyteArray values)
{
	int count = values ? (*env)->GetArrayLength(env, values) : 0;
	jbyte *in;

	if (!count)
		return;
	in = calloc(count, sizeof(jbyte));
	(*env)->GetByteArrayRegion(env, values, 0, count, in);
	atl_camera_metadata_add(metadata_of(ptr), (uint32_t)tag, ATL_CAMERA2_TYPE_BYTE, in, count);
	free(in);
}

JNIEXPORT jintArray JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1getTags(JNIEnv *env, jclass class, jlong ptr)
{
	struct atl_camera_metadata *md = metadata_of(ptr);
	int count = atl_camera_metadata_n_entries(md);
	jintArray array = (*env)->NewIntArray(env, count);

	for (int i = 0; i < count; i++) {
		jint tag = (jint)atl_camera_metadata_entry_at(md, i)->tag;

		(*env)->SetIntArrayRegion(env, array, i, 1, &tag);
	}
	return array;
}

JNIEXPORT jint JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1getTag(JNIEnv *env, jclass class, jlong ptr, jstring name_str)
{
	const char *name;
	uint32_t tag;

	if (!name_str)
		return (jint)ATL_CAMERA2_TAG_INVALID;

	name = (*env)->GetStringUTFChars(env, name_str, NULL);
	tag = atl_camera_metadata_tag_from_name(metadata_of(ptr), name);
	(*env)->ReleaseStringUTFChars(env, name_str, name);
	return (jint)tag;
}

JNIEXPORT jstring JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1getTagName(JNIEnv *env, jclass class, jlong ptr, jint tag)
{
	const char *name = atl_camera_metadata_tag_name(metadata_of(ptr), (uint32_t)tag);

	return name ? (*env)->NewStringUTF(env, name) : NULL;
}

JNIEXPORT jint JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1getType(JNIEnv *env, jclass class, jlong ptr, jint tag)
{
	const struct atl_camera_metadata_entry *entry = entry_of(ptr, tag);

	return entry ? entry->type : -1;
}

JNIEXPORT jbyteArray JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1readBytes(JNIEnv *env, jclass class, jlong ptr, jint tag)
{
	const struct atl_camera_metadata_entry *entry = entry_of(ptr, tag);
	jbyteArray array;

	if (!entry || entry->type != ATL_CAMERA2_TYPE_BYTE)
		return NULL;

	array = (*env)->NewByteArray(env, entry->count);
	(*env)->SetByteArrayRegion(env, array, 0, entry->count, (const jbyte *)entry->data);
	return array;
}

/* byte and rational entries read as ints too: HAL enums are bytes but Java
 * keys are ints, and a rational is its (numerator, denominator) pair */
JNIEXPORT jintArray JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1readInts(JNIEnv *env, jclass class, jlong ptr, jint tag)
{
	const struct atl_camera_metadata_entry *entry = entry_of(ptr, tag);
	jintArray array;
	jint *values;
	int count;

	if (!entry)
		return NULL;

	switch (entry->type) {
	case ATL_CAMERA2_TYPE_BYTE:
	case ATL_CAMERA2_TYPE_INT32:
		count = entry->count;
		break;
	case ATL_CAMERA2_TYPE_RATIONAL:
		count = entry->count * 2;
		break;
	default:
		return NULL;
	}

	array = (*env)->NewIntArray(env, count);
	if (entry->type == ATL_CAMERA2_TYPE_BYTE) {
		values = calloc(count ? count : 1, sizeof(jint));
		for (int i = 0; i < count; i++)
			values[i] = ((const uint8_t *)entry->data)[i];
		(*env)->SetIntArrayRegion(env, array, 0, count, values);
		free(values);
	} else {
		(*env)->SetIntArrayRegion(env, array, 0, count, (const jint *)entry->data);
	}
	return array;
}

JNIEXPORT jfloatArray JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1readFloats(JNIEnv *env, jclass class, jlong ptr, jint tag)
{
	const struct atl_camera_metadata_entry *entry = entry_of(ptr, tag);
	jfloatArray array;

	if (!entry || entry->type != ATL_CAMERA2_TYPE_FLOAT)
		return NULL;

	array = (*env)->NewFloatArray(env, entry->count);
	(*env)->SetFloatArrayRegion(env, array, 0, entry->count, (const jfloat *)entry->data);
	return array;
}

/* int32 entries widen, so a Range<Long> key works whichever width the HAL used */
JNIEXPORT jlongArray JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1readLongs(JNIEnv *env, jclass class, jlong ptr, jint tag)
{
	const struct atl_camera_metadata_entry *entry = entry_of(ptr, tag);
	jlongArray array;
	jlong *values;

	if (!entry)
		return NULL;
	if (entry->type == ATL_CAMERA2_TYPE_INT64) {
		array = (*env)->NewLongArray(env, entry->count);
		(*env)->SetLongArrayRegion(env, array, 0, entry->count, (const jlong *)entry->data);
		return array;
	}
	if (entry->type != ATL_CAMERA2_TYPE_INT32 && entry->type != ATL_CAMERA2_TYPE_BYTE)
		return NULL;

	values = calloc(entry->count ? entry->count : 1, sizeof(jlong));
	for (int i = 0; i < entry->count; i++)
		values[i] = entry->type == ATL_CAMERA2_TYPE_INT32 ? ((const int32_t *)entry->data)[i]
		                                                  : ((const uint8_t *)entry->data)[i];
	array = (*env)->NewLongArray(env, entry->count);
	(*env)->SetLongArrayRegion(env, array, 0, entry->count, values);
	free(values);
	return array;
}

JNIEXPORT jdoubleArray JNICALL Java_android_hardware_camera2_impl_CameraMetadataNative_native_1readDoubles(JNIEnv *env, jclass class, jlong ptr, jint tag)
{
	const struct atl_camera_metadata_entry *entry = entry_of(ptr, tag);
	jdoubleArray array;

	if (!entry || entry->type != ATL_CAMERA2_TYPE_DOUBLE)
		return NULL;

	array = (*env)->NewDoubleArray(env, entry->count);
	(*env)->SetDoubleArrayRegion(env, array, 0, entry->count, (const jdouble *)entry->data);
	return array;
}
