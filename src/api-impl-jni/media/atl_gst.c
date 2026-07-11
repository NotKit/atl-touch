#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "atl_gst.h"

bool atl_gst_ensure_init(void)
{
	static gsize initialized = 0;
	static bool ok = false;

	if (g_once_init_enter(&initialized)) {
		GError *error = NULL;
		ok = gst_init_check(NULL, NULL, &error);
		if (!ok) {
			fprintf(stderr, "atl_gst: gst_init_check failed: %s\n", error ? error->message : "(no detail)");
			g_clear_error(&error);
		}
		g_once_init_leave(&initialized, 1);
	}
	return ok;
}

GstElement *atl_gst_make_audio_sink(void)
{
	const char *desc = getenv("ATL_MEDIA_AUDIO_SINK");
	if (!desc || !*desc)
		return NULL;

	GError *error = NULL;
	GstElement *sink = gst_parse_bin_from_description(desc, TRUE, &error);
	if (!sink) {
		fprintf(stderr, "atl_gst: bad ATL_MEDIA_AUDIO_SINK '%s': %s\n", desc, error ? error->message : "");
		g_clear_error(&error);
	}
	return sink;
}
