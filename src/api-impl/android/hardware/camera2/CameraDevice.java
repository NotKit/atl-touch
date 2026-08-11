package android.hardware.camera2;

public abstract class CameraDevice {
	public static final int AUDIO_RESTRICTION_NONE = 0;
	public static final int AUDIO_RESTRICTION_VIBRATION = 1;
	public static final int AUDIO_RESTRICTION_VIBRATION_SOUND = 3;
	public static final int TEMPLATE_MANUAL = 6;
	public static final int TEMPLATE_PREVIEW = 1;
	public static final int TEMPLATE_RECORD = 3;
	public static final int TEMPLATE_STILL_CAPTURE = 2;
	public static final int TEMPLATE_VIDEO_SNAPSHOT = 4;
	public static final int TEMPLATE_ZERO_SHUTTER_LAG = 5;

	public static abstract class StateCallback {
	
	public static final int ERROR_CAMERA_DEVICE = 4;

	public static final int ERROR_CAMERA_DISABLED = 3;

	public static final int ERROR_CAMERA_IN_USE = 1;

	public static final int ERROR_CAMERA_SERVICE = 5;

	public static final int ERROR_MAX_CAMERAS_IN_USE = 2;

	public void onClosed(android.hardware.camera2.CameraDevice a0) { }

	public void onDisconnected(android.hardware.camera2.CameraDevice a0) { }

	public void onError(android.hardware.camera2.CameraDevice a0, int a1) { }

	public void onOpened(android.hardware.camera2.CameraDevice a0) { }
}

	public android.hardware.camera2.CaptureRequest.Builder createCaptureRequest(int a0) throws android.hardware.camera2.CameraAccessException { return null; }

	public void close() { }

	public void createCaptureSession(java.util.List a0, android.hardware.camera2.CameraCaptureSession.StateCallback a1, android.os.Handler a2) throws android.hardware.camera2.CameraAccessException { }
}
