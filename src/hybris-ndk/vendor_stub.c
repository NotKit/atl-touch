/*
 * A vendor library an app dlopen()s at runtime, answered with a logged no-op.
 *
 * Google Camera's libgcastartup.so dlopen()s the Pixel's own libraries -
 * libgxp.so, libedgetpu_*.so, libOpenCL-pixel.so, lib_aion_buffer.so,
 * lib_jpg_encoder.so - for HDR+ and the TPU/GXP paths. They exist on the device
 * (in /android/vendor/lib64) but only the Android linker can load them, and the
 * app reaches its symbols with dlsym() on the handle it got back, which the
 * shim bionic linker answers from the host library it opened. Forwarding a
 * library whose symbol list is unknown is therefore not possible; what is
 * possible is to let the dlopen() succeed with an empty library, so the app
 * takes its "this device does not have it" path instead of dying on a NULL
 * handle it did not check.
 *
 * One of these is built per soname (see src/hybris-ndk/meson.build) so the log
 * line names the library that was asked for. HDR+ and the ML modules are out of
 * scope here; when one of them turns out to be needed, the answer is a
 * real forwarder for the handful of symbols the app looks up, not this.
 */

#include <stdio.h>

#ifndef ATL_VENDOR_SONAME
#define ATL_VENDOR_SONAME "unknown"
#endif

__attribute__((constructor)) static void announce(void)
{
	fprintf(stderr, "atl-vendor-stub: %s opened as an empty library; "
	                "every symbol an app looks up in it will be missing\n", ATL_VENDOR_SONAME);
}
