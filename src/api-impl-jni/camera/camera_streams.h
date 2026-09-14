#ifndef ATL_CAMERA_STREAMS_H
#define ATL_CAMERA_STREAMS_H

#include <stdbool.h>
#include <stdint.h>

#include "camera_backend.h"

/*
 * The camera2 stream session behind one open camera (AOSP: Camera3Device as
 * the CameraDeviceClient sees it). A backend that has real streams gets them
 * forwarded; one that has a single NV21 preview stream gets them emulated
 * here - every configured stream is filled from that one frame, converted to
 * its own format and size. The camera2 device layer speaks only this.
 *
 * Every call is made from the app thread; the callbacks arrive on backend
 * threads, never under a lock of this object's.
 */

struct atl_camera_streams;

struct atl_camera_streams *atl_camera_streams_new(const struct atl_camera_backend *backend,
                                                  struct atl_camera *camera,
                                                  const struct atl_camera_stream_callbacks *callbacks,
                                                  void *user);
/* tears the session down first; call before the backend closes the camera.
 * Buffers a consumer still holds stay valid until it releases them. */
void atl_camera_streams_free(struct atl_camera_streams *streams);

/* n_streams = 0 tears the session down. input is the reprocessing input the
 * session was created with, or NULL; a backend that emulates its streams
 * ignores it and serves the input from the ImageWriter queue instead. */
bool atl_camera_streams_configure(struct atl_camera_streams *streams,
                                  const struct atl_camera_stream *configs, int n_streams,
                                  const struct atl_camera_stream_input *input);
/* true when the backend serves the streams itself, false while emulated */
bool atl_camera_streams_are_real(struct atl_camera_streams *streams);
/* the same answer before a session is configured: whether the next one would
 * be the backend's own streams. The reprocessing input has to be built one way
 * or the other before the outputs are known. */
bool atl_camera_streams_would_be_real(struct atl_camera_streams *streams);

bool atl_camera_streams_submit(struct atl_camera_streams *streams, int request_id,
                               const struct atl_camera_metadata *settings, uint32_t targets,
                               bool repeating);
void atl_camera_streams_cancel_repeating(struct atl_camera_streams *streams);
/* the one-shot requests not completed yet are dropped; returns how many ids
 * were written, at most max */
int atl_camera_streams_flush(struct atl_camera_streams *streams, int *ids, int max);

/*
 * A reprocess capture on the emulated streams: the app's NV21 frame goes
 * through the targeted streams as if the camera had just produced it, and is
 * reported against the request. False on real streams, which have no input.
 */
bool atl_camera_streams_reprocess(struct atl_camera_streams *streams, int request_id,
                                  const struct atl_camera_metadata *settings, uint32_t targets,
                                  const uint8_t *nv21, int width, int height, int64_t timestamp);

/*
 * The same on real streams, where the input is a HAL stream of its own: the
 * app's buffer goes to the camera as it is, and a reprocess capture takes the
 * oldest queued one. Both are false on a session the backend does not serve
 * itself, and false when the backend has no reprocessing.
 */
bool atl_camera_streams_can_reprocess(struct atl_camera_streams *streams);
bool atl_camera_streams_queue_input(struct atl_camera_streams *streams,
                                    struct atl_camera_buffer *buffer,
                                    atl_camera_input_released_cb released, void *user);
bool atl_camera_streams_submit_reprocess(struct atl_camera_streams *streams, int request_id,
                                         const struct atl_camera_metadata *settings,
                                         uint32_t targets, int64_t input_timestamp);

/* consumer side: hand a buffer back to whoever produced it */
void atl_camera_buffer_release(struct atl_camera_buffer *buffer);

#endif
