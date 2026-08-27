package android.security;

/**
 * API 33. Nothing under ATL throws this: the AndroidKeyStore provider in
 * android.security.keystore never reports a keystore error code. The class has
 * to exist anyway because mozilla.components.lib.dataprotect.Keystore catches
 * it around KeyGenerator.generateKey(), and verifying that method needs the
 * type to resolve.
 *
 * Only the two error codes with a caller are declared, both read out of
 * android.jar rather than guessed. getRetryPolicy(), isTransientFailure() and
 * the rest of AOSP's accessors are left out until something asks for them.
 */
public class KeyStoreException extends Exception {
	public static final int ERROR_INCORRECT_USAGE = 13;
	public static final int ERROR_TOO_MANY_KEYS = 18;

	private final int numericErrorCode;

	public KeyStoreException(int errorCode, String message) {
		super(message);
		this.numericErrorCode = errorCode;
	}

	public int getNumericErrorCode() {
		return numericErrorCode;
	}
}
