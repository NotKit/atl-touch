/*
 * Shim, not a vendored header: NdkImageReader.h includes <cutils/native_handle.h>
 * for one function ATL never calls (AImageReader_getWindowNativeHandle). Only
 * the type has to exist for the NDK headers to parse on a glibc toolchain.
 */
#pragma once

#include <stdint.h>

typedef struct native_handle {
	int version;
	int numFds;
	int numInts;
	int data[0];
} native_handle_t;

typedef const native_handle_t *buffer_handle_t;
