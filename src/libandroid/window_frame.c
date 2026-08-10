/*
 * The pixels an ANativeWindow presents, see window_frame.h.
 */

#include <string.h>

#include "window_frame.h"

size_t atl_window_format_bpp(int format)
{
	switch (format) {
	case ATL_WINDOW_FORMAT_RGBA_8888:
	case ATL_WINDOW_FORMAT_RGBX_8888:
		return 4;
	case ATL_WINDOW_FORMAT_RGB_888:
		return 3;
	case ATL_WINDOW_FORMAT_RGB_565:
		return 2;
	default:
		return 0;
	}
}

bool atl_window_format_readable(int format)
{
	return atl_window_format_bpp(format) || format == ATL_WINDOW_FORMAT_YCBCR_420_888;
}

void atl_window_frame_nv21(struct atl_window_frame *frame, const uint8_t *data, int width,
                           int height, size_t row_stride)
{
	const uint8_t *chroma = data + row_stride * height;

	frame->pixels = data;
	frame->width = width;
	frame->height = height;
	frame->stride = row_stride;
	frame->format = ATL_WINDOW_FORMAT_YCBCR_420_888;
	frame->planes[0] = (struct atl_window_plane){data, row_stride, 1};
	/* NV21 is Cr first, so Cb is the odd byte of each pair */
	frame->planes[1] = (struct atl_window_plane){chroma + 1, row_stride, 2};
	frame->planes[2] = (struct atl_window_plane){chroma, row_stride, 2};
}

/* the crop clamped into the frame; false when nothing is left of it */
static bool crop_clamp(const struct atl_window_frame *frame, const int32_t *crop,
                       int *left, int *top, int *width, int *height)
{
	int l = 0, t = 0, r = frame->width, b = frame->height;

	if (crop) {
		l = crop[0] > 0 ? crop[0] : 0;
		t = crop[1] > 0 ? crop[1] : 0;
		r = crop[2] < frame->width ? crop[2] : frame->width;
		b = crop[3] < frame->height ? crop[3] : frame->height;
	}
	if (l >= r || t >= b)
		return false;

	*left = l;
	*top = t;
	*width = r - l;
	*height = b - t;
	return true;
}

void atl_window_frame_out_size(const struct atl_window_frame *frame, const int32_t *crop,
                               int transform, int *out_width, int *out_height)
{
	int left, top, width, height;

	*out_width = *out_height = 0;
	if (!frame || frame->width <= 0 || frame->height <= 0)
		return;
	if (!crop_clamp(frame, crop, &left, &top, &width, &height))
		return;

	if (transform & ATL_WINDOW_TRANSFORM_ROTATE_90) {
		*out_width = height;
		*out_height = width;
	} else {
		*out_width = width;
		*out_height = height;
	}
}

static uint8_t clamp_u8(int value)
{
	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return (uint8_t)value;
}

/*
 * One YCbCr pixel, BT.601 with a video-range luma - the same arithmetic
 * camera_frame.c converts preview frames with, so the whole of ATL agrees on
 * what a camera's colours look like. The chroma planes are half resolution in
 * both directions.
 */
static void read_ycbcr(const struct atl_window_frame *frame, int x, int y, uint8_t *out)
{
	const struct atl_window_plane *luma = &frame->planes[0];
	const struct atl_window_plane *cb = &frame->planes[1];
	const struct atl_window_plane *cr = &frame->planes[2];
	size_t chroma_x = (size_t)(x / 2), chroma_y = (size_t)(y / 2);
	int c = 298 * (luma->data[(size_t)y * luma->row_stride + (size_t)x * luma->pixel_stride] - 16);
	int u = cb->data[chroma_y * cb->row_stride + chroma_x * cb->pixel_stride] - 128;
	int v = cr->data[chroma_y * cr->row_stride + chroma_x * cr->pixel_stride] - 128;

	out[0] = clamp_u8((c + 409 * v + 128) >> 8);
	out[1] = clamp_u8((c - 100 * u - 208 * v + 128) >> 8);
	out[2] = clamp_u8((c + 516 * u + 128) >> 8);
	out[3] = 0xff;
}

static void read_pixel(const uint8_t *src, int format, uint8_t *out)
{
	uint16_t rgb565;

	switch (format) {
	case ATL_WINDOW_FORMAT_RGBA_8888:
		memcpy(out, src, 4);
		return;
	case ATL_WINDOW_FORMAT_RGBX_8888:
		memcpy(out, src, 3);
		out[3] = 0xff;
		return;
	case ATL_WINDOW_FORMAT_RGB_888:
		memcpy(out, src, 3);
		out[3] = 0xff;
		return;
	case ATL_WINDOW_FORMAT_RGB_565:
		/* little-endian, as every Android raster is */
		rgb565 = (uint16_t)(src[0] | (src[1] << 8));
		/* replicate the high bits into the low ones so full-scale stays
		 * full-scale: 31 -> 255, not 248 */
		out[0] = (uint8_t)((rgb565 >> 8 & 0xf8) | (rgb565 >> 13 & 0x07));
		out[1] = (uint8_t)((rgb565 >> 3 & 0xfc) | (rgb565 >> 9 & 0x03));
		out[2] = (uint8_t)((rgb565 << 3 & 0xf8) | (rgb565 >> 2 & 0x07));
		out[3] = 0xff;
		return;
	default: /* unreachable: the bytes-per-pixel lookup rejected it */
		out[0] = out[1] = out[2] = 0;
		out[3] = 0xff;
		return;
	}
}

bool atl_window_frame_to_rgba(const struct atl_window_frame *frame, const int32_t *crop,
                              int transform, uint8_t *out)
{
	int left, top, width, height, out_width, out_height, x, y;
	size_t bpp;

	if (!frame || !out)
		return false;
	bpp = atl_window_format_bpp(frame->format);
	if (bpp) {
		if (!frame->pixels || frame->stride < (size_t)frame->width * bpp)
			return false;
	} else if (frame->format == ATL_WINDOW_FORMAT_YCBCR_420_888) {
		if (!frame->planes[0].data || !frame->planes[1].data || !frame->planes[2].data)
			return false;
	} else {
		return false;
	}
	if (!crop_clamp(frame, crop, &left, &top, &width, &height))
		return false;

	atl_window_frame_out_size(frame, crop, transform, &out_width, &out_height);

	/* the whole frame, already in the raster's own format and order: the
	 * common case, and the one a viewfinder runs at 30 frames a second */
	if (!transform && frame->format == ATL_WINDOW_FORMAT_RGBA_8888 &&
	    width == frame->width && height == frame->height && frame->stride == (size_t)width * 4) {
		memcpy(out, frame->pixels, (size_t)width * height * 4);
		return true;
	}

	for (y = 0; y < out_height; y++) {
		for (x = 0; x < out_width; x++) {
			int sx, sy;

			/* undo the quarter turn: it maps a source pixel to
			 * (height - 1 - sy, sx), so read it back the other way */
			if (transform & ATL_WINDOW_TRANSFORM_ROTATE_90) {
				sx = y;
				sy = height - 1 - x;
			} else {
				sx = x;
				sy = y;
			}
			if (transform & ATL_WINDOW_TRANSFORM_MIRROR_HORIZONTAL)
				sx = width - 1 - sx;
			if (transform & ATL_WINDOW_TRANSFORM_MIRROR_VERTICAL)
				sy = height - 1 - sy;

			if (bpp)
				read_pixel(frame->pixels + (size_t)(top + sy) * frame->stride +
				               (size_t)(left + sx) * bpp,
				           frame->format, out + ((size_t)y * out_width + x) * 4);
			else
				read_ycbcr(frame, left + sx, top + sy,
				           out + ((size_t)y * out_width + x) * 4);
		}
	}
	return true;
}
