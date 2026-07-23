package android.hardware;

public class Camera {

	public interface PreviewCallback {}

	public interface AutoFocusCallback {}

	public interface ErrorCallback {}

	public static class CameraInfo {
		public static final int CAMERA_FACING_BACK = 0;
		public static final int CAMERA_FACING_FRONT = 1;
		public int facing;
		public int orientation;
	}

	public static int getNumberOfCameras() { return 0; }

	public static void getCameraInfo(int cameraId, CameraInfo cameraInfo) {}

	public static Camera open() {
		return null;
	}

	public static Camera open(int cameraId) {
		return null;
	}

	public void setErrorCallback(ErrorCallback callback) {}
}
