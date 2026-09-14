#ifndef ATL_VIDEO_ENCODER_H
#define ATL_VIDEO_ENCODER_H

#include <jni.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The H.264 encoder behind a recording Surface.
 *
 * One GStreamer pipeline per encoder, fed NV21 frames by whoever owns the
 * Surface (today a camera2 capture session) and running in one of two modes:
 *
 *  - file mode (MediaRecorder): ... ! h264parse ! mp4mux ! filesink, so the
 *    encoder writes the MP4 itself and finish() is what makes it playable
 *    (the moov is written on EOS).
 *  - sample mode (MediaCodec.createInputSurface): ... ! appsink, so the app
 *    pulls encoded access units out of dequeueOutputBuffer and hands them to
 *    a MediaMuxer.
 *
 * Frames are scaled to the encoder's size in the pipeline, so a session whose
 * stream is larger than the recording still records at the requested size.
 */

struct atl_video_encoder;

/* pull results */
#define ATL_ENCODER_AGAIN (-1)
#define ATL_ENCODER_EOS   (-2)

/* output_path NULL selects sample mode; bitrate is in bits per second */
struct atl_video_encoder *atl_video_encoder_new(int width, int height, int fps, int bitrate,
                                                const char *output_path);

/* the encoder behind a recording Surface, with a reference taken; NULL when
 * the surface is not one */
struct atl_video_encoder *atl_video_encoder_from_surface(JNIEnv *env, jobject surface);
void atl_video_encoder_attach_surface(JNIEnv *env, struct atl_video_encoder *encoder, jobject surface);
void atl_video_encoder_ref(struct atl_video_encoder *encoder);
void atl_video_encoder_unref(struct atl_video_encoder *encoder);

void atl_video_encoder_get_size(struct atl_video_encoder *encoder, int *width, int *height);

bool atl_video_encoder_start(struct atl_video_encoder *encoder);

/* producer side, called on the camera backend's frame thread; timestamp is in
 * nanoseconds and only its difference from the first frame's matters */
void atl_video_encoder_submit(struct atl_video_encoder *encoder, const uint8_t *nv21,
                             int width, int height, int stride, int64_t timestamp);

/* sample mode: copy the next access unit into out. Returns its size, or
 * ATL_ENCODER_AGAIN / ATL_ENCODER_EOS. */
int atl_video_encoder_pull(struct atl_video_encoder *encoder, uint8_t *out, size_t capacity,
                           int64_t *pts_us, bool *keyframe, int64_t timeout_us);

/* the SPS+PPS of the stream (Annex B, as MediaCodec's csd-0), once the first
 * access unit has been pulled; NULL before that */
const uint8_t *atl_video_encoder_get_csd(struct atl_video_encoder *encoder, size_t *size);

/* no more frames; what is already in the pipeline still comes out */
void atl_video_encoder_signal_eos(struct atl_video_encoder *encoder);

/* stop feeding: the pipeline drains and, in file mode, finishes the MP4 */
bool atl_video_encoder_finish(struct atl_video_encoder *encoder);

uint64_t atl_video_encoder_get_frame_count(struct atl_video_encoder *encoder);

#endif
