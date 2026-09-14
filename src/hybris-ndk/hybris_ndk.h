#ifndef ATL_HYBRIS_NDK_H
#define ATL_HYBRIS_NDK_H

#include <stdbool.h>

/*
 * The Android side of an app's own native libraries.
 *
 * A camera app like Google Camera links its native code against
 * libcamera2ndk.so, libbinder_ndk.so and libnativewindow.so. Those are Android
 * libraries: the shim bionic linker cannot load them (they want the whole
 * Android userspace behind them) and the host has nothing of those names, so
 * every symbol in them is an unresolved relocation and the app's library never
 * loads. ATL answers with small host libraries that carry the NDK's symbols and
 * forward each call into the real Android library through libhybris' loader -
 * the same android_dlopen() ATL's own camera2ndk backend uses, so the app and
 * ATL end up on one instance of libcamera2ndk.so, one ACameraManager and one
 * binder thread pool.
 *
 * bionic_translation finds them by soname, from
 * share/bionic_translation/cfg.d/atl-android-ndk.cfg.
 */

/* android_dlsym(android_dlopen(soname), symbol), with both cached; NULL (and
 * one log line per symbol) when this process has no libhybris or the Android
 * library does not have the symbol */
void *atl_hybris_ndk_sym(const char *soname, const char *symbol);

/*
 * The binder thread pool has to be running before any camera is opened: the HAL
 * calls back into this process to dequeue buffers, and with no thread to serve
 * those calls it times out on every frame. Called from the camera2ndk
 * forwarder's ACameraManager_create(), which is where an app that never touches
 * ATL's own backend would otherwise get it wrong. Idempotent.
 */
void atl_hybris_ndk_binder_pool(void);

/* forward a call to the same-named function in the real Android library */
#define ATL_HYBRIS_FORWARD(soname, ret, name, params, args, on_missing) \
	ret name params                                                     \
	{                                                                   \
		static ret(*real) params;                                       \
                                                                        \
		if (!real) {                                                    \
			real = atl_hybris_ndk_sym(soname, #name);                   \
			if (!real)                                                  \
				return on_missing;                                      \
		}                                                               \
		return real args;                                               \
	}

#define ATL_HYBRIS_FORWARD_VOID(soname, name, params, args) \
	void name params                                        \
	{                                                       \
		static void(*real) params;                          \
                                                            \
		if (!real) {                                        \
			real = atl_hybris_ndk_sym(soname, #name);       \
			if (!real)                                      \
				return;                                     \
		}                                                   \
		real args;                                          \
	}

#endif
