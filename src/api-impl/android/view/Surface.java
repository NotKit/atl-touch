package android.view;

import android.graphics.SurfaceTexture;

public class Surface implements android.os.Parcelable {
	public long widget;
	/* the SurfaceView this surface presents into (null for detached surfaces) */
	SurfaceView view;
	/* struct ANativeWindow*, built by the SurfaceView when its layer came up and
	 * read by ANativeWindow_fromSurface. Guarded by this object's monitor. */
	public long nativeWindow;
	/* the frame sink behind a Surface(SurfaceTexture); camera2 reads this
	 * field from native to find the mailbox to fill */
	private SurfaceTexture surfaceTexture;
	/* the other kind of sink: struct atl_image_reader *, set from native when
	 * an ImageReader hands out its Surface */
	private long imageReaderPtr;
	/* struct atl_video_encoder *: a MediaRecorder or MediaCodec encoder input */
	private long videoEncoderPtr;
	/* struct atl_image_writer *: the input side of a reprocessing session,
	 * where an ImageWriter queues images rather than a producer filling them */
	private long imageWriterPtr;

	public Surface() {
	}

	public Surface(SurfaceTexture surfaceTexture) {
		if (surfaceTexture == null)
			throw new IllegalArgumentException("surfaceTexture must not be null");
		this.surfaceTexture = surfaceTexture;
	}

	public Surface(android.view.SurfaceControl a0) {
	}

	/* who asks whether a surface is usable says a lot about where an app's
	 * viewfinder pipeline got to; ATL_DEBUG_SURFACE traces the first few */
	private static final boolean DEBUG = System.getenv("ATL_DEBUG_SURFACE") != null;
	private static int debugTraces = 0;

	public boolean isValid() {
		boolean valid = view != null || widget != 0 || nativeWindow != 0 || surfaceTexture != null ||
		    imageReaderPtr != 0 || videoEncoderPtr != 0 || imageWriterPtr != 0;
		if (DEBUG && debugTraces < 30) {
			debugTraces++;
			StringBuilder b = new StringBuilder("Surface: isValid() -> " + valid + " from");
			StackTraceElement[] stack = new Throwable().getStackTrace();
			for (int i = 1; i < stack.length && i < 9; i++)
				b.append("\n    ").append(stack[i]);
			System.err.println(b);
		}
		return valid;
	}

	public void release() {
		surfaceTexture = null;
		imageReaderPtr = 0;
		videoEncoderPtr = 0;
		imageWriterPtr = 0;
		/* a SurfaceView's window belongs to its layer and survives this; the one
		 * ANativeWindow_fromSurface made for a layerless surface does not */
		if (nativeWindow != 0 && native_releaseWindow(nativeWindow))
			nativeWindow = 0;
	}

	private static native boolean native_releaseWindow(long window);

	/** the size the surface presents at, which is the layer's buffer size */
	public int getWidth() {
		return view != null ? view.getSurfaceWidth() : 0;
	}

	public int getHeight() {
		return view != null ? view.getSurfaceHeight() : 0;
	}

	/* called from native MediaCodec (any thread): present a decoded video frame */
	void postFrame(android.graphics.Bitmap frame) {
		if (view != null)
			view.postFrame(frame);
	}

	public static final android.os.Parcelable.Creator<android.view.Surface> CREATOR = null;

	public static final int ROTATION_0 = 0;

	public static final int ROTATION_180 = 2;

	public static final int ROTATION_270 = 3;

	public static final int ROTATION_90 = 1;

	public void writeToParcel(android.os.Parcel a0, int a1) { }
}
