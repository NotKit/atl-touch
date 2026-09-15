package android.media;

import android.graphics.ImageFormat;
import android.hardware.HardwareBuffer;
import android.hardware.camera2.impl.HandlerExecutor;
import android.os.Handler;
import android.view.Surface;

import java.nio.ByteBuffer;
import java.util.concurrent.Executor;

/**
 * A Surface that hands its frames to the app as android.media.Image.
 *
 * The queue lives in native code (image_reader.h): the producer — today a
 * camera2 capture session — converts each frame into the reader's format and
 * enqueues it, up to maxImages outstanding buffers. Beyond that frames are
 * dropped rather than queued, so an app that stops draining falls behind
 * instead of growing without bound.
 */
public class ImageReader implements AutoCloseable {

	public interface OnImageAvailableListener {
		void onImageAvailable(ImageReader reader);
	}

	private final int width;
	private final int height;
	private final int format;
	private final int maxImages;
	private final long usage;
	private final Surface surface = new Surface();

	private long nativePtr;
	private OnImageAvailableListener listener;
	private Executor listenerExecutor;

	private ImageReader(int width, int height, int format, int maxImages, long usage) {
		this.width = width;
		this.height = height;
		this.format = format;
		this.maxImages = maxImages;
		this.usage = usage;

		nativePtr = native_create(width, height, format, maxImages, usage, this);
		if (nativePtr == 0)
			throw new UnsupportedOperationException("cannot create a " + width + "x" + height +
			    " ImageReader of format 0x" + Integer.toHexString(format));
		native_attachSurface(nativePtr, surface);
	}

	public static ImageReader newInstance(int width, int height, int format, int maxImages) {
		return newInstance(width, height, format, maxImages,
		    HardwareBuffer.USAGE_CPU_READ_OFTEN);
	}

	public static ImageReader newInstance(int width, int height, int format, int maxImages, long usage) {
		if (width < 1 || height < 1)
			throw new IllegalArgumentException("an ImageReader needs a positive size");
		if (maxImages < 1)
			throw new IllegalArgumentException("maxImages must be at least 1");
		return new ImageReader(width, height, format, maxImages, usage);
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	public int getImageFormat() {
		return format;
	}

	public int getMaxImages() {
		return maxImages;
	}

	public long getUsage() {
		return usage;
	}

	public int getHardwareBufferFormat() {
		return format;
	}

	public int getDataSpace() {
		return 0; /* DataSpace.DATASPACE_UNKNOWN */
	}

	public Surface getSurface() {
		return surface;
	}

	/** The oldest queued image, or null when nothing is waiting. */
	public Image acquireNextImage() {
		return acquire(false);
	}

	/** The newest queued image; everything older is dropped. */
	public Image acquireLatestImage() {
		return acquire(true);
	}

	/* ATL_DEBUG_IMAGEREADER: one line per acquire, close and callback, with the
	 * image timestamp and the calling thread - what an app's frame matching sees */
	public static final boolean DEBUG = System.getenv("ATL_DEBUG_IMAGEREADER") != null;
	/* ATL_DEBUG_IMAGEREADER=stacks adds the Java stack of every acquire and
	 * close on a PRIVATE reader: which of the app's wrappers is asking */
	public static final boolean DEBUG_STACKS = DEBUG && "stacks".equals(System.getenv("ATL_DEBUG_IMAGEREADER"));

	private void debugStack(String what) {
		if (format != 0x22)
			return;
		android.util.Log.i("ImageReaderDbg", tag() + " " + what + " from\n"
		    + android.util.Log.getStackTraceString(new Throwable()));
	}

	private String tag() {
		return "ImageReader[" + width + "x" + height + " fmt 0x" + Integer.toHexString(format) + "]";
	}

	private synchronized Image acquire(boolean latest) {
		if (nativePtr == 0)
			throw new IllegalStateException("this ImageReader has been closed");

		long image = native_acquire(nativePtr, latest);
		if (image == 0) {
			if (DEBUG)
				android.util.Log.i("ImageReaderDbg", tag() + " acquire" + (latest ? "Latest" : "Next") + " -> null on " + Thread.currentThread().getName());
			return null;
		}
		if (image == -1)
			throw new IllegalStateException("maxImages (" + maxImages + ") have already been acquired");
		SurfaceImage result = new SurfaceImage(image);
		if (DEBUG)
			android.util.Log.i("ImageReaderDbg", tag() + " acquire" + (latest ? "Latest" : "Next") + " -> ts=" + result.timestamp + " on " + Thread.currentThread().getName());
		if (DEBUG_STACKS)
			debugStack("acquire ts=" + result.timestamp);
		return result;
	}

	public synchronized void setOnImageAvailableListener(OnImageAvailableListener listener, Handler handler) {
		this.listener = listener;
		/*
		 * The callbacks come off ATL's own dispatch thread now, so a null
		 * Handler has to name a Looper rather than mean "here": AOSP takes the
		 * calling thread's, and an app that passes null expects its own thread.
		 * The headless harness has no Looper at all and still runs inline.
		 */
		if (handler == null) {
			android.os.Looper looper = android.os.Looper.myLooper();

			if (looper == null)
				looper = android.os.Looper.getMainLooper();
			if (looper != null)
				handler = new Handler(looper);
		}
		this.listenerExecutor = HandlerExecutor.of(handler);
		if (nativePtr != 0)
			native_setListenerEnabled(nativePtr, listener != null);
	}

	/** Drops the buffers the reader is holding on to but nobody is using. */
	public synchronized void discardFreeBuffers() {
		if (nativePtr != 0)
			native_discardFreeBuffers(nativePtr);
	}

	@Override
	public synchronized void close() {
		if (nativePtr != 0) {
			native_close(nativePtr);
			nativePtr = 0;
		}
		surface.release();
	}

	/* called from native, on the main loop */
	private void dispatchImageAvailable() {
		final OnImageAvailableListener target;
		final Executor executor;

		synchronized (this) {
			target = listener;
			executor = listenerExecutor;
		}
		if (target == null)
			return;
		HandlerExecutor.run(executor, new Runnable() {
			@Override
			public void run() {
				/*
				 * The queue is checked again here, on the thread that is about
				 * to acquire, because this runnable is a hop away from the one
				 * that decided to post it. Two callbacks can be in flight over
				 * one image - the app drains the queue in the first
				 * (acquireLatestImage recycles all but the newest) and the
				 * second then acquires nothing. An app is entitled to
				 * dereference what it acquires, and Google Camera does: its
				 * viewfinder thread dies on the null and takes the preview
				 * with it.
				 */
				synchronized (ImageReader.this) {
					if (nativePtr == 0 || !native_hasQueuedImage(nativePtr)) {
						if (DEBUG)
							android.util.Log.i("ImageReaderDbg", tag() + " callback skipped, nothing queued, on " + Thread.currentThread().getName());
						return;
					}
				}
				if (DEBUG)
					android.util.Log.i("ImageReaderDbg", tag() + " onImageAvailable on " + Thread.currentThread().getName());
				target.onImageAvailable(ImageReader.this);
			}
		});
	}

	/** The reader's own images; closing one returns its buffer to the reader. */
	private class SurfaceImage extends Image {
		private long imagePtr;
		private final int imageWidth;
		private final int imageHeight;
		private final int imageFormat;
		private final long timestamp;
		private final int planeCount;
		private Plane[] planes;

		SurfaceImage(long imagePtr) {
			this.imagePtr = imagePtr;

			long[] info = native_imageInfo(imagePtr);
			imageWidth = (int)info[0];
			imageHeight = (int)info[1];
			imageFormat = (int)info[2];
			planeCount = (int)info[3];
			timestamp = info[4];
		}

		@Override
		public int getFormat() {
			throwISEIfInvalid();
			return imageFormat;
		}

		@Override
		public int getWidth() {
			throwISEIfInvalid();
			return imageWidth;
		}

		@Override
		public int getHeight() {
			throwISEIfInvalid();
			return imageHeight;
		}

		@Override
		public long getTimestamp() {
			throwISEIfInvalid();
			return timestamp;
		}

		@Override
		public synchronized Plane[] getPlanes() {
			throwISEIfInvalid();
			if (planes == null) {
				planes = new Plane[planeCount];
				for (int i = 0; i < planeCount; i++)
					planes[i] = new SurfacePlane(i);
			}
			return planes.clone();
		}

		@Override
		public HardwareBuffer getHardwareBuffer() {
			throwISEIfInvalid();
			return native_imageHardwareBuffer(imagePtr);
		}

		@Override
		public synchronized void close() {
			if (imagePtr != 0) {
				if (DEBUG)
					android.util.Log.i("ImageReaderDbg", tag() + " close ts=" + timestamp + " on " + Thread.currentThread().getName());
				if (DEBUG_STACKS)
					debugStack("close ts=" + timestamp);
				native_imageClose(imagePtr);
				imagePtr = 0;
				planes = null;
			}
		}

		@Override
		boolean isImageValid() {
			return imagePtr != 0;
		}

		@Override
		long getNativeImagePtr() {
			return imagePtr;
		}

		private class SurfacePlane extends Image.Plane {
			private final int index;
			private final int rowStride;
			private final int pixelStride;
			/* AOSP's name for the mapping: camera apps swap it by reflection to
			 * feed their pipeline a buffer of their own */
			private ByteBuffer mBuffer;

			SurfacePlane(int index) {
				this.index = index;

				int[] strides = native_planeStrides(imagePtr, index);
				rowStride = strides[0];
				pixelStride = strides[1];
			}

			@Override
			public synchronized ByteBuffer getBuffer() {
				throwISEIfInvalid();
				if (mBuffer == null)
					mBuffer = native_planeBuffer(imagePtr, index);
				return mBuffer;
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

	public static final class Builder {
		private final int width;
		private final int height;
		private int format = ImageFormat.PRIVATE;
		private int maxImages = 1;
		private long usage = HardwareBuffer.USAGE_CPU_READ_OFTEN;

		public Builder(int width, int height) {
			this.width = width;
			this.height = height;
		}

		public Builder setImageFormat(int format) {
			this.format = format;
			return this;
		}

		/** ATL has no gralloc formats of its own: this is the image format. */
		public Builder setDefaultHardwareBufferFormat(int format) {
			this.format = format;
			return this;
		}

		public Builder setDefaultDataSpace(int dataSpace) {
			return this;
		}

		public Builder setMaxImages(int maxImages) {
			this.maxImages = maxImages;
			return this;
		}

		public Builder setUsage(long usage) {
			this.usage = usage;
			return this;
		}

		public ImageReader build() {
			return newInstance(width, height, format, maxImages, usage);
		}
	}

	private static native long native_create(int width, int height, int format, int maxImages,
	    long usage, ImageReader self);
	private static native void native_attachSurface(long ptr, Surface surface);
	private static native void native_setListenerEnabled(long ptr, boolean enabled);
	/** whether an acquire would find anything; see dispatchImageAvailable */
	private static native boolean native_hasQueuedImage(long ptr);
	private static native long native_acquire(long ptr, boolean latest);
	private static native void native_discardFreeBuffers(long ptr);
	private static native void native_close(long ptr);

	/* {width, height, format, planeCount, timestamp} */
	static native long[] native_imageInfo(long imagePtr);
	static native int[] native_planeStrides(long imagePtr, int plane);
	static native ByteBuffer native_planeBuffer(long imagePtr, int plane);
	static native HardwareBuffer native_imageHardwareBuffer(long imagePtr);
	static native void native_imageClose(long imagePtr);
}
