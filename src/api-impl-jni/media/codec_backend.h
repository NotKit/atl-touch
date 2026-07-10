#ifndef ATL_CODEC_BACKEND_H
#define ATL_CODEC_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Pluggable codec backend behind android.media.MediaCodec.
 *
 * A backend provides decoder instances at the MediaCodec abstraction level:
 * compressed buffers in, PCM or video frames out, driven by the app's own
 * dequeue loop (no pipeline, no clock — that's the app's job with MediaCodec).
 *
 * Backends:
 *  - "ffmpeg" (codec_backend_ffmpeg.c): libavcodec, hardware-accelerated on
 *    mainline through hwdevice contexts (VAAPI/V4L2), software fallback.
 *  - "ndk" (TODO, separate task): Android hardware codecs on halium devices
 *    through libhybris + the device's libmediandk (AMediaCodec_*). Slot in by
 *    implementing atl_codec_backend and returning it from
 *    atl_codec_backend_get() when selected/detected.
 */

struct atl_codec; /* opaque per-decoder instance, owned by the backend */

/* dequeue result codes (>= 0 is success / a byte count for audio) */
#define ATL_CODEC_AGAIN -1
#define ATL_CODEC_EOS   -2
#define ATL_CODEC_ERROR -3

/* One decoded video frame in CPU memory, RGBA8888, tightly packed
 * (stride = width * 4). The receiver takes ownership: hand pixels to a
 * consumer with atl_video_frame_take_pixels() or discard everything with
 * atl_video_frame_free(). */
struct atl_video_frame {
	int width;
	int height;
	uint8_t *pixels;
	int64_t pts_us;
};

void atl_video_frame_free(struct atl_video_frame *frame);
/* detach the pixel buffer (caller becomes responsible for free()) */
uint8_t *atl_video_frame_take_pixels(struct atl_video_frame *frame);

struct atl_codec_backend {
	const char *name;

	/* codec_name is the (ffmpeg-style) decoder name MediaCodec.java resolves
	 * from the MIME type; returns NULL if this backend can't provide it */
	struct atl_codec *(*create)(const char *codec_name);

	void (*configure_audio)(struct atl_codec *codec, const uint8_t *extradata, size_t extradata_size,
	                        int sample_rate, int nb_channels);
	/* csd = concatenated codec-specific data (csd-0 + csd-1) */
	void (*configure_video)(struct atl_codec *codec, const uint8_t *csd, size_t csd_size);

	bool (*start)(struct atl_codec *codec);

	bool (*is_video)(struct atl_codec *codec);

	/* data == NULL signals end of stream. Returns 0, ATL_CODEC_AGAIN (input
	 * full, retry after draining output) or ATL_CODEC_ERROR. */
	int (*queue_input)(struct atl_codec *codec, const uint8_t *data, size_t size, int64_t pts_us);

	/* Audio: writes interleaved S16 PCM into out; returns the byte count,
	 * ATL_CODEC_AGAIN or ATL_CODEC_EOS. */
	int (*dequeue_audio)(struct atl_codec *codec, uint8_t *out, size_t capacity, int64_t *pts_us);

	/* Video: returns a frame the caller owns, or NULL with *status set to
	 * ATL_CODEC_AGAIN / ATL_CODEC_EOS / ATL_CODEC_ERROR. */
	struct atl_video_frame *(*dequeue_video)(struct atl_codec *codec, int *status);

	void (*release)(struct atl_codec *codec);
};

/* The active backend: ATL_MEDIA_CODEC_BACKEND env var selects by name,
 * default "ffmpeg". Never returns NULL. */
const struct atl_codec_backend *atl_codec_backend_get(void);

extern const struct atl_codec_backend atl_codec_backend_ffmpeg;

#endif
