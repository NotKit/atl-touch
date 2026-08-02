/*
 * PNG writer for ATL_CAMERA_DUMP_FRAMES, using the Skia encoder already
 * linked for android.graphics.Bitmap. Plain-C entry point for the backends.
 */

#include <stdint.h>

#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkStream.h"
#include "include/encode/SkPngEncoder.h"

extern "C" bool atl_camera_write_png(const char *path, const uint8_t *rgba, int width, int height)
{
	SkFILEWStream stream(path);
	if (!stream.isValid())
		return false;
	SkPixmap pixmap(SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kOpaque_SkAlphaType),
	                rgba, (size_t)width * 4);
	return SkPngEncoder::Encode(&stream, pixmap, SkPngEncoder::Options());
}
