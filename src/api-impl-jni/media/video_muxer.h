#ifndef ATL_VIDEO_MUXER_H
#define ATL_VIDEO_MUXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The container writer behind android.media.MediaMuxer: appsrc ! h264parse !
 * mp4mux ! filesink, fed the encoded access units a MediaCodec encoder hands
 * the app. One video track is all a camera app needs, and all the single
 * appsrc serves.
 */

struct atl_video_muxer;

struct atl_video_muxer *atl_video_muxer_new(const char *path);

/* the caps of the one track; csd is the Annex B SPS/PPS, which h264parse also
 * finds in the stream itself, so it may be NULL */
bool atl_video_muxer_add_track(struct atl_video_muxer *muxer, int width, int height, int fps,
                               const uint8_t *csd, size_t csd_size);
void atl_video_muxer_set_orientation(struct atl_video_muxer *muxer, int degrees);

bool atl_video_muxer_start(struct atl_video_muxer *muxer);
bool atl_video_muxer_write(struct atl_video_muxer *muxer, const uint8_t *data, size_t size,
                           int64_t pts_us, bool keyframe);
/* EOS and drain: the MP4's moov is written here */
bool atl_video_muxer_stop(struct atl_video_muxer *muxer);
void atl_video_muxer_free(struct atl_video_muxer *muxer);

#endif
