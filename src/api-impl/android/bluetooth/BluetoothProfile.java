package android.bluetooth;

import java.util.ArrayList;
import java.util.List;

public class BluetoothProfile {

	public static final int HEADSET = 1;
	public static final int A2DP = 2;
	public static final int GATT = 7;
	public static final int GATT_SERVER = 8;

	public static final int STATE_DISCONNECTED = 0;
	public static final int STATE_CONNECTING = 1;
	public static final int STATE_CONNECTED = 2;
	public static final int STATE_DISCONNECTING = 3;

	public interface ServiceListener {}

	/* no bluetooth stack, so a profile never has a device connected to it */
	public List getConnectedDevices() {
		return new ArrayList(0);
	}

	public int getConnectionState(BluetoothDevice device) {
		return STATE_DISCONNECTED;
	}
}
