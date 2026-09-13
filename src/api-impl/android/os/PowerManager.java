package android.os;

public final class PowerManager {
	public final class WakeLock {
		private boolean held;
		private boolean refCounted = true;
		private int count;

		public void setReferenceCounted(boolean referenceCounted) {
			refCounted = referenceCounted;
		}

		public void acquire() {
			if (refCounted) {
				count++;
			}
			held = true;
		}

		public void release() {
			if (refCounted) {
				count--;
				if (count > 0)
					return;
			}
			held = false;
		}

		public void release(int flags) { release(); }

		public boolean isHeld() {
			return held;
		}

		public void acquire(long timeout) {
			acquire();
			new Thread(new Runnable() {
				public void run() {
					try {
						Thread.sleep(timeout);
					} catch (InterruptedException e) {
					}
					release();
				}
			}).start();
		}
	}

	public WakeLock newWakeLock(int levelAndFlags, String tag) {
		return new WakeLock();
	}

	public void goToSleep(long time) {
		// This requires DEVICE_POWER permission which is only possible for system processes. Any normal application gets a SecurityException.
		throw new SecurityException();
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
