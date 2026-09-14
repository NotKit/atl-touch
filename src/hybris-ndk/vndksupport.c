/*
 * libvndksupport.so: how an Android app asks for a vendor library.
 *
 * android_load_sphal_library() is Android's "load this out of the vendor
 * namespace" call, which is exactly what libhybris' android_dlopen() does on a
 * Halium port, so this one really does resolve from the Android side. An app
 * that goes through it - rather than plain dlopen() - gets the device's own
 * vendor library and its symbols.
 */

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>

static void *(*android_dlopen_fn)(const char *, int);
static int (*android_dlclose_fn)(void *);

static void loader_load(void)
{
	static bool tried;

	if (tried)
		return;
	tried = true;
	android_dlopen_fn = dlsym(RTLD_DEFAULT, "android_dlopen");
	android_dlclose_fn = dlsym(RTLD_DEFAULT, "android_dlclose");
	if (!android_dlopen_fn)
		fprintf(stderr, "atl-ndk: libvndksupport has no libhybris loader to hand a "
		                "vendor library to\n");
}

void *android_load_sphal_library(const char *name, int flag)
{
	loader_load();
	if (!android_dlopen_fn)
		return NULL;

	void *handle = android_dlopen_fn(name, flag);

	fprintf(stderr, "atl-ndk: android_load_sphal_library(%s) = %p\n", name, handle);
	return handle;
}

int android_unload_sphal_library(void *handle)
{
	loader_load();
	return android_dlclose_fn ? android_dlclose_fn(handle) : -1;
}
