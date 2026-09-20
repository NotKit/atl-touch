package android.app;

import android.content.Intent;

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

	/**
	 * Result of requestDismissKeyguard(). A camera app subclasses this from a
	 * method the verifier walks on first use, so the class has to exist even
	 * where the call itself is never made.
	 */
	public static abstract class KeyguardDismissCallback {
		public void onDismissError() {}

		public void onDismissSucceeded() {}

		public void onDismissCancelled() {}
	}

	/** Nothing is locked under ATL, so the dismissal has already succeeded. */
	public void requestDismissKeyguard(Activity activity, KeyguardDismissCallback callback) {
		if (callback != null)
			callback.onDismissSucceeded();
	}

	/**
	 * No lock screen credential is configured under ATL, so there is nothing to
	 * confirm. Callers treat null as "no credentials" and carry on unlocked.
	 */
	public Intent createConfirmDeviceCredentialIntent(CharSequence title, CharSequence description) {
		return null;
	}
}
