package android.hardware;

import android.os.Parcel;
import android.os.Parcelable;

/**
 * A native graphics buffer.
 *
 * ATL has no gralloc, so a HardwareBuffer is either a plain native allocation
 * (create()) or a window onto a buffer something else owns — today the pixels
 * of an android.media.Image, handed over without a copy. Closing the buffer
 * only frees what it allocated itself.
 */
public final class HardwareBuffer implements Parcelable, AutoCloseable {

	public static final int RGBA_8888 = 1;
	public static final int RGBX_8888 = 2;
	public static final int RGB_888 = 3;
	public static final int RGB_565 = 4;
	public static final int RGBA_FP16 = 0x16;
	public static final int RGBA_1010102 = 0x2b;
	public static final int BLOB = 0x21;
	public static final int D_16 = 0x30;
	public static final int D_24 = 0x31;
	public static final int DS_24UI8 = 0x32;
	public static final int D_FP32 = 0x33;
	public static final int DS_FP32UI8 = 0x34;
	public static final int S_UI8 = 0x35;
	public static final int YCBCR_420_888 = 0x23;
	public static final int YCBCR_P010 = 0x36;

	public static final long USAGE_CPU_READ_RARELY = 2;
	public static final long USAGE_CPU_READ_OFTEN = 3;
	public static final long USAGE_CPU_WRITE_RARELY = 2 << 4;
	public static final long USAGE_CPU_WRITE_OFTEN = 3 << 4;
	public static final long USAGE_GPU_SAMPLED_IMAGE = 1 << 8;
	public static final long USAGE_GPU_COLOR_OUTPUT = 1 << 9;
	public static final long USAGE_COMPOSER_OVERLAY = 1 << 11;
	public static final long USAGE_PROTECTED_CONTENT = 1 << 14;
	public static final long USAGE_VIDEO_ENCODE = 1 << 16;
	public static final long USAGE_GPU_DATA_BUFFER = 1 << 24;
	public static final long USAGE_SENSOR_DIRECT_DATA = 1 << 23;
	public static final long USAGE_GPU_CUBE_MAP = 1 << 25;
	public static final long USAGE_GPU_MIPMAP_COMPLETE = 1 << 26;
	public static final long USAGE_FRONT_BUFFER = 1L << 32;

	private final int width;
	private final int height;
	private final int format;
	private final int layers;
	private final long usage;
	private long nativePtr;

	private HardwareBuffer(long nativePtr, int width, int height, int format, int layers, long usage) {
		this.nativePtr = nativePtr;
		this.width = width;
		this.height = height;
		this.format = format;
		this.layers = layers;
		this.usage = usage;
	}

	public static HardwareBuffer create(int width, int height, int format, int layers, long usage) {
		if (width <= 0 || height <= 0 || layers <= 0)
			throw new IllegalArgumentException("bad HardwareBuffer geometry");

		long ptr = native_create(width, height, format, layers, usage);
		if (ptr == 0)
			throw new IllegalArgumentException("cannot allocate a " + width + "x" + height +
			    " HardwareBuffer of format " + format);
		return new HardwareBuffer(ptr, width, height, format, layers, usage);
	}

	public static boolean isSupported(int width, int height, int format, int layers, long usage) {
		return width > 0 && height > 0 && layers > 0;
	}

	/* called from native: wrap a buffer somebody else owns (an Image's pixels) */
	static HardwareBuffer fromNative(long ptr, int width, int height, int format, int layers, long usage) {
		return new HardwareBuffer(ptr, width, height, format, layers, usage);
	}

	public int getWidth() {
		checkOpen();
		return width;
	}

	public int getHeight() {
		checkOpen();
		return height;
	}

	public int getFormat() {
		checkOpen();
		return format;
	}

	public int getLayers() {
		checkOpen();
		return layers;
	}

	public long getUsage() {
		checkOpen();
		return usage;
	}

	public boolean isClosed() {
		return nativePtr == 0;
	}

	@Override
	public synchronized void close() {
		if (nativePtr != 0) {
			native_close(nativePtr);
			nativePtr = 0;
		}
	}

	/** The native handle, for JNI code that has to reach the pixels. */
	public long getNativePtr() {
		return nativePtr;
	}

	private void checkOpen() {
		if (nativePtr == 0)
			throw new IllegalStateException("this HardwareBuffer has been closed");
	}

	@Override
	public int describeContents() {
		return 0;
	}

	@Override
	public void writeToParcel(Parcel dest, int flags) {
		throw new UnsupportedOperationException("HardwareBuffer cannot be parcelled under ATL");
	}

	public static final Parcelable.Creator<HardwareBuffer> CREATOR = new Parcelable.Creator<HardwareBuffer>() {
		@Override
		public HardwareBuffer createFromParcel(Parcel in) {
			throw new UnsupportedOperationException("HardwareBuffer cannot be parcelled under ATL");
		}

		@Override
		public HardwareBuffer[] newArray(int size) {
			return new HardwareBuffer[size];
		}
	};

	private static native long native_create(int width, int height, int format, int layers, long usage);
	private static native void native_close(long ptr);
}
