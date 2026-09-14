/*
 * libhybris' android loader, for the forwarder libraries. See hybris_ndk.h.
 *
 * There is no build-time libhybris dependency here, exactly as in
 * camera_backend_camera2ndk.c: on a desktop nothing exports android_dlopen, the
 * lookups fail once with a log line and every forwarded call returns its
 * "unavailable" answer.
 */

#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hybris_ndk.h"

#define MAX_LIBS 16

static void *(*android_dlopen_fn)(const char *, int);
static void *(*android_dlsym_fn)(void *, const char *);
static const char *(*android_dlerror_fn)(void);

static struct {
	const char *soname;
	void *handle;
	bool tried;
} libs[MAX_LIBS];

static const char *hybris_error(void)
{
	const char *error = android_dlerror_fn ? android_dlerror_fn() : NULL;

	return error ? error : "no error reported";
}

/*
 * On a UT device libhybris-common is already in the process (the session
 * preloads it), so a plain symbol lookup finds it; dlopen'ing it here would
 * fail on its initial-exec TLS anyway.
 */
static bool loader_load(void)
{
	static bool tried;
	static bool ok;

	if (tried)
		return ok;
	tried = true;

	android_dlopen_fn = dlsym(RTLD_DEFAULT, "android_dlopen");
	android_dlsym_fn = dlsym(RTLD_DEFAULT, "android_dlsym");
	android_dlerror_fn = dlsym(RTLD_DEFAULT, "android_dlerror");
	if (!android_dlopen_fn || !android_dlsym_fn) {
		fprintf(stderr, "atl-ndk: no libhybris android loader in this process, "
		                "the Android NDK libraries cannot be reached\n");
		return false;
	}
	ok = true;
	return true;
}

static void *lib_open(const char *soname)
{
	int free_slot = -1;

	for (int i = 0; i < MAX_LIBS; i++) {
		if (libs[i].soname && !strcmp(libs[i].soname, soname))
			return libs[i].handle;
		if (!libs[i].soname && free_slot < 0)
			free_slot = i;
	}
	if (!loader_load() || free_slot < 0)
		return NULL;

	/* android_dlopen() is refcounted by soname inside libhybris, so this is the
	 * same library object ATL's own camera2ndk backend holds */
	void *handle = android_dlopen_fn(soname, RTLD_LAZY);

	if (!handle)
		fprintf(stderr, "atl-ndk: android_dlopen(%s) failed: %s\n", soname, hybris_error());
	else
		fprintf(stderr, "atl-ndk: %s forwarded to the Android %s\n", soname, soname);

	libs[free_slot].soname = soname;
	libs[free_slot].handle = handle;
	return handle;
}

void *atl_hybris_ndk_sym(const char *soname, const char *symbol)
{
	void *handle = lib_open(soname);
	void *sym = handle ? android_dlsym_fn(handle, symbol) : NULL;

	if (!sym)
		fprintf(stderr, "atl-ndk: %s has no %s\n", soname, symbol);
	return sym;
}

void atl_hybris_ndk_binder_pool(void)
{
	static bool done;

	if (done)
		return;
	done = true;

	void (*set_max)(uint32_t) = atl_hybris_ndk_sym("libbinder_ndk.so",
	                                               "ABinderProcess_setThreadPoolMaxThreadCount");
	void (*start)(void) = atl_hybris_ndk_sym("libbinder_ndk.so", "ABinderProcess_startThreadPool");

	if (!start)
		return;
	if (set_max)
		set_max(4);
	start();
	fprintf(stderr, "atl-ndk: binder thread pool started\n");
}
