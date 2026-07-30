package android.graphics;

import android.os.Handler;

/**
 * A stream of image frames as a GL texture. Nothing produces frames here (no
 * camera, no hardware video decode), so a texture stays empty and the listener
 * is never called — but views that take one have to load.
 */
public class SurfaceTexture {

	public interface OnFrameAvailableListener {
		void onFrameAvailable(SurfaceTexture surfaceTexture);
	}

	public static class OutOfResourcesException extends RuntimeException {
		public OutOfResourcesException() {}

		public OutOfResourcesException(String name) {
			super(name);
		}
	}

	private int texName;

	public SurfaceTexture(int texName) {
		this.texName = texName;
	}

	public SurfaceTexture(int texName, boolean singleBufferMode) {
		this(texName);
	}

	public SurfaceTexture(boolean singleBufferMode) {
		this(0);
	}

	public void setOnFrameAvailableListener(OnFrameAvailableListener listener) {}

	public void setOnFrameAvailableListener(OnFrameAvailableListener listener, Handler handler) {}

	public void setDefaultBufferSize(int width, int height) {}

	public void updateTexImage() {}

	public void releaseTexImage() {}

	public void detachFromGLContext() {}

	public void attachToGLContext(int texName) {
		this.texName = texName;
	}

	public void getTransformMatrix(float[] mtx) {
		// identity: nothing has transformed a frame that does not exist
		for (int i = 0; i < 16 && i < mtx.length; i++)
			mtx[i] = (i % 5 == 0) ? 1f : 0f;
	}

	public long getTimestamp() {
		return 0;
	}

	public void release() {}

	public boolean isReleased() {
		return false;
	}
}
