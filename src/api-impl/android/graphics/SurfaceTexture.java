package android.graphics;

import android.os.Handler;

/**
 * A frame sink that hands the newest frame to a GL texture (updateTexImage) or,
 * for view compositing, to a Bitmap. Frames come from native producers such as
 * Camera.setPreviewTexture().
 *
 * Unlike AOSP this is not a real BufferQueue: unless the producer fills the
 * texture itself, the frame is a CPU upload that reaches the app's
 * GL_TEXTURE_EXTERNAL_OES texture through an EGLImage, so it is only in GL once
 * updateTexImage() runs in the caller's context.
 */
public class SurfaceTexture {

	public interface OnFrameAvailableListener {
		void onFrameAvailable(SurfaceTexture surfaceTexture);
	}

	public static class OutOfResourceException extends RuntimeException {
		public OutOfResourceException() {}

		public OutOfResourceException(String name) {
			super(name);
		}
	}

	private long nativePtr;
	private OnFrameAvailableListener frameListener;
	private Handler frameHandler;

	public SurfaceTexture(int texName) {
		this(texName, false);
	}

	public SurfaceTexture(int texName, boolean singleBufferMode) {
		nativePtr = native_create(texName, true, singleBufferMode);
	}

	/** Detached variant (webrtc SurfaceTextureHelper); needs attachToGLContext(). */
	public SurfaceTexture(boolean singleBufferMode) {
		nativePtr = native_create(0, false, singleBufferMode);
	}

	public void setOnFrameAvailableListener(OnFrameAvailableListener listener) {
		setOnFrameAvailableListener(listener, null);
	}

	/**
	 * The listener runs on the main loop; with a handler it is posted there
	 * instead, which is how apps move frames onto their own render thread.
	 */
	public void setOnFrameAvailableListener(OnFrameAvailableListener listener, Handler handler) {
		frameListener = listener;
		frameHandler = handler;
		if (nativePtr != 0)
			native_setFrameAvailableEnabled(nativePtr, listener != null);
	}

	/** called from native on the main loop, once per arriving frame */
	private void postFrameAvailableFromNative() {
		final OnFrameAvailableListener listener = frameListener;
		final Handler handler = frameHandler;

		if (listener == null)
			return;
		if (handler != null) {
			handler.post(new Runnable() {
				@Override
				public void run() {
					listener.onFrameAvailable(SurfaceTexture.this);
				}
			});
		} else {
			listener.onFrameAvailable(this);
		}
	}

	public void setDefaultBufferSize(int width, int height) {
		if (nativePtr != 0)
			native_setDefaultBufferSize(nativePtr, width, height);
	}

	/** Uploads the newest frame into the bound texture name, in the caller's GL context. */
	public void updateTexImage() {
		checkNotReleased();
		native_updateTexImage(nativePtr);
	}

	public void releaseTexImage() {
		/* nothing to hand back: the texture keeps a CPU-uploaded copy */
	}

	public void attachToGLContext(int texName) {
		checkNotReleased();
		native_attachToGLContext(nativePtr, texName);
	}

	public void detachFromGLContext() {
		checkNotReleased();
		native_attachToGLContext(nativePtr, 0);
	}

	/**
	 * Maps texture coordinates onto the frame. Always a vertical flip here:
	 * frames are uploaded top row first, which GL samples as t=1.
	 */
	public void getTransformMatrix(float[] mtx) {
		if (mtx == null || mtx.length < 16)
			throw new IllegalArgumentException("matrix must have 16 elements");
		if (nativePtr != 0)
			native_getTransformMatrix(nativePtr, mtx);
	}

	public long getTimestamp() {
		return nativePtr != 0 ? native_getTimestamp(nativePtr) : 0;
	}

	/**
	 * ATL extension: the newest frame as a Bitmap, for consumers that composite
	 * through Skia instead of GL (TextureView). Null until a frame arrives.
	 */
	public Bitmap getLatestFrameBitmap() {
		return nativePtr != 0 ? native_getLatestFrameBitmap(nativePtr) : null;
	}

	public void release() {
		if (nativePtr != 0) {
			native_release(nativePtr);
			nativePtr = 0;
		}
		frameListener = null;
		frameHandler = null;
	}

	public boolean isReleased() {
		return nativePtr == 0;
	}

	private void checkNotReleased() {
		if (nativePtr == 0)
			throw new IllegalStateException("SurfaceTexture has been released");
	}

	private native long native_create(int texName, boolean attached, boolean singleBufferMode);
	private native void native_release(long nativePtr);
	private native void native_setFrameAvailableEnabled(long nativePtr, boolean enabled);
	private native void native_setDefaultBufferSize(long nativePtr, int width, int height);
	private native void native_updateTexImage(long nativePtr);
	private native void native_attachToGLContext(long nativePtr, int texName);
	private native void native_getTransformMatrix(long nativePtr, float[] mtx);
	private native long native_getTimestamp(long nativePtr);
	private native Bitmap native_getLatestFrameBitmap(long nativePtr);
}
