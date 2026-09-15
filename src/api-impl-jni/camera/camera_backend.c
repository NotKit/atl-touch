#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera_backend.h"

static const struct atl_camera_backend *backend_pick(void);
static const struct atl_camera_backend *resolved_backend;

/* read on the app's GL thread, written when a camera arms its fast path */
static volatile bool external_textures;

bool atl_camera_external_textures(void)
{
	return external_textures;
}

void atl_camera_set_external_textures(bool available)
{
	external_textures = available;
}

/*
 * Picking a backend dlopens the device's camera libraries, which takes long
 * enough that an app enumerating cameras from a second thread lands in the
 * middle of it. Resolving under a pthread_once makes that second caller wait
 * for the answer instead of being told there is no backend at all, which an
 * app takes as "this device has no cameras" and never asks again.
 */
static void resolve_backend(void)
{
	resolved_backend = backend_pick();
}

const struct atl_camera_backend *atl_camera_backend_get(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;

	pthread_once(&once, resolve_backend);
	return resolved_backend;
}

static const struct atl_camera_backend *backend_pick(void)
{
	const struct atl_camera_backend *backend = NULL;

	if (!getenv("ATL_UGLY_ENABLE_CAMERA")) {
		fprintf(stderr, "Camera: disabled (set ATL_UGLY_ENABLE_CAMERA=1 to enable)\n");
		return NULL;
	}

	const char *name = getenv("ATL_CAMERA_BACKEND");
	if (name && !strcmp(name, "none")) {
		fprintf(stderr, "Camera: backend 'none' selected, no cameras\n");
		return NULL;
	}
	if (name && !strcmp(name, "hybris")) {
		backend = atl_camera_backend_hybris_get();
		if (!backend)
			fprintf(stderr, "Camera: backend 'hybris' requested but the libhybris camera "
			                "compat layer is unavailable, no cameras\n");
		else
			fprintf(stderr, "Camera: using backend '%s'\n", backend->name);
		return backend;
	}
	/* an explicitly requested backend is never quietly replaced by another one:
	 * a camera2 app on a device would then look like a working ATL with the
	 * wrong cameras */
	if (name && !strcmp(name, "camera2ndk")) {
		backend = atl_camera_backend_camera2ndk_get();
		if (!backend)
			fprintf(stderr, "Camera: backend 'camera2ndk' requested but the Android camera2 "
			                "NDK libraries could not be loaded, no cameras\n");
		else
			fprintf(stderr, "Camera: using backend '%s'\n", backend->name);
		return backend;
	}
	if (name && !strcmp(name, "replay")) {
		backend = atl_camera_backend_replay_get();
		if (!backend)
			fprintf(stderr, "Camera: backend 'replay' requested but no recording could be "
			                "read, no cameras\n");
		else
			fprintf(stderr, "Camera: using backend '%s'\n", backend->name);
		return backend;
	}
	if (name && strcmp(name, "gst")) {
		fprintf(stderr, "Camera: unknown backend '%s' (gst, camera2ndk, hybris, replay, none), "
		                "no cameras\n", name);
		return NULL;
	}
	/* auto: the device backends in order of what they can serve (camera2ndk
	 * covers Camera1 as well, hybris does not do camera2), else gst */
	if (!name) {
		backend = atl_camera_backend_camera2ndk_get();
		if (!backend)
			backend = atl_camera_backend_hybris_get();
		if (backend) {
			fprintf(stderr, "Camera: using backend '%s'\n", backend->name);
			return backend;
		}
	}
	backend = atl_camera_backend_gst_get();
	if (backend)
		fprintf(stderr, "Camera: using backend '%s'\n", backend->name);
	else
		fprintf(stderr, "Camera: no backend available, no cameras\n");
	return backend;
}
