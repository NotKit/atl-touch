package android.os;

public final class PowerManager {
	public final class WakeLock {
		public void setReferenceCounted(boolean referenceCounted) {}

		public void acquire() {}

		public void release() {}

		/* there are no release flags to honour, so the flags are dropped */
		public void release(int flags) {
			release();
		}

		public boolean isHeld() {
			return false;
		}

		public void acquire(long timeout) {}
	}

	public WakeLock newWakeLock(int levelAndFlags, String tag) {
		return new WakeLock();
	}

	public void userActivity(long dummy, boolean dummy2) {}

	public static final int FULL_WAKE_LOCK = 0x1a;

	public boolean isPowerSaveMode() {
		return false;
	}

	public boolean isScreenOn() {
		return true;
	}

	/* nothing here tracks a screen-off state, so the device is always interactive */
	public boolean isInteractive() {
		return true;
	}

	public boolean isSustainedPerformanceModeSupported() {
		return false;
	}

	public boolean isIgnoringBatteryOptimizations(String packageName) {
		return true;
	}

	public static final int ON_AFTER_RELEASE = 536870912;

	public static final int PARTIAL_WAKE_LOCK = 1;

	public static final int SCREEN_BRIGHT_WAKE_LOCK = 10;
	/* ATL reads no thermal sensor: the status is always "none" and the listener
	 * is never called. Apps register one before opening the camera. */
	public static final int THERMAL_STATUS_NONE = 0;
	public static final int THERMAL_STATUS_LIGHT = 1;
	public static final int THERMAL_STATUS_MODERATE = 2;
	public static final int THERMAL_STATUS_SEVERE = 3;
	public static final int THERMAL_STATUS_CRITICAL = 4;
	public static final int THERMAL_STATUS_EMERGENCY = 5;
	public static final int THERMAL_STATUS_SHUTDOWN = 6;

	public interface OnThermalStatusChangedListener {
		void onThermalStatusChanged(int status);
	}

	public int getCurrentThermalStatus() {
		return THERMAL_STATUS_NONE;
	}

	public void addThermalStatusListener(java.util.concurrent.Executor executor,
	    OnThermalStatusChangedListener listener) {}

	public void addThermalStatusListener(OnThermalStatusChangedListener listener) {}

	public void removeThermalStatusListener(OnThermalStatusChangedListener listener) {}
}
