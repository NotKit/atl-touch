#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codec_backend.h"

void atl_video_frame_free(struct atl_video_frame *frame)
{
	if (!frame)
		return;
	free(frame->pixels);
	free(frame);
}

uint8_t *atl_video_frame_take_pixels(struct atl_video_frame *frame)
{
	uint8_t *pixels = frame->pixels;
	frame->pixels = NULL;
	free(frame);
	return pixels;
}

const struct atl_codec_backend *atl_codec_backend_get(void)
{
	static const struct atl_codec_backend *backend = NULL;

	if (!backend) {
		const char *name = getenv("ATL_MEDIA_CODEC_BACKEND");
		if (name && strcmp(name, atl_codec_backend_ffmpeg.name) != 0)
			fprintf(stderr, "MediaCodec: unknown backend '%s' (\"ndk\" for hybris devices is not "
			                "implemented yet), using '%s'\n",
			        name, atl_codec_backend_ffmpeg.name);
		backend = &atl_codec_backend_ffmpeg;
	}
	return backend;
}
