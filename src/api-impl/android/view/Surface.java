package android.view;

public class Surface {
	public long widget;
	/* the SurfaceView this surface presents into (null for detached surfaces) */
	SurfaceView view;
	/* struct ANativeWindow*, built by the SurfaceView when its layer came up and
	 * read by ANativeWindow_fromSurface. Guarded by this object's monitor. */
	public long nativeWindow;

	public boolean isValid() {
		return view != null || widget != 0 || nativeWindow != 0;
	}

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
}
