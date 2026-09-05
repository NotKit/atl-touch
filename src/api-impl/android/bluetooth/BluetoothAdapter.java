package android.bluetooth;

import android.content.Context;

/**
 * There is no bluetooth stack behind this. The adapter answers "off" to
 * everything so callers take their no-bluetooth path instead of waiting on a
 * profile proxy that will never arrive.
 */
public class BluetoothAdapter {

	public static final int STATE_OFF = 10;
	public static final int STATE_ON = 12;

	public static BluetoothAdapter getDefaultAdapter() {
		return null;
	}

	public boolean isEnabled() {
		return false;
	}

	public int getState() {
		return STATE_OFF;
	}

	/** Returns false without ever calling the listener, so no proxy is handed out. */
	public boolean getProfileProxy(Context context, BluetoothProfile.ServiceListener listener, int profile) {
		return false;
	}

	public int getProfileConnectionState(int profile) {
		return BluetoothProfile.STATE_DISCONNECTED;
	}

	public void closeProfileProxy(int profile, BluetoothProfile proxy) {}
}
