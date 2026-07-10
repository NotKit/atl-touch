package android.view;

public class Surface {
	public long widget;
	/* the SurfaceView this surface presents into (null for detached surfaces) */
	SurfaceView view;

	public boolean isValid() {
		return view != null || widget != 0;
	}

	/* called from native MediaCodec (any thread): present a decoded video frame */
	void postFrame(android.graphics.Bitmap frame) {
		if (view != null)
			view.postFrame(frame);
	}
}
