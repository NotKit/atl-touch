package android.app;

public class KeyguardManager {
	public boolean inKeyguardRestrictedInputMode() {
		return false;
	}

	public boolean isKeyguardLocked() {
		return false;
	}

	public boolean isKeyguardSecure() {
		return true;
	}

	/**
	 * No lock screen credential is configured under ATL. androidx.biometric's
	 * BiometricManager.canAuthenticate() calls this, and Fenix's tab tray calls
	 * that from onCreateView -- the missing method aborted the whole view.
	 */
	public boolean isDeviceSecure() {
		return false;
	}

	public boolean isDeviceLocked() {
		return false;
	}
}
