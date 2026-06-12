#include <jni.h>
#include <stdio.h>

// FIXME: put the header in a common place
#include "../api-impl-jni/defines.h"
#include "bitmap.h"

#define ANDROID_BITMAP_RESULT_SUCCESS 0

int AndroidBitmap_getInfo(JNIEnv *env, jobject bitmap, struct AndroidBitmapInfo *info)
{
	info->width = _GET_INT_FIELD(bitmap, "width");
	info->height = _GET_INT_FIELD(bitmap, "height");
	info->stride = _GET_INT_FIELD(bitmap, "stride");
	info->format = _GET_INT_FIELD(_GET_OBJ_FIELD(bitmap, "config", "Landroid/graphics/Bitmap$Config;"), "android_memory_format");
	return ANDROID_BITMAP_RESULT_SUCCESS;
}

/* the bitmap is backed by an SkBitmap whose pixels can be accessed in place;
 * Bitmap.getPixelsPtr() also invalidates the cached GTK-side texture copy */
int AndroidBitmap_lockPixels(JNIEnv *env, jobject bitmap, void **pixels)
{
	*pixels = _PTR((*env)->CallLongMethod(env, bitmap, _METHOD(_CLASS(bitmap), "getPixelsPtr", "()J")));
	return ANDROID_BITMAP_RESULT_SUCCESS;
}

int AndroidBitmap_unlockPixels(JNIEnv *env, jobject bitmap)
{
	return ANDROID_BITMAP_RESULT_SUCCESS;
}
