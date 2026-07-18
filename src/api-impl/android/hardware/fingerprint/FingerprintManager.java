package android.hardware.fingerprint;

import android.os.CancellationSignal;
import android.os.Handler;

public class FingerprintManager {

	public static class CryptoObject {}

	public static class AuthenticationResult {
		public CryptoObject getCryptoObject() {
			return null;
		}
	}

	public static abstract class AuthenticationCallback {
		public void onAuthenticationError(int errorCode, CharSequence errString) {}
		public void onAuthenticationHelp(int helpCode, CharSequence helpString) {}
		public void onAuthenticationSucceeded(AuthenticationResult result) {}
		public void onAuthenticationFailed() {}
	}

	public boolean isHardwareDetected() {
		return false;
	}

	public boolean hasEnrolledFingerprints() {
		return false;
	}

	public void authenticate(CryptoObject crypto, CancellationSignal cancel, int flags, AuthenticationCallback callback, Handler handler) {}
}
