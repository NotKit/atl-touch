package android.net;

import android.os.Handler;

public class ConnectivityManager {

	/* static: apps subclass it without a ConnectivityManager in hand */
	public static class NetworkCallback {
		public void onAvailable(Network network) {}
		public void onLost(Network network) {}
	
	public void onLinkPropertiesChanged(android.net.Network a0, android.net.LinkProperties a1) { }
}

	public NetworkInfo getNetworkInfo(int networkType) {
		return new NetworkInfo(nativeGetNetworkAvailable());
	}

	/* only one network is modelled here, so any Network describes the active one */
	public NetworkInfo getNetworkInfo(Network network) {
		return getActiveNetworkInfo();
	}

	public NetworkInfo getActiveNetworkInfo() {
		return new NetworkInfo(nativeGetNetworkAvailable());
	}

	public native void registerNetworkCallback(NetworkRequest request, NetworkCallback callback);

	/**
	 * Nothing can bring up a network to match a request here, so the callback
	 * is dropped rather than answered with the default network -- webrtc asks
	 * this way for a cellular network and would take the answer at its word.
	 */
	public void requestNetwork(NetworkRequest request, NetworkCallback callback) {}

	public void unregisterNetworkCallback(NetworkCallback callback) {}

	public native boolean isActiveNetworkMetered();

	protected native boolean nativeGetNetworkAvailable();

	public NetworkInfo[] getAllNetworkInfo() {
		return new NetworkInfo[] {getActiveNetworkInfo()};
	}

	public Network getActiveNetwork() {
		return new Network();
	}

	public Network[] getAllNetworks() {
		return new Network[] {getActiveNetwork()};
	}

	public NetworkCapabilities getNetworkCapabilities(Network network) {
		return null;
	}

	public void registerDefaultNetworkCallback(NetworkCallback cb, Handler hdl) {}

	public void registerDefaultNetworkCallback(NetworkCallback cb) {}

	public ProxyInfo getDefaultProxy() { return null; }

	/**
	 * The interface carrying the default route, and its addresses.
	 *
	 * Only one network is modelled here, so every Network describes that one.
	 * Returning null instead -- which is what this did -- makes a caller treat
	 * the network as unknown and ignore it: webrtc did exactly that, found
	 * nothing but loopback, and no call could allocate a port.
	 *
	 * The route is found by asking the kernel which local address it would
	 * send from. Connecting a UDP socket only sets that; no packet is sent.
	 */
	public android.net.LinkProperties getLinkProperties(android.net.Network network) {
		java.net.NetworkInterface iface = getDefaultRouteInterface();
		if (iface == null)
			return null;

		java.util.List<LinkAddress> addresses = new java.util.ArrayList<LinkAddress>();
		for (java.net.InterfaceAddress address : iface.getInterfaceAddresses()) {
			if (address.getAddress() != null)
				addresses.add(new LinkAddress(address.getAddress(), address.getNetworkPrefixLength()));
		}
		try {
			return new LinkProperties(iface.getName(), addresses);
		} catch (Throwable t) {
			return null;
		}
	}

	private static java.net.NetworkInterface getDefaultRouteInterface() {
		try (java.net.DatagramSocket socket = new java.net.DatagramSocket()) {
			socket.connect(java.net.InetAddress.getByName("8.8.8.8"), 53);
			java.net.NetworkInterface iface =
			    java.net.NetworkInterface.getByInetAddress(socket.getLocalAddress());
			if (iface != null && !iface.isLoopback())
				return iface;
		} catch (Throwable t) {
			// fall through to the scan below
		}
		/* No route, or the address is not on an interface we can name: take the
		 * first interface that is up, not loopback and has an address. */
		try {
			java.util.Enumeration<java.net.NetworkInterface> ifaces =
			    java.net.NetworkInterface.getNetworkInterfaces();
			while (ifaces != null && ifaces.hasMoreElements()) {
				java.net.NetworkInterface iface = ifaces.nextElement();
				if (iface.isUp() && !iface.isLoopback() && iface.getInetAddresses().hasMoreElements())
					return iface;
			}
		} catch (Throwable t) {
			// no interfaces to report
		}
		return null;
	}

	public static final int TYPE_BLUETOOTH = 7;

	public static final int TYPE_ETHERNET = 9;

	public static final int TYPE_MOBILE = 0;

	public static final int TYPE_WIFI = 1;

	public static final int TYPE_WIMAX = 6;

	public static final java.lang.String CONNECTIVITY_ACTION = "android.net.conn.CONNECTIVITY_CHANGE";

	public static final int RESTRICT_BACKGROUND_STATUS_DISABLED = 1;

	public static final int RESTRICT_BACKGROUND_STATUS_WHITELISTED = 2;

	public static final int RESTRICT_BACKGROUND_STATUS_ENABLED = 3;

	/**
	 * There is no Data Saver here, so background use is never restricted.
	 * Fenix's DownloadLanguagesFeature.start calls this, and the missing
	 * method escaped FragmentManager.executeOpsTogether -- the transaction
	 * that was moving the fragment to STARTED stopped there.
	 */
	public int getRestrictBackgroundStatus() { return RESTRICT_BACKGROUND_STATUS_DISABLED; }
}
