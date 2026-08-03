#ifndef ATL_CAMERA_FRAME_H
#define ATL_CAMERA_FRAME_H

#include <stdbool.h>
#include <stdint.h>

/* copy a (possibly padded) NV21 frame into a contiguous width-strided buffer
 * of width*height*3/2 bytes */
void atl_camera_nv21_pack(uint8_t *dst, const uint8_t *nv21, int width, int height, int stride);

/* BT.601 video-range NV21 -> RGBA8888 (stride = Y-plane row stride) */
void atl_camera_nv21_to_rgba(const uint8_t *nv21, int width, int height, int stride, uint8_t *rgba);

/* camera_frame_png.cpp (Skia) */
bool atl_camera_write_png(const char *path, const uint8_t *rgba, int width, int height);

#endif
