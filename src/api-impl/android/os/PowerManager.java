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

	public boolean isIgnoringBatteryOptimizations(String packageName) {
		return true;
	}

	public static final int ON_AFTER_RELEASE = 536870912;

	public static final int PARTIAL_WAKE_LOCK = 1;

	public static final int SCREEN_BRIGHT_WAKE_LOCK = 10;
}
