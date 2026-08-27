#ifndef BITMAP_H
#define BITMAP_H

#include <jni.h>
#include <stdint.h>

/* the NDK's AndroidBitmapFormat values. They are not a plain sequence, and
 * they have to be these numbers: Bitmap.Config stores the same ones for
 * AndroidBitmap_getInfo to report, and app code compares against the NDK's. */
enum {
	ANDROID_BITMAP_FORMAT_NONE = 0,
	ANDROID_BITMAP_FORMAT_RGBA_8888 = 1,
	ANDROID_BITMAP_FORMAT_RGB_565 = 4,
	ANDROID_BITMAP_FORMAT_RGBA_4444 = 7,
	ANDROID_BITMAP_FORMAT_A_8 = 8,
	ANDROID_BITMAP_FORMAT_RGBA_F16 = 9,
	ANDROID_BITMAP_FORMAT_RGBA_1010102 = 10,
};

struct AndroidBitmapInfo {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	int32_t format;
	uint32_t flags;
};

int AndroidBitmap_getInfo(JNIEnv *env, jobject bitmap,
                          struct AndroidBitmapInfo *info);
int AndroidBitmap_lockPixels(JNIEnv *env, jobject bitmap, void **pixels);
int AndroidBitmap_unlockPixels(JNIEnv *env, jobject bitmap);

#endif
