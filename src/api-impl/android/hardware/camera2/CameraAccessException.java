package android.hardware.camera2;

import android.util.AndroidException;

public class CameraAccessException extends AndroidException {
	public static final int CAMERA_IN_USE = 4;
	public static final int MAX_CAMERAS_IN_USE = 5;
	public static final int CAMERA_DISABLED = 1;
	public static final int CAMERA_DISCONNECTED = 2;
	public static final int CAMERA_ERROR = 3;

	private final int reason;

	public CameraAccessException(int problem) {
		this.reason = problem;
	}

	public CameraAccessException(int problem, String message) {
		super(message);
		this.reason = problem;
	}

	public final int getReason() {
		return reason;
	}
}
