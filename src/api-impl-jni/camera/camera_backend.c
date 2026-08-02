#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera_backend.h"

const struct atl_camera_backend *atl_camera_backend_get(void)
{
	static const struct atl_camera_backend *backend = NULL;
	static bool resolved = false;

	if (resolved)
		return backend;
	resolved = true;

	if (!getenv("ATL_UGLY_ENABLE_CAMERA")) {
		fprintf(stderr, "Camera: disabled (set ATL_UGLY_ENABLE_CAMERA=1 to enable)\n");
		return NULL;
	}

	const char *name = getenv("ATL_CAMERA_BACKEND");
	if (name && !strcmp(name, "none")) {
		fprintf(stderr, "Camera: backend 'none' selected, no cameras\n");
		return NULL;
	}
	/* TODO: "gst" (US-002) and "hybris" (US-009) backends */
	if (name && (!strcmp(name, "gst") || !strcmp(name, "hybris"))) {
		fprintf(stderr, "Camera: backend '%s' not implemented yet, no cameras\n", name);
		return NULL;
	}
	if (name) {
		fprintf(stderr, "Camera: unknown backend '%s' (gst, hybris, none), no cameras\n", name);
		return NULL;
	}
	fprintf(stderr, "Camera: no backend available, no cameras\n");
	return NULL;
}
