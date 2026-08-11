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
	}
}
