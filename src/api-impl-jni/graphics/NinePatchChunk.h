#pragma once

#include <stdint.h>

/*
 * Res_png_9patch: layout of the npTc chunk aapt embeds in compiled .9.png
 * files. aapt serializes the struct fields (offsets, padding) in host order;
 * only the div and color arrays are written in network order.
 */
struct Res_png_9patch {
	int8_t wasDeserialized;
	uint8_t numXDivs;
	uint8_t numYDivs;
	uint8_t numColors;
	// Offsets (from the start of this structure) to the xDivs, yDivs and
	// colors arrays. The serialized (aapt/PNG file) form places the arrays
	// immediately after the struct.
	uint32_t xDivsOffset;
	uint32_t yDivsOffset;
	int32_t paddingLeft, paddingRight;
	int32_t paddingTop, paddingBottom;
	uint32_t colorsOffset;
} __attribute__((packed));

/* Validate chunk bytes and return a malloc'd, host-endian, self-contained
 * copy (arrays deserialized, offsets fixed up), or NULL if invalid.
 * Implemented in android_graphics_NinePatch.cpp. */
struct Res_png_9patch *atl_ninepatch_chunk_parse(const uint8_t *data, uint32_t size);
