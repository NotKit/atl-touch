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
	/* TODO: "hybris" backend (US-009) */
	if (name && !strcmp(name, "hybris")) {
		fprintf(stderr, "Camera: backend 'hybris' not implemented yet, no cameras\n");
		return NULL;
	}
	if (name && strcmp(name, "gst")) {
		fprintf(stderr, "Camera: unknown backend '%s' (gst, hybris, none), no cameras\n", name);
		return NULL;
	}
	/* explicit "gst" or auto (hybris would take precedence once it exists) */
	backend = atl_camera_backend_gst_get();
	if (backend)
		fprintf(stderr, "Camera: using backend '%s'\n", backend->name);
	else
		fprintf(stderr, "Camera: no backend available, no cameras\n");
	return backend;
}
