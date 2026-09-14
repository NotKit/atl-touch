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

/* Nearest neighbour: an ImageReader smaller than the stream gets its own size
 * without a second pipeline. Chroma is sampled per VU pair, so the interleave
 * survives. */
void atl_camera_nv21_scale(uint8_t *dst, int dst_width, int dst_height, const uint8_t *nv21,
                           int src_width, int src_height, int src_stride)
{
	const uint8_t *src_vu = nv21 + (size_t)src_stride * src_height;
	uint8_t *dst_vu = dst + (size_t)dst_width * dst_height;

	if (dst_width == src_width && dst_height == src_height) {
		atl_camera_nv21_pack(dst, nv21, src_width, src_height, src_stride);
		return;
	}

	for (int row = 0; row < dst_height; row++) {
		const uint8_t *src_row = nv21 + (size_t)(row * src_height / dst_height) * src_stride;
		uint8_t *out = dst + (size_t)row * dst_width;

		for (int col = 0; col < dst_width; col++)
			out[col] = src_row[col * src_width / dst_width];
	}
	for (int row = 0; row < dst_height / 2; row++) {
		const uint8_t *src_row = src_vu + (size_t)(row * src_height / dst_height) * src_stride;
		uint8_t *out = dst_vu + (size_t)row * dst_width;

		for (int col = 0; col < dst_width / 2; col++) {
			/* the pair index scales like a half-width column, and each
			 * pair is two bytes into the interleaved row */
			int src_col = (col * src_width / dst_width) * 2;

			out[col * 2] = src_row[src_col];         /* V */
			out[col * 2 + 1] = src_row[src_col + 1]; /* U */
		}
	}
}

int atl_camera_yuv420_step(int width, int height, int need_width, int need_height)
{
	int step;

	if (need_width < 1 || need_height < 1)
		return 1;

	step = MIN(width / need_width, height / need_height);
	/* rounding the result down to even can take it back under what was asked
	 * for, so the step that survives is the one whose frame still fits */
	while (step > 1) {
		int out_width, out_height;

		atl_camera_yuv420_size(width, height, step, &out_width, &out_height);
		if (out_width >= need_width && out_height >= need_height)
			break;
		step--;
	}
	return step < 1 ? 1 : step;
}

void atl_camera_yuv420_size(int width, int height, int step, int *out_width, int *out_height)
{
	if (step < 1)
		step = 1;
	*out_width = MAX((width / step) & ~1, 2);
	*out_height = MAX((height / step) & ~1, 2);
}

void atl_camera_yuv420_to_nv21(uint8_t *dst, int out_width, int out_height,
                               const struct atl_camera_yuv420 *src, int step)
{
	/* the V plane is already the NV21 chroma plane when the HAL hands out
	 * semi-planar chroma with U one byte into it, which is the usual case */
	bool interleaved = src->u_pixel == 2 && src->v_pixel == 2 && src->u == src->v + 1 &&
	                   src->u_stride == src->v_stride;
	uint8_t *chroma = dst + (size_t)out_width * out_height;

	if (step < 1)
		step = 1;

	for (int row = 0; row < out_height; row++) {
		const uint8_t *y = src->y + (size_t)row * step * src->y_stride;
		uint8_t *out = dst + (size_t)row * out_width;

		if (step == 1) {
			memcpy(out, y, (size_t)out_width);
			continue;
		}
		for (int col = 0; col < out_width; col++)
			out[col] = y[(size_t)col * step];
	}

	/* a chroma row covers two luma rows, so it steps at the same rate as the
	 * luma rows do - and so does a VU pair against two luma columns */
	for (int row = 0; row < out_height / 2; row++) {
		uint8_t *out = chroma + (size_t)row * out_width;

		if (interleaved && step == 1) {
			size_t at = (size_t)row * src->v_stride;
			size_t left = (size_t)src->v_len > at ? (size_t)src->v_len - at : 0;

			memcpy(out, src->v + at, MIN(left, (size_t)out_width));
			continue;
		}
		for (int col = 0; col < out_width / 2; col++) {
			size_t v_at = (size_t)row * step * src->v_stride + (size_t)col * step * src->v_pixel;
			size_t u_at = (size_t)row * step * src->u_stride + (size_t)col * step * src->u_pixel;

			out[col * 2] = v_at < (size_t)src->v_len ? src->v[v_at] : 128;
			out[col * 2 + 1] = u_at < (size_t)src->u_len ? src->u[u_at] : 128;
		}
	}
}

/* Clockwise, because that is the direction CaptureRequest.JPEG_ORIENTATION
 * counts in: the picture has to turn by that much to come out upright. */
void atl_camera_rgba_rotate(uint8_t *dst, const uint8_t *rgba, int width, int height, int degrees)
{
	const uint32_t *src = (const uint32_t *)rgba;
	uint32_t *out = (uint32_t *)dst;

	switch (((degrees % 360) + 360) % 360) {
	case 90:
		for (int row = 0; row < height; row++)
			for (int col = 0; col < width; col++)
				out[(size_t)col * height + (height - 1 - row)] = src[(size_t)row * width + col];
		break;
	case 180:
		for (int row = 0; row < height; row++)
			for (int col = 0; col < width; col++)
				out[(size_t)(height - 1 - row) * width + (width - 1 - col)] =
				    src[(size_t)row * width + col];
		break;
	case 270:
		for (int row = 0; row < height; row++)
			for (int col = 0; col < width; col++)
				out[(size_t)(width - 1 - col) * height + row] = src[(size_t)row * width + col];
		break;
	default:
		memcpy(dst, rgba, (size_t)width * height * 4);
		break;
	}
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
