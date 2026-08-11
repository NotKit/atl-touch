package android.net;

import android.os.Handler;

public class ConnectivityManager {

	public static class NetworkCallback {
		public void onAvailable(Network network) {}
		public void onLost(Network network) {}
	
	public void onLinkPropertiesChanged(android.net.Network a0, android.net.LinkProperties a1) { }
}

	public NetworkInfo getNetworkInfo(int networkType) {
		return new NetworkInfo(nativeGetNetworkAvailable());
	}

	public NetworkInfo getActiveNetworkInfo() {
		return new NetworkInfo(nativeGetNetworkAvailable());
	}

	public native void registerNetworkCallback(NetworkRequest request, NetworkCallback callback);

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

	public android.net.LinkProperties getLinkProperties(android.net.Network a0) { return null; }

	public static final int TYPE_BLUETOOTH = 7;

	public static final int TYPE_ETHERNET = 9;

	public static final int TYPE_MOBILE = 0;

	public static final int TYPE_WIFI = 1;

	public static final int TYPE_WIMAX = 6;

	public static final java.lang.String CONNECTIVITY_ACTION = "android.net.conn.CONNECTIVITY_CHANGE";
}
