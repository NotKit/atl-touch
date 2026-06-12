#include <algorithm>
#include <string.h>

#include "../defines.h"
#include "ATLCanvas.h"

#include "include/core/SkData.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkStream.h"
#include "include/encode/SkPngEncoder.h"

extern "C" {
#include "../generated_headers/android_graphics_Bitmap.h"
}

/*
 * Bitmap is implemented as an SkBitmap (the `texture` field) plus a lazily
 * created ATLCanvas drawing into it (the `snapshot` field). Unlike the
 * GdkTexture/GtkSnapshot model, both coexist: drawing mutates the pixels in
 * place, so no conversion between the two is ever needed.
 */

/* ANDROID_BITMAP_FORMAT_* values, as stored in Bitmap.Config */
static SkImageInfo image_info_for_format(int width, int height, int format)
{
	switch (format) {
		case 4: /* RGB_565 */
			return SkImageInfo::Make(width, height, kRGB_565_SkColorType, kOpaque_SkAlphaType);
		case 7: /* RGBA_4444 */
			return SkImageInfo::Make(width, height, kARGB_4444_SkColorType, kPremul_SkAlphaType);
		case 8: /* A_8 */
			return SkImageInfo::Make(width, height, kAlpha_8_SkColorType, kPremul_SkAlphaType);
		case 9: /* RGBA_F16 */
			return SkImageInfo::Make(width, height, kRGBA_F16_SkColorType, kPremul_SkAlphaType);
		case 1: /* RGBA_8888 */
		default:
			return SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
	}
}

/* android int pixels are 0xAARRGGBB words, i.e. BGRA bytes on little-endian, unpremultiplied */
static SkImageInfo java_pixels_info(int width, int height)
{
	return SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kUnpremul_SkAlphaType);
}

JNIEXPORT jlong JNICALL Java_android_graphics_Bitmap_native_1create_1bitmap(JNIEnv *env, jclass clazz, jint width, jint height, jint stride, jint format)
{
	SkBitmap *bitmap = new SkBitmap();
	SkImageInfo info = image_info_for_format(width, height, format);
	bitmap->allocPixels(info, std::max((size_t)stride, info.minRowBytes()));
	bitmap->eraseColor(SK_ColorTRANSPARENT);
	return _INTPTR(bitmap);
}

JNIEXPORT jlong JNICALL Java_android_graphics_Bitmap_native_1create_1canvas(JNIEnv *env, jclass clazz, jlong bitmap_ptr)
{
	return _INTPTR(ATLCanvas::for_bitmap((SkBitmap *)_PTR(bitmap_ptr)));
}

JNIEXPORT jlong JNICALL Java_android_graphics_Bitmap_native_1create_1gdk_1texture(JNIEnv *env, jclass clazz, jlong bitmap_ptr)
{
	SkBitmap *bitmap = (SkBitmap *)_PTR(bitmap_ptr);
	if (bitmap->colorType() == kRGBA_8888_SkColorType)
		return _INTPTR(atl_skbitmap_to_gdk_texture(bitmap));
	/* convert other formats to RGBA8888 first */
	SkBitmap converted;
	converted.allocPixels(SkImageInfo::Make(bitmap->width(), bitmap->height(), kRGBA_8888_SkColorType, kPremul_SkAlphaType));
	bitmap->readPixels(converted.pixmap());
	return _INTPTR(atl_skbitmap_to_gdk_texture(&converted));
}

JNIEXPORT void JNICALL Java_android_graphics_Bitmap_native_1unref_1gdk_1texture(JNIEnv *env, jclass clazz, jlong texture_ptr)
{
	g_object_unref(GDK_TEXTURE(_PTR(texture_ptr)));
}

JNIEXPORT jint JNICALL Java_android_graphics_Bitmap_native_1get_1width(JNIEnv *env, jclass clazz, jlong bitmap_ptr)
{
	return ((SkBitmap *)_PTR(bitmap_ptr))->width();
}

JNIEXPORT jint JNICALL Java_android_graphics_Bitmap_native_1get_1height(JNIEnv *env, jclass clazz, jlong bitmap_ptr)
{
	return ((SkBitmap *)_PTR(bitmap_ptr))->height();
}

JNIEXPORT void JNICALL Java_android_graphics_Bitmap_native_1erase_1color(JNIEnv *env, jclass clazz, jlong bitmap_ptr, jint color)
{
	((SkBitmap *)_PTR(bitmap_ptr))->eraseColor((SkColor)(uint32_t)color);
}

JNIEXPORT void JNICALL Java_android_graphics_Bitmap_native_1recycle(JNIEnv *env, jclass clazz, jlong bitmap_ptr, jlong canvas_ptr, jlong gdk_texture_ptr)
{
	if (canvas_ptr)
		delete (ATLCanvas *)_PTR(canvas_ptr);
	if (bitmap_ptr)
		delete (SkBitmap *)_PTR(bitmap_ptr);
	if (gdk_texture_ptr)
		g_object_unref(GDK_TEXTURE(_PTR(gdk_texture_ptr)));
}

JNIEXPORT jlong JNICALL Java_android_graphics_Bitmap_native_1copy_1bitmap(JNIEnv *env, jclass clazz, jlong bitmap_ptr)
{
	SkBitmap *src = (SkBitmap *)_PTR(bitmap_ptr);
	SkBitmap *copy = new SkBitmap();
	copy->allocPixels(src->info(), src->rowBytes());
	src->readPixels(copy->pixmap());
	return _INTPTR(copy);
}

JNIEXPORT void JNICALL Java_android_graphics_Bitmap_native_1get_1pixels(JNIEnv *env, jclass clazz, jlong bitmap_ptr, jintArray pixels, jint offset, jint stride, jint x, jint y, jint width, jint height)
{
	SkBitmap *bitmap = (SkBitmap *)_PTR(bitmap_ptr);
	jint *array = env->GetIntArrayElements(pixels, NULL);
	bitmap->readPixels(java_pixels_info(width, height), array + offset, stride * 4, x, y);
	env->ReleaseIntArrayElements(pixels, array, 0);
}

JNIEXPORT void JNICALL Java_android_graphics_Bitmap_native_1set_1pixels(JNIEnv *env, jclass clazz, jlong bitmap_ptr, jintArray pixels, jint offset, jint stride, jint x, jint y, jint width, jint height)
{
	SkBitmap *bitmap = (SkBitmap *)_PTR(bitmap_ptr);
	jint *array = env->GetIntArrayElements(pixels, NULL);
	SkPixmap pixmap(java_pixels_info(width, height), array + offset, stride * 4);
	bitmap->writePixels(pixmap, x, y);
	env->ReleaseIntArrayElements(pixels, array, 0);
}

JNIEXPORT void JNICALL Java_android_graphics_Bitmap_native_1copy_1to_1buffer(JNIEnv *env, jclass clazz, jlong bitmap_ptr, jobject buffer, jint format, jint stride)
{
	SkBitmap *bitmap = (SkBitmap *)_PTR(bitmap_ptr);
	void *data = env->GetDirectBufferAddress(buffer);
	if (data) {
		bitmap->readPixels(image_info_for_format(bitmap->width(), bitmap->height(), format), data, stride, 0, 0);
		return;
	}
	/* heap buffer: copy via the backing array */
	jclass buffer_class = env->GetObjectClass(buffer);
	jbyteArray array_ref = (jbyteArray)env->CallObjectMethod(buffer, env->GetMethodID(buffer_class, "array", "()Ljava/lang/Object;"));
	jint array_offset = env->CallIntMethod(buffer, env->GetMethodID(buffer_class, "arrayOffset", "()I"));
	jbyte *array = env->GetByteArrayElements(array_ref, NULL);
	bitmap->readPixels(image_info_for_format(bitmap->width(), bitmap->height(), format), array + array_offset, stride, 0, 0);
	env->ReleaseByteArrayElements(array_ref, array, 0);
}

JNIEXPORT jlong JNICALL Java_android_graphics_Bitmap_native_1get_1pixels_1ptr(JNIEnv *env, jclass clazz, jlong bitmap_ptr)
{
	return _INTPTR(((SkBitmap *)_PTR(bitmap_ptr))->getPixels());
}

JNIEXPORT jbyteArray JNICALL Java_android_graphics_Bitmap_native_1save_1to_1png(JNIEnv *env, jclass clazz, jlong bitmap_ptr)
{
	SkBitmap *bitmap = (SkBitmap *)_PTR(bitmap_ptr);
	SkPixmap pixmap;
	if (!bitmap->peekPixels(&pixmap))
		return NULL;
	SkDynamicMemoryWStream stream;
	if (!SkPngEncoder::Encode(&stream, pixmap, SkPngEncoder::Options()))
		return NULL;
	sk_sp<SkData> data = stream.detachAsData();
	jbyteArray result = env->NewByteArray(data->size());
	env->SetByteArrayRegion(result, 0, data->size(), (const jbyte *)data->data());
	return result;
}
