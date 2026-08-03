/* Pixel helpers shared by the camera backends and the preview path. */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "camera_frame.h"

void atl_camera_nv21_pack(uint8_t *dst, const uint8_t *nv21, int width, int height, int stride)
{
	if (stride == width) {
		memcpy(dst, nv21, (size_t)width * height * 3 / 2);
		return;
	}
	for (int row = 0; row < height; row++)
		memcpy(dst + (size_t)row * width, nv21 + (size_t)row * stride, width);
	for (int row = 0; row < height / 2; row++)
		memcpy(dst + (size_t)width * height + (size_t)row * width,
		       nv21 + (size_t)stride * height + (size_t)row * stride, width);
}

static inline uint8_t clamp_u8(int v)
{
	return v < 0 ? 0 : v > 255 ? 255 : v;
}

void atl_camera_nv21_to_rgba(const uint8_t *nv21, int width, int height, int stride, uint8_t *rgba)
{
	const uint8_t *y_plane = nv21;
	const uint8_t *vu_plane = nv21 + (size_t)stride * height;

	for (int row = 0; row < height; row++) {
		const uint8_t *y_row = y_plane + (size_t)row * stride;
		const uint8_t *vu_row = vu_plane + (size_t)(row / 2) * stride;
		uint8_t *out = rgba + (size_t)row * width * 4;
		for (int col = 0; col < width; col++) {
			int c = 298 * (y_row[col] - 16);
			int v = vu_row[(col & ~1)] - 128;
			int u = vu_row[(col & ~1) + 1] - 128;
			out[col * 4 + 0] = clamp_u8((c + 409 * v + 128) >> 8);
			out[col * 4 + 1] = clamp_u8((c - 100 * u - 208 * v + 128) >> 8);
			out[col * 4 + 2] = clamp_u8((c + 516 * u + 128) >> 8);
			out[col * 4 + 3] = 0xff;
		}
	}
}

void atl_camera_dump_frame(const char *dir, uint64_t count, const uint8_t *nv21,
                           int width, int height, int stride)
{
	char path[512];
	FILE *f;

	if (!dir)
		return;

	snprintf(path, sizeof(path), "%s/frame-count", dir);
	f = fopen(path, "w");
	if (f) {
		fprintf(f, "%" G_GUINT64_FORMAT "\n", count);
		fclose(f);
	}

	if (count % 30 != 1)
		return;

	uint8_t *rgba = malloc((size_t)width * height * 4);
	if (!rgba)
		return;
	atl_camera_nv21_to_rgba(nv21, width, height, stride, rgba);
	snprintf(path, sizeof(path), "%s/frame-%06" G_GUINT64_FORMAT ".png", dir, count);
	if (!atl_camera_write_png(path, rgba, width, height))
		fprintf(stderr, "Camera: failed to write %s\n", path);
	free(rgba);
}
