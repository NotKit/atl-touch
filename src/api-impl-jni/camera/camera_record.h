#ifndef ATL_CAMERA_RECORD_H
#define ATL_CAMERA_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#include "camera_backend.h"

/*
 * ATL_CAMERA_RECORD: the camera2 session, kept in a ring in memory and written
 * out around a capture (camera_recording.h is the format, camera_replay.c
 * plays it back). Every call here is a no-op when it is unset.
 *
 * The tap is camera_streams.c, so a session is recorded whether the backend
 * serves its streams itself or the emulation stands in for them - the events
 * are the same either way, and so is what a replay has to produce.
 */

struct atl_camera_streams;

bool atl_camera_record_enabled(void);

/* the session's streams, and the characteristics of every camera the backend
 * has; n_streams = 0 ends the session and writes out a burst still in hand */
void atl_camera_record_session(struct atl_camera_streams *session,
                               const struct atl_camera_backend *backend,
                               const struct atl_camera_stream *streams, int n_streams,
                               const struct atl_camera_stream_input *input);
void atl_camera_record_end(struct atl_camera_streams *session);

/* a request as the app submitted it, 3A decisions and all; a one-shot is what
 * a burst is centred on */
void atl_camera_record_request(struct atl_camera_streams *session, int request_id,
                               const struct atl_camera_metadata *settings, uint32_t targets,
                               bool repeating, bool reprocess, int64_t input_timestamp);

void atl_camera_record_started(struct atl_camera_streams *session, int request_id,
                               int64_t frame_number, int64_t timestamp);
void atl_camera_record_result(struct atl_camera_streams *session, int request_id,
                              int64_t frame_number, const struct atl_camera_metadata *result);
void atl_camera_record_failed(struct atl_camera_streams *session, int request_id,
                              int64_t frame_number);
void atl_camera_record_buffer(struct atl_camera_streams *session,
                              const struct atl_camera_buffer *buffer);
void atl_camera_record_lost(struct atl_camera_streams *session, int request_id,
                            int64_t frame_number, int stream);

#endif
