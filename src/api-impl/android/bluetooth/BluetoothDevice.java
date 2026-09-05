package android.bluetooth;

/**
 * A remote bluetooth device. Nothing here ever produces one -- there is no
 * bluetooth stack behind the adapter -- so this only exists to give the apps
 * that walk a (always empty) device list a type to name.
 */
public class BluetoothDevice {

	public static final int BOND_NONE = 10;
	public static final int BOND_BONDING = 11;
	public static final int BOND_BONDED = 12;

	public String getName() {
		return null;
	}

	public String getAddress() {
		return null;
	}

	public int getBondState() {
		return BOND_NONE;
	}

	public String toString() {
		return String.valueOf(getAddress());
	}
}
