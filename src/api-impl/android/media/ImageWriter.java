package android.media;

import android.graphics.ImageFormat;
import android.hardware.camera2.impl.HandlerExecutor;
import android.os.Handler;
import android.view.Surface;

import java.nio.ByteBuffer;
import java.util.concurrent.Executor;

/**
 * The producer side of a reprocessing session's input Surface.
 *
 * Two ways to put an image in: dequeueInputImage() hands out an empty buffer
 * to fill, and queueInputImage() takes an Image that came from an ImageReader
 * (the zero-shutter-lag path — a PRIVATE frame the app kept and decided to
 * turn into a JPEG). Both end up in the queue the next reprocess capture
 * takes its frame from.
 */
public class ImageWriter implements AutoCloseable {

	public interface OnImageReleasedListener {
		void onImageReleased(ImageWriter writer);
	}

	private final int width;
	private final int height;
	private final int format;
	private final int maxImages;

	/* the camera's own input stream is behind the Surface: a queued image is
	 * out until the camera gives it back, and only then is the app told */
	private final boolean realInput;

	private long nativePtr;
	private OnImageReleasedListener listener;
	private Executor listenerExecutor;

	private ImageWriter(Surface surface, int maxImages, int format) {
		nativePtr = native_fromSurface(surface, this);
		if (nativePtr == 0)
			throw new IllegalArgumentException("that Surface is not a reprocessing input");

		int[] info = native_info(nativePtr);
		width = info[0];
		height = info[1];
		this.format = info[2];
		this.maxImages = Math.min(maxImages, info[3]);
		realInput = info[4] != 0;

		if (format != ImageFormat.UNKNOWN && format != this.format) {
			close();
			throw new IllegalArgumentException("the input Surface is format 0x" +
			    Integer.toHexString(this.format) + ", not 0x" + Integer.toHexString(format));
		}
	}

	public static ImageWriter newInstance(Surface surface, int maxImages) {
		return newInstance(surface, maxImages, ImageFormat.UNKNOWN);
	}

	public static ImageWriter newInstance(Surface surface, int maxImages, int format) {
		if (surface == null)
			throw new IllegalArgumentException("surface must not be null");
		if (maxImages < 1)
			throw new IllegalArgumentException("maxImages must be at least 1");
		return new ImageWriter(surface, maxImages, format);
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	public int getFormat() {
		return format;
	}

	public int getMaxImages() {
		return maxImages;
	}

	public int getDataSpace() {
		return 0; /* DataSpace.DATASPACE_UNKNOWN */
	}

	/**
	 * An empty image to fill. PRIVATE has no CPU layout to hand out, which is
	 * what AOSP refuses too — those images come from a reader instead.
	 */
	public synchronized Image dequeueInputImage() {
		checkOpen();
		if (format == ImageFormat.PRIVATE)
			throw new IllegalStateException("PRIVATE images cannot be dequeued, only queued");
		if (realInput)
			throw new IllegalStateException("the camera owns this input's buffers; queue one of "
			    + "its own images instead");

		long frame = native_dequeue(nativePtr);
		if (frame == 0)
			throw new IllegalStateException("maxImages (" + maxImages + ") are already dequeued");
		return new WriterImage(frame);
	}

	public synchronized void queueInputImage(Image image) {
		checkOpen();
		if (image == null)
			throw new IllegalArgumentException("image must not be null");

		boolean queued;
		if (image instanceof WriterImage) {
			WriterImage own = (WriterImage)image;
			long timestamp = own.getTimestamp();

			queued = native_queueOwn(nativePtr, own.detach(), timestamp);
		} else {
			long imagePtr = image.getNativeImagePtr();

			if (imagePtr == 0)
				throw new IllegalArgumentException("that Image is not backed by a buffer");
			queued = native_queueForeign(nativePtr, imagePtr);
			image.close(); /* AOSP takes ownership of a queued image */
		}
		if (!queued)
			throw new IllegalStateException("the reprocessing input queue is full");
		/* a real input answers when the camera is done with the buffer */
		if (!realInput)
			dispatchImageReleased();
	}

	public synchronized void setOnImageReleasedListener(OnImageReleasedListener listener, Handler handler) {
		this.listener = listener;
		this.listenerExecutor = HandlerExecutor.of(handler);
	}

	@Override
	public synchronized void close() {
		if (nativePtr != 0) {
			native_close(nativePtr);
			nativePtr = 0;
		}
	}

	private void dispatchImageReleased() {
		final OnImageReleasedListener target = listener;
		final Executor executor = listenerExecutor;

		if (target == null)
			return;
		HandlerExecutor.run(executor, new Runnable() {
			@Override
			public void run() {
				target.onImageReleased(ImageWriter.this);
			}
		});
	}

	private void checkOpen() {
		if (nativePtr == 0)
			throw new IllegalStateException("this ImageWriter has been closed");
	}

	/** An image the app fills; its buffer belongs to the writer's queue. */
	private class WriterImage extends Image {
		private long framePtr;
		private long timestamp;
		private Plane[] planes;

		WriterImage(long framePtr) {
			this.framePtr = framePtr;
		}

		/* queueInputImage takes the buffer over: the image is spent afterwards */
		long detach() {
			long ptr = framePtr;

			framePtr = 0;
			planes = null;
			return ptr;
		}

		@Override
		public int getFormat() {
			throwISEIfInvalid();
			return format;
		}

		@Override
		public int getWidth() {
			throwISEIfInvalid();
			return width;
		}

		@Override
		public int getHeight() {
			throwISEIfInvalid();
			return height;
		}

		@Override
		public long getTimestamp() {
			throwISEIfInvalid();
			return timestamp;
		}

		@Override
		public void setTimestamp(long timestamp) {
			throwISEIfInvalid();
			this.timestamp = timestamp;
		}

		@Override
		public synchronized Plane[] getPlanes() {
			throwISEIfInvalid();
			if (planes == null) {
				planes = new Plane[3];
				for (int i = 0; i < planes.length; i++)
					planes[i] = new WriterPlane(i);
			}
			return planes.clone();
		}

		@Override
		public synchronized void close() {
			if (framePtr != 0) {
				native_cancel(nativePtr, framePtr);
				framePtr = 0;
				planes = null;
			}
		}

		@Override
		boolean isImageValid() {
			return framePtr != 0;
		}

		private class WriterPlane extends Image.Plane {
			private final int rowStride;
			private final int pixelStride;
			private final int offset;
			private final int size;
			private ByteBuffer buffer;

			WriterPlane(int index) {
				int[] info = native_planeInfo(framePtr, index);

				rowStride = info[0];
				pixelStride = info[1];
				offset = info[2];
				size = info[3];
			}

			@Override
			public synchronized ByteBuffer getBuffer() {
				throwISEIfInvalid();
				if (buffer == null)
					buffer = native_planeBuffer(framePtr, offset, size);
				return buffer;
			}

			@Override
			public int getRowStride() {
				throwISEIfInvalid();
				return rowStride;
			}

			@Override
			public int getPixelStride() {
				throwISEIfInvalid();
				return pixelStride;
			}
		}
	}

	private static native long native_fromSurface(Surface surface, ImageWriter self);
	/* {width, height, format, maxImages} */
	private static native int[] native_info(long ptr);
	private static native long native_dequeue(long ptr);
	private static native boolean native_queueOwn(long ptr, long framePtr, long timestamp);
	private static native boolean native_queueForeign(long ptr, long imagePtr);
	/* {rowStride, pixelStride, offset, size} */
	private static native int[] native_planeInfo(long framePtr, int plane);
	private static native ByteBuffer native_planeBuffer(long framePtr, int offset, int size);
	private static native void native_cancel(long ptr, long framePtr);
	private static native void native_close(long ptr);
}
