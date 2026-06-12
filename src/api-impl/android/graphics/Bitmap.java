package android.graphics;

import android.util.DisplayMetrics;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.Buffer;

/*
 * Bitmap is implemented as an SkBitmap (the `texture` field) plus a lazily
 * created native canvas drawing into it (the `snapshot` field). Drawing
 * mutates the pixels in place, so both representations coexist. A GdkTexture
 * copy for GTK-side consumers (ImageView, backgrounds, ...) is cached in
 * `gdk_texture` and invalidated whenever the bitmap is drawn into.
 */
public final class Bitmap {

	public enum Config {
		RGB_565(2, /*ANDROID_BITMAP_FORMAT_RGB_565*/ 4),
		ARGB_8888(4, /*ANDROID_BITMAP_FORMAT_RGBA_8888*/ 1),
		ARGB_4444(2, /*ANDROID_BITMAP_FORMAT_RGBA_4444*/ 7),
		ALPHA_8(1, /*ANDROID_BITMAP_FORMAT_A_8*/ 8),
		RGBA_F16(8, /*ANDROID_BITMAP_FORMAT_RGBA_F16*/ 9),
		HARDWARE(4, /*ANDROID_BITMAP_FORMAT_RGBA_8888*/ 1);

		private int bytes_per_pixel;
		int android_memory_format; // used by native function AndroidBitmap_getInfo()

		private Config(int bytes_per_pixel, int android_memory_format) {
			this.bytes_per_pixel = bytes_per_pixel;
			this.android_memory_format = android_memory_format;
		}
	}

	public enum CompressFormat {
		JPEG,
		PNG,
		WEBP,
		WEBP_LOSSY,
		WEBP_LOSSLESS,
	}

	private int width;
	private int height;
	private int stride;
	private long texture;     // SkBitmap*
	private long snapshot;    // ATLCanvas*
	private long gdk_texture; // cached GdkTexture* copy for GTK consumers
	private Config config = Config.ARGB_8888;
	private boolean hasAlpha = true;
	long bytes = 0; // used by native function AndroidBitmap_lockPixels()
	private boolean recycled = false;
	boolean mutable = true;

	Bitmap(long texture) {
		this(native_get_width(texture), native_get_height(texture), Config.ARGB_8888);
		this.texture = texture;
	}

	private Bitmap(int width, int height, Config config) {
		this.config = config;
		this.width = width;
		this.height = height;
		int stride = width * config.bytes_per_pixel;
		this.stride = (stride + 3) & ~3; // 4-byte alignment
	}

	public static Bitmap createBitmap(int width, int height, Config config) {
		return new Bitmap(width, height, config);
	}

	public static Bitmap createBitmap(DisplayMetrics metrics, int width, int height, Config config) {
		return new Bitmap(width, height, config);
	}

	public static Bitmap createBitmap(DisplayMetrics metrics, int width, int height, Config config, boolean hasAlpha, ColorSpace colorSpace) {
		Bitmap bitmap = new Bitmap(width, height, config);
		bitmap.hasAlpha = hasAlpha;
		return bitmap;
	}

	public static Bitmap createBitmap(Bitmap src, int x, int y, int width, int height) {
		Bitmap dest = new Bitmap(width, height, src.getConfig());
		new Canvas(dest).drawBitmap(src, new Rect(x, y, x + width, y + height), new Rect(0, 0, width, height), null);
		return dest;
	}

	public static Bitmap createBitmap(Bitmap src, int x, int y, int width, int height, Matrix matrix, boolean filter) {
		Bitmap dest = new Bitmap(width, height, src.getConfig());
		Canvas canvas = new Canvas(dest);
		canvas.concat(matrix);
		canvas.drawBitmap(src, new Rect(x, y, x + width, y + height), new Rect(0, 0, width, height), null);
		return dest;
	}

	public static Bitmap createBitmap(int[] colors, int width, int height, Config config) {
		return createBitmap(width, height, config);
	}

	public static Bitmap createBitmap(Bitmap src) {
		return new Bitmap(native_copy_bitmap(src.getTexture()));
	}

	public static Bitmap createScaledBitmap(Bitmap src, int dstWidth, int dstHeight, boolean filter) {
		Bitmap dest = new Bitmap(dstWidth, dstHeight, src.getConfig());
		new Canvas(dest).drawBitmap(src, new Rect(0, 0, src.getWidth(), src.getHeight()), new Rect(0, 0, dstWidth, dstHeight), null);
		return dest;
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	public Config getConfig() {
		return config;
	}

	public synchronized long getTexture() {
		if (texture == 0) {
			texture = native_create_bitmap(width, height, stride, config.android_memory_format);
		}
		return texture;
	}

	synchronized long getSnapshot() {
		if (snapshot == 0) {
			snapshot = native_create_canvas(getTexture());
		}
		// the pixels are about to be drawn into: drop the cached GTK copy
		if (gdk_texture != 0) {
			native_unref_gdk_texture(gdk_texture);
			gdk_texture = 0;
		}
		return snapshot;
	}

	public synchronized long getGdkTexture() {
		if (gdk_texture == 0) {
			gdk_texture = native_create_gdk_texture(getTexture());
		}
		return gdk_texture;
	}

	public void eraseColor(int color) {
		getSnapshot(); // invalidates the cached GdkTexture
		native_erase_color(getTexture(), color);
	}

	public void recycle() {
		native_recycle(texture, snapshot, gdk_texture);
		texture = 0;
		snapshot = 0;
		gdk_texture = 0;
		recycled = true;
	}

	public int getRowBytes() {
		return stride;
	}

	public int getAllocationByteCount() {
		return height * getRowBytes();
	}

	public void prepareToDraw() {
		getTexture();
	}

	public void setDensity(int density) {}

	public int getScaledWidth(int density) {
		return width;
	}

	public int getScaledHeight(int density) {
		return height;
	}

	public boolean isRecycled() {
		return recycled;
	}

	public void setHasAlpha(boolean hasAlpha) {
		this.hasAlpha = hasAlpha;
	}

	public boolean hasAlpha() {
		return hasAlpha;
	}

	public Bitmap copy(Bitmap.Config config, boolean isMutable) {
		Bitmap bitmap = new Bitmap(width, height, config);
		bitmap.texture = native_copy_bitmap(getTexture());
		return bitmap;
	}

	public void getPixels(int[] pixels, int offset, int stride, int x, int y, int width, int height) {
		native_get_pixels(getTexture(), pixels, offset, stride, x, y, width, height);
	}

	public void copyPixelsToBuffer(Buffer buffer) {
		native_copy_to_buffer(getTexture(), buffer, config.android_memory_format, getRowBytes());
		buffer.position(buffer.position() + getAllocationByteCount());
	}

	public int getByteCount() {
		return getAllocationByteCount();
	}

	public boolean isMutable() {
		return mutable;
	}

	public boolean compress(Bitmap.CompressFormat format, int quality, OutputStream stream) throws IOException {
		if (format == CompressFormat.PNG) {
			stream.write(native_save_to_png(getTexture()));
			return true;
		} else {
			stream.write(("fixme Bitmap.compress " + format.name()).getBytes());
			return false;
		}
	}

	public void setPixels(int[] pixels, int offset, int stride, int x, int y, int width, int height) {
		getSnapshot(); // invalidates the cached GdkTexture
		native_set_pixels(getTexture(), pixels, offset, stride, x, y, width, height);
	}

	/* used by native function AndroidBitmap_lockPixels() */
	long getPixelsPtr() {
		getSnapshot(); // the caller may write to the pixels: invalidate the cached GdkTexture
		return native_get_pixels_ptr(getTexture());
	}

	public void reconfigure(int width, int height, Bitmap.Config config) {}

	public void setPremultiplied(boolean premultiplied) {}

	public Bitmap extractAlpha() {
		return this.copy(config, mutable);
	}

	@SuppressWarnings("deprecation")
	@Override
	protected void finalize() throws Throwable {
		try {
			recycle();
		} finally {
			super.finalize();
		}
	}

	private static native long native_create_bitmap(int width, int height, int stride, int format);
	private static native long native_create_canvas(long bitmap);
	private static native long native_create_gdk_texture(long bitmap);
	private static native void native_unref_gdk_texture(long gdk_texture);
	private static native int native_get_width(long bitmap);
	private static native int native_get_height(long bitmap);
	private static native void native_erase_color(long bitmap, int color);
	private static native void native_recycle(long bitmap, long canvas, long gdk_texture);
	private static native long native_copy_bitmap(long bitmap);
	private static native void native_get_pixels(long bitmap, int[] pixels, int offset, int stride, int x, int y, int width, int height);
	private static native void native_copy_to_buffer(long bitmap, Buffer buffer, int format, int stride);
	private static native byte[] native_save_to_png(long bitmap);
	private static native void native_set_pixels(long bitmap, int[] pixels, int offset, int stride, int x, int y, int width, int height);
	private static native long native_get_pixels_ptr(long bitmap);
}
