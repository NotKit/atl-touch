package android.net;

public final class NetworkCapabilities {
	public static final int TRANSPORT_CELLULAR = 0;
	public static final int TRANSPORT_WIFI = 1;
	public static final int TRANSPORT_BLUETOOTH = 2;
	public static final int TRANSPORT_ETHERNET = 3;
	public static final int TRANSPORT_VPN = 4;
	public static final int TRANSPORT_WIFI_AWARE = 5;
	public static final int TRANSPORT_LOWPAN = 6;
	public static final int TRANSPORT_USB = 8;

	public boolean hasCapability(int capability) {
		return false;
	}

	/* the transports carrying a network are not known, so claim none of them */
	public boolean hasTransport(int transportType) {
		return false;
	}
}
