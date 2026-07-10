/*
 * Wrap a decoded RGBA video frame as an SkBitmap* consumable by
 * android.graphics.Bitmap.fromNative(long) — the handoff point between the
 * codec backend (plain C) and the Skia scene.
 */

#include <stdlib.h>

#include "include/core/SkBitmap.h"
#include "include/core/SkImageInfo.h"

extern "C" {

static void release_pixels(void *pixels, void *context)
{
	free(pixels);
}

/* takes ownership of pixels (malloc'd RGBA8888, stride = width * 4) */
void *atl_video_frame_wrap_skbitmap(uint8_t *pixels, int width, int height)
{
	SkBitmap *bitmap = new SkBitmap();
	if (!bitmap->installPixels(SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kOpaque_SkAlphaType),
	                           pixels, (size_t)width * 4, release_pixels, nullptr)) {
		free(pixels);
		delete bitmap;
		return nullptr;
	}
	return bitmap;
}

} // extern "C"
