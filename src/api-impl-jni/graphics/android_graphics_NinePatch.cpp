#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"
#include "ATLCanvas.h"
#include "AndroidPaint.h"
#include "NinePatchChunk.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"

extern "C" {
#include "../generated_headers/android_graphics_Canvas.h"
#include "../generated_headers/android_graphics_NinePatch.h"
}

/*
 * android.graphics.NinePatch natives: the jlong chunk handle is a self-
 * contained, host-endian Res_png_9patch* (divs/colors arrays inline after
 * the struct), as produced by validateNinePatchChunk().
 */

enum {
	// The 9 patch segment is not a solid color.
	NO_COLOR = 0x00000001,
	// The 9 patch segment is completely transparent.
	TRANSPARENT_COLOR = 0x00000000
};

/* Validate the chunk bytes and return a malloc'd host-endian copy, or NULL.
 * Handles both the serialized form (network order, offsets relative to the
 * struct) and the already-deserialized form. */
struct Res_png_9patch *atl_ninepatch_chunk_parse(const uint8_t *data, uint32_t size)
{
	if (size < sizeof(struct Res_png_9patch) || size > 1024 * 1024)
		return NULL;

	struct Res_png_9patch *chunk = (struct Res_png_9patch *)malloc(size);
	if (!chunk)
		return NULL;
	memcpy(chunk, data, size);

	/* aapt serializes the struct fields (offsets, padding) in host order;
	 * only the div and color arrays are written in network order */
	if (chunk->wasDeserialized) {
		chunk->xDivsOffset = sizeof(struct Res_png_9patch);
		chunk->yDivsOffset = chunk->xDivsOffset + chunk->numXDivs * sizeof(int32_t);
		chunk->colorsOffset = chunk->yDivsOffset + chunk->numYDivs * sizeof(int32_t);
	}

	// verify that all arrays are fully inside the chunk bounds
	if (chunk->xDivsOffset > size || chunk->yDivsOffset > size || chunk->colorsOffset > size
	    || chunk->numXDivs > (size - chunk->xDivsOffset) / sizeof(int32_t)
	    || chunk->numYDivs > (size - chunk->yDivsOffset) / sizeof(int32_t)
	    || chunk->numColors > (size - chunk->colorsOffset) / sizeof(int32_t)) {
		free(chunk);
		return NULL;
	}

	int32_t *xDivs = (int32_t *)((char *)chunk + chunk->xDivsOffset);
	int32_t *yDivs = (int32_t *)((char *)chunk + chunk->yDivsOffset);
	int32_t *colors = (int32_t *)((char *)chunk + chunk->colorsOffset);

	if (!chunk->wasDeserialized) {
		for (int i = 0; i < chunk->numXDivs; i++)
			xDivs[i] = ntohl(xDivs[i]);
		for (int i = 0; i < chunk->numYDivs; i++)
			yDivs[i] = ntohl(yDivs[i]);
		for (int i = 0; i < chunk->numColors; i++)
			colors[i] = ntohl(colors[i]);
		chunk->wasDeserialized = 1;
	}

	return chunk;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_NinePatch_isNinePatchChunk(JNIEnv *env, jclass, jbyteArray chunk_array)
{
	if (!chunk_array)
		return JNI_FALSE;
	if ((uint32_t)env->GetArrayLength(chunk_array) < sizeof(struct Res_png_9patch))
		return JNI_FALSE;
	jbyte first;
	env->GetByteArrayRegion(chunk_array, 0, 1, &first);
	return first != -1 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL Java_android_graphics_NinePatch_validateNinePatchChunk(JNIEnv *env, jclass, jbyteArray chunk_array)
{
	uint32_t size = env->GetArrayLength(chunk_array);
	jbyte *data = env->GetByteArrayElements(chunk_array, NULL);
	struct Res_png_9patch *chunk = atl_ninepatch_chunk_parse((const uint8_t *)data, size);
	env->ReleaseByteArrayElements(chunk_array, data, JNI_ABORT);
	if (!chunk) {
		jclass exception = env->FindClass("java/lang/RuntimeException");
		env->ThrowNew(exception, "invalid nine-patch chunk");
		return 0;
	}
	return _INTPTR(chunk);
}

JNIEXPORT void JNICALL Java_android_graphics_NinePatch_nativeFinalize(JNIEnv *env, jclass, jlong chunk_ptr)
{
	free(_PTR(chunk_ptr));
}

JNIEXPORT void JNICALL Java_android_graphics_NinePatch_nativeGetPadding(JNIEnv *env, jclass, jlong chunk_ptr, jobject rect)
{
	struct Res_png_9patch *chunk = (struct Res_png_9patch *)_PTR(chunk_ptr);
	jclass rect_class = env->GetObjectClass(rect);
	env->SetIntField(rect, env->GetFieldID(rect_class, "left", "I"), chunk->paddingLeft);
	env->SetIntField(rect, env->GetFieldID(rect_class, "top", "I"), chunk->paddingTop);
	env->SetIntField(rect, env->GetFieldID(rect_class, "right", "I"), chunk->paddingRight);
	env->SetIntField(rect, env->GetFieldID(rect_class, "bottom", "I"), chunk->paddingBottom);
}

/* The PNG file could theoretically come from untrusted sources. So, our parsing code must be
 * safe against buffer overflows and other possible attacks. */
JNIEXPORT jbyteArray JNICALL Java_android_graphics_NinePatch_nativeReadChunkFromFile(JNIEnv *env, jclass, jstring path_jstr)
{
	uint8_t buf[8];
	jbyteArray result = NULL;

	const char *path = env->GetStringUTFChars(path_jstr, NULL);
	FILE *f = fopen(path, "rb");
	env->ReleaseStringUTFChars(path_jstr, path);

	if (!f)
		return NULL;
	// always check that we actually read all bytes, to avoid uninitialized data
	if (fread(buf, 1, 8, f) != 8) {
		fclose(f);
		return NULL;
	}
	if (buf[0] != 0x89 || buf[1] != 'P' || buf[2] != 'N' || buf[3] != 'G') {
		fclose(f);
		return NULL;
	}
	while (fread(buf, 1, 8, f) == 8) {
		uint32_t chunk_size = ntohl(((uint32_t *)buf)[0]);

		if (!strncmp((char *)&buf[4], "npTc", 4)) {
			// limit maximum size to 1MB to make potential unknown vulnerabilities harder to exploit
			if (chunk_size < sizeof(struct Res_png_9patch) || chunk_size > 1024 * 1024)
				break;
			uint8_t *data = (uint8_t *)malloc(chunk_size);
			if (!data)
				break;
			if (fread(data, 1, chunk_size, f) == chunk_size) {
				result = env->NewByteArray(chunk_size);
				env->SetByteArrayRegion(result, 0, chunk_size, (jbyte *)data);
			}
			free(data);
			break;
		} else {
			// skip chunk payload plus the 4-byte CRC
			fseek(f, chunk_size + 4, SEEK_CUR);
		}
	}
	fclose(f);

	return result;
}

/* Draw the nine patches of `bitmap`, as described by `chunk`, stretched over
 * the dst rect. Same layout logic the GSK NinePatchPaintable used, targeting
 * an SkCanvas. */
JNIEXPORT void JNICALL Java_android_graphics_Canvas_nDrawNinePatch(JNIEnv *env, jclass, jlong canvas_ptr, jlong bitmap_ptr, jlong chunk_ptr,
                                                                   jfloat dst_left, jfloat dst_top, jfloat dst_right, jfloat dst_bottom, jlong paint_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(canvas_ptr);
	SkBitmap *bitmap = (SkBitmap *)_PTR(bitmap_ptr);
	struct Res_png_9patch *chunk = (struct Res_png_9patch *)_PTR(chunk_ptr);
	if (!atl_canvas || !bitmap || !chunk)
		return;

	SkCanvas *canvas = atl_canvas->canvas;
	sk_sp<SkImage> image = atl_image_for_draw(atl_canvas, bitmap);
	if (!image)
		return;

	SkPaint paint;
	if (paint_ptr)
		paint = ((AndroidPaint *)_PTR(paint_ptr))->paint;
	SkSamplingOptions sampling(SkFilterMode::kLinear);

	const int width = bitmap->width();
	const int height = bitmap->height();
	const float dst_width = dst_right - dst_left;
	const float dst_height = dst_bottom - dst_top;

	int32_t *xDivs = (int32_t *)((char *)chunk + chunk->xDivsOffset);
	int32_t *yDivs = (int32_t *)((char *)chunk + chunk->yDivsOffset);
	int32_t *color = (int32_t *)((char *)chunk + chunk->colorsOffset);

	// total size of the stretchable (odd) sections along each axis
	int strechy_width = 0, strechy_height = 0;
	for (int i = 1; i < chunk->numXDivs + 1; i += 2)
		strechy_width += (i == chunk->numXDivs ? width : xDivs[i]) - xDivs[i - 1];
	for (int i = 1; i < chunk->numYDivs + 1; i += 2)
		strechy_height += (i == chunk->numYDivs ? height : yDivs[i]) - yDivs[i - 1];

	float strech_factor_width = strechy_width ? (dst_width - (width - strechy_width)) / strechy_width : 0;
	float strech_factor_height = strechy_height ? (dst_height - (height - strechy_height)) / strechy_height : 0;

	canvas->save();
	canvas->translate(dst_left, dst_top);

	float rect_y = 0, rect_height = 0;
	for (int j = 0; j < chunk->numYDivs + 1; j++, rect_y += rect_height) {
		int ydiv_start = j ? yDivs[j - 1] : 0;
		int ydiv_end = (j == chunk->numYDivs) ? height : yDivs[j];
		float actual_stretch_factor_height = (j % 2) ? strech_factor_height : 1;
		rect_height = (ydiv_end - ydiv_start) * actual_stretch_factor_height;
		if (!rect_height) // skip empty sections
			continue;
		float rect_x = 0, rect_width = 0;
		for (int i = 0; i < chunk->numXDivs + 1; i++, rect_x += rect_width) {
			int xdiv_start = i ? xDivs[i - 1] : 0;
			int xdiv_end = (i == chunk->numXDivs) ? width : xDivs[i];
			float actual_stretch_factor_width = (i % 2) ? strech_factor_width : 1;
			rect_width = (xdiv_end - xdiv_start) * actual_stretch_factor_width;
			if (!rect_width) // skip empty sections
				continue;
			SkRect rect = SkRect::MakeXYWH(rect_x, rect_y, rect_width, rect_height);
			if (*color == NO_COLOR) {
				SkRect texture_bounds = SkRect::MakeXYWH(rect_x - xdiv_start * actual_stretch_factor_width,
				                                         rect_y - ydiv_start * actual_stretch_factor_height,
				                                         width * actual_stretch_factor_width,
				                                         height * actual_stretch_factor_height);
				canvas->save();
				canvas->clipRect(rect);
				canvas->drawImageRect(image, texture_bounds, sampling, &paint);
				canvas->restore();
			} else if (*color != TRANSPARENT_COLOR) {
				SkPaint color_paint(paint);
				color_paint.setShader(nullptr);
				color_paint.setColor((SkColor)(uint32_t)*color);
				canvas->drawRect(rect, color_paint);
			}
			color++;
		}
	}

	canvas->restore();
}
