#ifndef ATL_WINDOW_FRAME_H
#define ATL_WINDOW_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The pixels an ANativeWindow presents, turned into the RGBA_8888 raster ATL
 * draws with. A producer writes whatever the window's format says (an app
 * through ANativeWindow_lock, a compositor buffer through a surface
 * transaction); the scene only knows one raster.
 *
 * Separate from native_window.c so it can be compiled on its own, which is
 * what tests/camera/window_frame_test.c does - the crop and transform
 * arithmetic is the part with corners in it.
 */

/* android.graphics.PixelFormat's, which AHardwareBuffer shares for the packed
 * raster formats */
#define ATL_WINDOW_FORMAT_RGBA_8888 1
#define ATL_WINDOW_FORMAT_RGBX_8888 2
#define ATL_WINDOW_FORMAT_RGB_888   3
#define ATL_WINDOW_FORMAT_RGB_565   4
/* not a raster at all: three planes, and what Google Camera presents in */
#define ATL_WINDOW_FORMAT_YCBCR_420_888 0x23

/* ANativeWindowTransform: a horizontal mirror, then a vertical one, then a
 * clockwise quarter turn, in that order */
#define ATL_WINDOW_TRANSFORM_MIRROR_HORIZONTAL 0x01
#define ATL_WINDOW_TRANSFORM_MIRROR_VERTICAL   0x02
#define ATL_WINDOW_TRANSFORM_ROTATE_90         0x04

/*
 * One plane of a YCbCr frame. The strides are what make the three 4:2:0
 * layouts one case: NV12 and NV21 point the two chroma planes into the same
 * interleaved run with a pixel stride of 2 (and differ only in which comes
 * first), I420 gives each its own with a pixel stride of 1.
 */
struct atl_window_plane {
	const uint8_t *data;
	size_t row_stride;
	size_t pixel_stride;
};

struct atl_window_frame {
	const uint8_t *pixels; /* the packed raster formats */
	int width;
	int height;
	size_t stride; /* bytes per row, >= width * bytes per pixel */
	int format;
	/* YCBCR_420_888 instead of pixels/stride: Y, Cb, Cr */
	struct atl_window_plane planes[3];
};

/* how many bytes a pixel of this format takes, 0 for one that is not a packed
 * raster (a plane set is not, and neither is a format ATL cannot read) */
size_t atl_window_format_bpp(int format);

/* whether a frame of this format can be presented at all */
bool atl_window_format_readable(int format);

/* fill in the three planes of an NV21 buffer - a Y plane followed by
 * interleaved Cr/Cb, which is what ATL's own frames are */
void atl_window_frame_nv21(struct atl_window_frame *frame, const uint8_t *data, int width,
                           int height, size_t row_stride);

/*
 * The size the frame comes out as: the crop's, with the sides swapped by a
 * quarter turn. crop is a left/top/right/bottom rectangle within the frame, or
 * NULL for the whole of it; an empty or out-of-bounds one is clamped, and a
 * crop that clamps away to nothing reports 0x0.
 */
void atl_window_frame_out_size(const struct atl_window_frame *frame, const int32_t *crop,
                               int transform, int *out_width, int *out_height);

/*
 * Convert into `out`, which holds out_width * out_height RGBA_8888 pixels as
 * reported by atl_window_frame_out_size(). False when there is nothing to
 * convert or the format is not one of the packed rasters above.
 */
bool atl_window_frame_to_rgba(const struct atl_window_frame *frame, const int32_t *crop,
                              int transform, uint8_t *out);

#endif
