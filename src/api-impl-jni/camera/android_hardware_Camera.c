#include "../defines.h"

#include "camera_backend.h"

#include "../generated_headers/android_hardware_Camera.h"

JNIEXPORT jint JNICALL Java_android_hardware_Camera_native_1getNumberOfCameras(JNIEnv *env, jclass class)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();

	if (!backend)
		return 0;
	return backend->get_camera_count();
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1getCameraInfo(JNIEnv *env, jclass class, jint camera_id, jobject info)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();
	int facing = ATL_CAMERA_FACING_BACK;
	int orientation = 0;

	if (backend && backend->get_camera_info(camera_id, &facing, &orientation)) {
		_SET_INT_FIELD(info, "facing", facing);
		_SET_INT_FIELD(info, "orientation", orientation);
	}
}

JNIEXPORT jlong JNICALL Java_android_hardware_Camera_native_1open(JNIEnv *env, jobject this, jint camera_id)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();

	if (!backend)
		return 0;
	return _INTPTR(backend->open(camera_id));
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1release(JNIEnv *env, jobject this, jlong camera_ptr)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();

	if (backend && camera_ptr)
		backend->close(_PTR(camera_ptr));
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1startPreview(JNIEnv *env, jobject this, jlong camera_ptr)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();

	if (backend && camera_ptr)
		backend->start_preview(_PTR(camera_ptr));
}

JNIEXPORT void JNICALL Java_android_hardware_Camera_native_1stopPreview(JNIEnv *env, jobject this, jlong camera_ptr)
{
	const struct atl_camera_backend *backend = atl_camera_backend_get();

	if (backend && camera_ptr)
		backend->stop_preview(_PTR(camera_ptr));
}
