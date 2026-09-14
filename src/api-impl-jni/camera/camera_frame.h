#ifndef ATL_CAMERA_FRAME_H
#define ATL_CAMERA_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* copy a (possibly padded) NV21 frame into a contiguous width-strided buffer
 * of width*height*3/2 bytes */
void atl_camera_nv21_pack(uint8_t *dst, const uint8_t *nv21, int width, int height, int stride);

/* nearest-neighbour NV21 -> packed NV21 of a different size; a plain pack when
 * the sizes match. Both sizes must be even. */
void atl_camera_nv21_scale(uint8_t *dst, int dst_width, int dst_height, const uint8_t *nv21,
                           int src_width, int src_height, int src_stride);

/* One YUV_420_888 image the way the NDK describes it: three planes with their
 * strides, semi-planar (NV12/NV21) or fully planar (I420). The lengths are what
 * the reader reported, and a sample past one of them reads as neutral grey. */
struct atl_camera_yuv420 {
	const uint8_t *y, *u, *v;
	int y_stride, u_stride, v_stride;
	int u_pixel, v_pixel; /* bytes between chroma samples along a row */
	int y_len, u_len, v_len;
};

/* The largest whole-pixel step that still leaves a frame of at least
 * need_width x need_height. A consumer no bigger than the stream is the usual
 * case on a phone - a 640x480 viewfinder off a 4080x3072 sensor stream - and
 * stepping over the source is what keeps the repack off the other 39/40ths of
 * it. 1 when nothing is known about the consumers or nothing smaller fits. */
int atl_camera_yuv420_step(int width, int height, int need_width, int need_height);

/* the frame that step produces; both dimensions stay even, so the chroma plane
 * stays whole */
void atl_camera_yuv420_size(int width, int height, int step, int *out_width, int *out_height);

/* YUV_420_888 -> packed NV21 (Y plane, then interleaved VU), taking every
 * step'th pixel of every step'th row. dst holds out_width*out_height*3/2 bytes;
 * the two sizes are the ones atl_camera_yuv420_size() gave. */
void atl_camera_yuv420_to_nv21(uint8_t *dst, int out_width, int out_height,
                               const struct atl_camera_yuv420 *src, int step);

/* rotate an RGBA8888 image clockwise by 0, 90, 180 or 270 degrees; dst holds
 * width*height*4 bytes, with the two dimensions swapped for 90 and 270 */
void atl_camera_rgba_rotate(uint8_t *dst, const uint8_t *rgba, int width, int height, int degrees);

/* BT.601 video-range NV21 -> RGBA8888 (stride = Y-plane row stride) */
void atl_camera_nv21_to_rgba(const uint8_t *nv21, int width, int height, int stride, uint8_t *rgba);

/* ATL_CAMERA_DUMP_FRAMES: <dir>/frame-count every frame, <dir>/frame-%06d.png
 * every 30th (frames 1, 31, ...) — a backend's only view without a display */
void atl_camera_dump_frame(const char *dir, uint64_t count, const uint8_t *nv21,
                           int width, int height, int stride);

/* camera_frame_encode.cpp (Skia) */
bool atl_camera_write_png(const char *path, const uint8_t *rgba, int width, int height);
/* *out is a malloc'd JPEG buffer owned by the caller */
bool atl_camera_encode_jpeg(const uint8_t *rgba, int width, int height, int quality,
                            uint8_t **out, size_t *out_size);

#endif
