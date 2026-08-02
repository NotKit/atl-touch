package android.hardware;

public class Camera {

	public static final int CAMERA_ERROR_UNKNOWN = 1;
	public static final int CAMERA_ERROR_EVICTED = 2;
	public static final int CAMERA_ERROR_SERVER_DIED = 100;

	public interface PreviewCallback {
		void onPreviewFrame(byte[] data, Camera camera);
	}

	public interface AutoFocusCallback {
		void onAutoFocus(boolean success, Camera camera);
	}

	public interface ShutterCallback {
		void onShutter();
	}

	public interface PictureCallback {
		void onPictureTaken(byte[] data, Camera camera);
	}

	public interface ErrorCallback {
		void onError(int error, Camera camera);
	}

	public static class CameraInfo {
		public static final int CAMERA_FACING_BACK = 0;
		public static final int CAMERA_FACING_FRONT = 1;
		public int facing;
		public int orientation;
		public boolean canDisableShutterSound = true;
	}

	public static class Size {
		public int width;
		public int height;

		public Size(int w, int h) {
			width = w;
			height = h;
		}

		@Override
		public boolean equals(Object obj) {
			if (!(obj instanceof Size))
				return false;
			Size s = (Size)obj;
			return width == s.width && height == s.height;
		}

		@Override
		public int hashCode() {
			return width * 32713 + height;
		}
	}

	/** Skeleton; the key/value machinery lands with the parameter getters/setters. */
	public class Parameters {
	}

	private long nativePtr;
	private int cameraId = -1;
	private ErrorCallback errorCallback;

	private Camera(int cameraId) {
		this.cameraId = cameraId;
		this.nativePtr = native_open(cameraId);
	}

	public static int getNumberOfCameras() {
		return native_getNumberOfCameras();
	}

	public static void getCameraInfo(int cameraId, CameraInfo cameraInfo) {
		native_getCameraInfo(cameraId, cameraInfo);
	}

	public static Camera open(int cameraId) {
		Camera camera = new Camera(cameraId);
		if (camera.nativePtr == 0)
			throw new RuntimeException("Fail to connect to camera service");
		return camera;
	}

	public static Camera open() {
		int count = getNumberOfCameras();
		CameraInfo info = new CameraInfo();
		for (int i = 0; i < count; i++) {
			getCameraInfo(i, info);
			if (info.facing == CameraInfo.CAMERA_FACING_BACK)
				return open(i);
		}
		return null;
	}

	public final void release() {
		if (nativePtr != 0) {
			native_release(nativePtr);
			nativePtr = 0;
		}
	}

	public void setErrorCallback(ErrorCallback callback) {
		errorCallback = callback;
	}

	public final void startPreview() {
		if (nativePtr != 0)
			native_startPreview(nativePtr);
	}

	public final void stopPreview() {
		if (nativePtr != 0)
			native_stopPreview(nativePtr);
	}

	private static native int native_getNumberOfCameras();
	private static native void native_getCameraInfo(int cameraId, CameraInfo cameraInfo);
	private native long native_open(int cameraId);
	private native void native_release(long nativePtr);
	private native void native_startPreview(long nativePtr);
	private native void native_stopPreview(long nativePtr);
}
