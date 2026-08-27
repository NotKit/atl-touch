package android.media;

public class AudioDeviceInfo {
	public static final int TYPE_AUX_LINE = 19;
	public static final int TYPE_BLE_BROADCAST = 30;
	public static final int TYPE_BLE_CENTRAL = 34;
	public static final int TYPE_BLE_CENTRAL_BROADCAST = 35;
	public static final int TYPE_BLE_HEADSET = 26;
	public static final int TYPE_BLE_HEARING_AID = 33;
	public static final int TYPE_BLE_SPEAKER = 27;
	public static final int TYPE_BLUETOOTH_A2DP = 8;
	public static final int TYPE_BLUETOOTH_SCO = 7;
	public static final int TYPE_BUILTIN_EARPIECE = 1;
	public static final int TYPE_BUILTIN_MIC = 15;
	public static final int TYPE_BUILTIN_SPEAKER = 2;
	public static final int TYPE_BUILTIN_SPEAKER_SAFE = 24;
	public static final int TYPE_BUS = 21;
	public static final int TYPE_DOCK = 13;
	public static final int TYPE_DOCK_ANALOG = 31;
	public static final int TYPE_FM = 14;
	public static final int TYPE_FM_TUNER = 16;
	public static final int TYPE_HDMI = 9;
	public static final int TYPE_HDMI_ARC = 10;
	public static final int TYPE_HDMI_EARC = 29;
	public static final int TYPE_HEARING_AID = 23;
	public static final int TYPE_IP = 20;
	public static final int TYPE_LINE_ANALOG = 5;
	public static final int TYPE_LINE_DIGITAL = 6;
	public static final int TYPE_MULTICHANNEL_GROUP = 32;
	public static final int TYPE_REMOTE_SUBMIX = 25;
	public static final int TYPE_TELEPHONY = 18;
	public static final int TYPE_TV_TUNER = 17;
	public static final int TYPE_UNKNOWN = 0;
	public static final int TYPE_USB_ACCESSORY = 12;
	public static final int TYPE_USB_DEVICE = 11;
	public static final int TYPE_USB_HEADSET = 22;
	public static final int TYPE_WIRED_HEADPHONES = 4;
	public static final int TYPE_WIRED_HEADSET = 3;

	private final int type;

	public AudioDeviceInfo() {
		this(TYPE_UNKNOWN);
	}

	public AudioDeviceInfo(int type) {
		this.type = type;
	}

	public boolean isSource() { return false; }

	public boolean isSink() { return false; }

	public CharSequence getProductName() { return ""; }

	public int getId() { return 0; }

	public int getType() { return type; }

	public int[] getChannelCounts() { return null; }

	public int[] getEncodings() { return null; }

	public int[] getSampleRates() { return null; }
}
