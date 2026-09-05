package android.net.wifi;

/** Nothing polls a wifi link here, so everything measured about it is unknown. */
public class WifiInfo {
	public static final int INVALID_RSSI = -127;
	public static final int LINK_SPEED_UNKNOWN = -1;

	public int getRssi() {
		return INVALID_RSSI;
	}

	public int getLinkSpeed() {
		return LINK_SPEED_UNKNOWN;
	}

	public String getSSID() {
		return "<unknown ssid>";
	}

	public String getMacAddress() {
		return "";
	}

	public String getBSSID() {
		return "";
	}
}
