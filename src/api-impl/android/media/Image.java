package android.media;

import android.graphics.Rect;
import android.hardware.HardwareBuffer;

import java.nio.ByteBuffer;

/**
 * One buffer handed over by an ImageReader (or, later, to an ImageWriter).
 *
 * The shape is AOSP's: an abstract class whose implementation belongs to the
 * reader that owns the buffer, so close() is what returns the slot. Reading
 * anything from a closed Image throws IllegalStateException — apps rely on
 * that to catch their own double-frees.
 */
public abstract class Image implements AutoCloseable {

	protected Rect mCropRect;

	protected Image() {
	}

	public abstract int getFormat();

	public abstract int getWidth();

	public abstract int getHeight();

	/** Capture timestamp in nanoseconds, the same clock the results carry. */
	public abstract long getTimestamp();

	public void setTimestamp(long timestamp) {
		throwISEIfInvalid();
	}

	public int getDataSpace() {
		throwISEIfInvalid();
		return 0; /* DataSpace.DATASPACE_UNKNOWN */
	}

	public void setDataSpace(int dataSpace) {
		throwISEIfInvalid();
	}

	public Rect getCropRect() {
		throwISEIfInvalid();
		if (mCropRect == null)
			return new Rect(0, 0, getWidth(), getHeight());
		return new Rect(mCropRect);
	}

	public void setCropRect(Rect cropRect) {
		throwISEIfInvalid();
		if (cropRect == null) {
			mCropRect = null;
			return;
		}
		Rect rect = new Rect(cropRect);
		if (!rect.intersect(0, 0, getWidth(), getHeight()))
			throw new IllegalArgumentException("crop rect is outside the image");
		mCropRect = rect;
	}

	/** null unless the implementation has a buffer to hand over. */
	public HardwareBuffer getHardwareBuffer() {
		throwISEIfInvalid();
		return null;
	}

	/** Empty for formats the app cannot read (PRIVATE). */
	public abstract Plane[] getPlanes();

	@Override
	public abstract void close();

	/* AOSP keeps this package-private; an implementation outside android.media
	 * is only expected to throw from its own accessors */
	boolean isImageValid() {
		return true;
	}

	/* the native buffer behind the image, for an ImageWriter handing one back
	 * for reprocessing; 0 when the implementation has none */
	long getNativeImagePtr() {
		return 0;
	}

	void throwISEIfInvalid() {
		if (!isImageValid())
			throw new IllegalStateException("this Image has been closed");
	}

	public static abstract class Plane {

		protected Plane() {
		}

		public abstract ByteBuffer getBuffer();

		public abstract int getRowStride();

		/** 1 for a packed plane, 2 for the interleaved chroma of an NV21 buffer. */
		public abstract int getPixelStride();
	}
}
