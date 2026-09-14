package android.view;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Handler;

/**
 * Copying the pixels of a live surface into a Bitmap.
 *
 * ATL has no path from a Surface's producer back to CPU-readable pixels, so
 * every request answers ERROR_SOURCE_NO_DATA on the caller's handler - the
 * same result Android gives for a surface that has not produced a frame.
 * Apps use this for thumbnails and screenshots and are written to cope.
 */
public final class PixelCopy {

	public static final int SUCCESS = 0;
	public static final int ERROR_UNKNOWN = 1;
	public static final int ERROR_TIMEOUT = 2;
	public static final int ERROR_SOURCE_NO_DATA = 3;
	public static final int ERROR_SOURCE_INVALID = 4;
	public static final int ERROR_DESTINATION_INVALID = 5;

	public interface OnPixelCopyFinishedListener {
		void onPixelCopyFinished(int copyResult);
	}

	private PixelCopy() {
	}

	public static void request(Surface source, Bitmap dest, OnPixelCopyFinishedListener listener,
	    Handler listenerThread) {
		finish(listener, listenerThread);
	}

	public static void request(Surface source, Rect srcRect, Bitmap dest,
	    OnPixelCopyFinishedListener listener, Handler listenerThread) {
		finish(listener, listenerThread);
	}

	public static void request(SurfaceView source, Bitmap dest, OnPixelCopyFinishedListener listener,
	    Handler listenerThread) {
		finish(listener, listenerThread);
	}

	public static void request(SurfaceView source, Rect srcRect, Bitmap dest,
	    OnPixelCopyFinishedListener listener, Handler listenerThread) {
		finish(listener, listenerThread);
	}

	public static void request(Window source, Bitmap dest, OnPixelCopyFinishedListener listener,
	    Handler listenerThread) {
		finish(listener, listenerThread);
	}

	public static void request(Window source, Rect srcRect, Bitmap dest,
	    OnPixelCopyFinishedListener listener, Handler listenerThread) {
		finish(listener, listenerThread);
	}

	private static void finish(final OnPixelCopyFinishedListener listener, Handler handler) {
		if (listener == null)
			return;
		Runnable answer = new Runnable() {
			@Override
			public void run() {
				listener.onPixelCopyFinished(ERROR_SOURCE_NO_DATA);
			}
		};
		if (handler != null)
			handler.post(answer);
		else
			answer.run();
	}
}
