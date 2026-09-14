#ifndef ATL_IMAGE_WRITER_H
#define ATL_IMAGE_WRITER_H

#include <jni.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * The input side of a reprocessing session: the queue an android.media.
 * ImageWriter pushes images into and a reprocess capture takes them out of.
 *
 * It is the mirror of image_reader.h — same bounded queue, same free list —
 * except that the app is the producer and the session is the consumer. The
 * images carry NV21, which is what every ATL buffer is: a PRIVATE image an
 * app got from an ImageReader can therefore be handed straight back.
 */

struct atl_image_writer;
struct atl_image_writer_frame;
struct atl_camera_buffer;

/*
 * A real session's input, where the camera's own stream is behind the Surface:
 * the Image's HAL buffer goes to the camera as it is, and the writer holds no
 * pixels of its own. queue() owns the buffer once it returns true and gives it
 * back through atl_image_writer_released() when the camera is done with it.
 */
struct atl_image_writer_sink {
	bool (*queue)(void *user, struct atl_camera_buffer *buffer);
	void *user;
	/* the reference the sink holds on user, plus one the writer takes across
	 * a queue() call: the session may be closed from another thread while an
	 * app thread is inside queueInputImage */
	void (*ref)(void *user);
	void (*unref)(void *user);
};

struct atl_image_writer *atl_image_writer_new(int width, int height, int format, int max_images);
/* an input a real session serves; any format the camera streams, since nothing
 * here has to understand the pixels */
struct atl_image_writer *atl_image_writer_new_input(int width, int height, int format,
                                                    int max_images,
                                                    const struct atl_image_writer_sink *sink);
bool atl_image_writer_is_real(struct atl_image_writer *writer);
/* drops the sink's own reference; call before the last reference to whatever
 * the sink points at goes */
void atl_image_writer_clear_sink(struct atl_image_writer *writer);
/* the camera released a queued buffer: the app may write another */
void atl_image_writer_released(struct atl_image_writer *writer);
void atl_image_writer_attach_surface(JNIEnv *env, struct atl_image_writer *writer, jobject surface);
/* the writer behind a reprocessing input Surface, with a reference taken */
struct atl_image_writer *atl_image_writer_from_surface(JNIEnv *env, jobject surface);
void atl_image_writer_ref(struct atl_image_writer *writer);
void atl_image_writer_unref(struct atl_image_writer *writer);

void atl_image_writer_get_size(struct atl_image_writer *writer, int *width, int *height);
int atl_image_writer_get_format(struct atl_image_writer *writer);

/* consumer side: the oldest queued image, or NULL. Give it back with
 * atl_image_writer_release(). */
struct atl_image_writer_frame *atl_image_writer_take(struct atl_image_writer *writer);
void atl_image_writer_release(struct atl_image_writer *writer, struct atl_image_writer_frame *frame);
const uint8_t *atl_image_writer_frame_data(const struct atl_image_writer_frame *frame,
                                           int *width, int *height, int64_t *timestamp);

#endif
