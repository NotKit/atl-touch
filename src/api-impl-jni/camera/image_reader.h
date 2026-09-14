#ifndef ATL_IMAGE_READER_H
#define ATL_IMAGE_READER_H

#include <jni.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "camera_backend.h"

/*
 * The native side of android.media.ImageReader: the consumer end of one
 * camera2 stream (AOSP: a BufferQueue consumer). The producer hands it filled
 * buffers in the reader's own format; the reader queues them for the app and
 * hands each back to the producer when the app closes the Image.
 *
 * At most maxImages buffers are outstanding - queued plus acquired by the app
 * - and a buffer beyond that is released at once, so an app that stops
 * draining falls behind rather than growing without bound.
 */

struct atl_image_reader;
struct atl_image;

/* the reader behind an ImageReader-backed android.view.Surface, with a
 * reference taken; NULL when the surface is not one */
struct atl_image_reader *atl_image_reader_from_surface(JNIEnv *env, jobject surface);
void atl_image_reader_ref(struct atl_image_reader *reader);
void atl_image_reader_unref(struct atl_image_reader *reader);

/* what the stream feeding it has to be */
void atl_image_reader_get_stream(struct atl_image_reader *reader, struct atl_camera_stream *stream);
int atl_image_reader_get_format(struct atl_image_reader *reader);

/* producer side, on any thread: the reader owns the buffer from here until
 * the app closes the Image, or releases it at once when it has no room */
void atl_image_reader_submit(struct atl_image_reader *reader, struct atl_camera_buffer *buffer);

/* the pixels behind an Image the app is holding, when they are one run of
 * bytes - NV21 for an emulated YUV_420_888 or PRIVATE image, the encoded bytes
 * for JPEG, the packed plane for raw - so an ImageWriter can hand one back for
 * reprocessing. False for a HAL buffer whose planes are separate. */
bool atl_image_get_data(struct atl_image *image, const uint8_t **data, size_t *size,
                        int *width, int *height, int *format, int64_t *timestamp);

/*
 * Takes the camera buffer out of an Image: the caller owns it and gives it
 * back with atl_camera_buffer_release(). This is what the reprocessing input
 * of a real session does - the camera gets its own gralloc buffer back rather
 * than a copy, so the buffer has to outlive the Image the app closes. NULL
 * when the image has none left.
 */
struct atl_camera_buffer *atl_image_take_buffer(struct atl_image *image);

#endif
