/* Pixel helpers shared by the camera backends and the preview path. */

#include <stddef.h>

#include "camera_frame.h"

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
