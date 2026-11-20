package android.os;

public class BatteryManager {

	public static final java.lang.String EXTRA_LEVEL = "level";

	public static final java.lang.String EXTRA_PLUGGED = "plugged";

	public static final java.lang.String EXTRA_PRESENT = "present";

	public static final java.lang.String EXTRA_SCALE = "scale";

	public int getIntProperty(int id) {
		return -1;
	}

	public boolean isCharging() {
		return false;
	}
}
