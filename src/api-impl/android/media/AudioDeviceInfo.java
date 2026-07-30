package android.media;

/** One audio endpoint. We report no devices, so instances only come from apps. */
public final class AudioDeviceInfo {
	/* type values match AOSP */
	public static final int TYPE_UNKNOWN = 0;
	public static final int TYPE_BUILTIN_EARPIECE = 1;
	public static final int TYPE_BUILTIN_SPEAKER = 2;
	public static final int TYPE_WIRED_HEADSET = 3;
	public static final int TYPE_WIRED_HEADPHONES = 4;
	public static final int TYPE_BLUETOOTH_SCO = 7;
	public static final int TYPE_BLUETOOTH_A2DP = 8;
	public static final int TYPE_USB_DEVICE = 11;
	public static final int TYPE_USB_HEADSET = 22;

	private final int type;

	public AudioDeviceInfo() {
		this(TYPE_UNKNOWN);
	}

	public AudioDeviceInfo(int type) {
		this.type = type;
	}

	public int getId() {
		return 0;
	}

	public int getType() {
		return type;
	}

	public CharSequence getProductName() {
		return "";
	}

	public boolean isSource() {
		return false;
	}

	public boolean isSink() {
		return false;
	}
}
