#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera_backend.h"

static const struct atl_camera_backend *backend_pick(void);
static const struct atl_camera_backend *resolved_backend;

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
	if (name && strcmp(name, "gst")) {
		fprintf(stderr, "Camera: unknown backend '%s' (gst, hybris, none), no cameras\n", name);
		return NULL;
	}
	/* auto: the device backend if its library is there, else gst */
	if (!name) {
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
