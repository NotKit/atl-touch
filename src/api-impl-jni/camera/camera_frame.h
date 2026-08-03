#ifndef ATL_CAMERA_FRAME_H
#define ATL_CAMERA_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* copy a (possibly padded) NV21 frame into a contiguous width-strided buffer
 * of width*height*3/2 bytes */
void atl_camera_nv21_pack(uint8_t *dst, const uint8_t *nv21, int width, int height, int stride);

/* BT.601 video-range NV21 -> RGBA8888 (stride = Y-plane row stride) */
void atl_camera_nv21_to_rgba(const uint8_t *nv21, int width, int height, int stride, uint8_t *rgba);

/* camera_frame_encode.cpp (Skia) */
bool atl_camera_write_png(const char *path, const uint8_t *rgba, int width, int height);
/* *out is a malloc'd JPEG buffer owned by the caller */
bool atl_camera_encode_jpeg(const uint8_t *rgba, int width, int height, int quality,
                            uint8_t **out, size_t *out_size);

#endif
