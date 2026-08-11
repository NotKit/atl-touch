package android.hardware.camera2;

public abstract class CameraCaptureSession {

	public static abstract class CaptureCallback {
	
	public void onCaptureFailed(android.hardware.camera2.CameraCaptureSession a0, android.hardware.camera2.CaptureRequest a1, android.hardware.camera2.CaptureFailure a2) { }
}

	public static abstract class StateCallback {
	
	public void onConfigureFailed(android.hardware.camera2.CameraCaptureSession a0) { }

	public void onConfigured(android.hardware.camera2.CameraCaptureSession a0) { }
}

	public void close() { }

	public int setRepeatingRequest(android.hardware.camera2.CaptureRequest a0, android.hardware.camera2.CameraCaptureSession.CaptureCallback a1, android.os.Handler a2) throws android.hardware.camera2.CameraAccessException { return 0; }
}
