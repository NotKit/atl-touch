#ifndef ATL_HARDWARE_BUFFER_H
#define ATL_HARDWARE_BUFFER_H

#include <jni.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* the frame description libandroid presents with: one definition on both sides
 * of the name lookup src/libandroid/surface_control.c does */
#include "../../libandroid/window_frame.h"

/*
 * The native side of android.hardware.HardwareBuffer.
 *
 * ATL has no gralloc: a buffer is either a plain allocation of its own or a
 * window onto memory somebody else owns (an Image's pixels). The owner
 * detaches it with atl_hardware_buffer_detach() when that memory goes away, so
 * a HardwareBuffer the app kept can be asked about its geometry but never
 * points at a recycled buffer.
 */

struct atl_hardware_buffer;

/* HardwareBuffer.create(): a zeroed allocation this buffer owns */
struct atl_hardware_buffer *atl_hardware_buffer_alloc(int width, int height, int format,
                                                      int layers, uint64_t usage);
/* a window onto memory the caller owns, with one reference for the caller */
struct atl_hardware_buffer *atl_hardware_buffer_wrap(uint8_t *data, size_t size, int width,
                                                     int height, int format, uint64_t usage);
/*
 * A buffer around a gralloc AHardwareBuffer somebody else allocated - a camera
 * HAL's, out of an Image - with a reference of its own on it. It has no CPU
 * copy: presenting it maps the gralloc buffer - or reads the planes in
 * `mapped`, when the owner has the buffer locked for the CPU already and can
 * say where they are - and the handle the app's native code sees is the real
 * one. NULL where there is no gralloc.
 */
struct atl_hardware_buffer *atl_hardware_buffer_wrap_native(void *ahardwarebuffer, int width,
                                                            int height, int format,
                                                            uint64_t usage,
                                                            const struct atl_window_frame *mapped);
void atl_hardware_buffer_detach(struct atl_hardware_buffer *buffer);
void atl_hardware_buffer_ref(struct atl_hardware_buffer *buffer);
void atl_hardware_buffer_unref(struct atl_hardware_buffer *buffer);

/* a new android.hardware.HardwareBuffer for it, taking a reference; NULL (with
 * no pending exception) when the class is not there */
jobject atl_hardware_buffer_to_java(JNIEnv *env, struct atl_hardware_buffer *buffer);

/*
 * The real gralloc AHardwareBuffer behind this one, for an app's native code
 * that means to bind it as an EGLImage or hand it to the compositor: allocated
 * from the device's own libnativewindow on first ask, filled once from the CPU
 * copy, and owned by this buffer. NULL where there is no gralloc at all (any
 * desktop), which is the same answer ATL gave before it could allocate one.
 *
 * The same handle comes back every time, so an app that renders into it and
 * then presents it is talking about one buffer throughout. Exported for
 * src/hybris-ndk/nativewindow_forward.c, which resolves it by name.
 */
void *atl_hardware_buffer_gralloc(struct atl_hardware_buffer *buffer);

/*
 * The AHardwareBuffer to hand an app's native code: the gralloc handle where
 * there is one, and ATL's own buffer where there is no gralloc at all, which
 * is opaque to the app and keeps ATL's own consumers working on a desktop.
 * Never a plain allocation on a device - a handle the device's libnativewindow
 * may be given has to be one it allocated.
 */
void *atl_hardware_buffer_native(struct atl_hardware_buffer *buffer);

/*
 * The pixels behind such a handle, for a surface transaction presenting it:
 * the CPU copy where ATL owns them, and the gralloc mapping where the app may
 * have rendered into the buffer since. A YCbCr buffer comes back as its three
 * planes, which only AHardwareBuffer_lockPlanes can describe. False when the
 * handle is not one of ours or ATL cannot read its format. Paired with the
 * unmap.
 */
bool atl_hardware_buffer_map(void *handle, struct atl_window_frame *frame);
void atl_hardware_buffer_unmap(void *handle);

#endif
