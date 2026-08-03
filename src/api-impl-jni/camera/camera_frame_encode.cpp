/*
 * PNG (ATL_CAMERA_DUMP_FRAMES) and JPEG (takePicture) encoders, using the Skia
 * encoders already linked for android.graphics.Bitmap. Plain-C entry points
 * for the backends.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "include/core/SkData.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkStream.h"
#include "include/encode/SkJpegEncoder.h"
#include "include/encode/SkPngEncoder.h"

static SkPixmap rgba_pixmap(const uint8_t *rgba, int width, int height)
{
	return SkPixmap(SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kOpaque_SkAlphaType),
	                rgba, (size_t)width * 4);
}

extern "C" bool atl_camera_write_png(const char *path, const uint8_t *rgba, int width, int height)
{
	SkFILEWStream stream(path);
	if (!stream.isValid())
		return false;
	SkPixmap pixmap = rgba_pixmap(rgba, width, height);
	return SkPngEncoder::Encode(&stream, pixmap, SkPngEncoder::Options());
}

/* on success *out is a malloc'd JPEG buffer the caller frees */
extern "C" bool atl_camera_encode_jpeg(const uint8_t *rgba, int width, int height, int quality,
                                       uint8_t **out, size_t *out_size)
{
	SkPixmap pixmap = rgba_pixmap(rgba, width, height);
	SkJpegEncoder::Options options;
	options.fQuality = quality;

	SkDynamicMemoryWStream stream;
	if (!SkJpegEncoder::Encode(&stream, pixmap, options))
		return false;

	sk_sp<SkData> data = stream.detachAsData();
	uint8_t *jpeg = (uint8_t *)malloc(data->size());
	if (!jpeg)
		return false;
	memcpy(jpeg, data->data(), data->size());
	*out = jpeg;
	*out_size = data->size();
	return true;
}
