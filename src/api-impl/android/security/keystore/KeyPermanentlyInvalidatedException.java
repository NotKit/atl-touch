package android.security.keystore;

import java.security.InvalidKeyException;

/**
 * Thrown when a key is gone for good because the enrolled biometrics changed.
 * Our keystore never invalidates anything, so nothing throws this — apps still
 * catch it.
 */
public class KeyPermanentlyInvalidatedException extends InvalidKeyException {

	public KeyPermanentlyInvalidatedException() {
		super("Key permanently invalidated");
	}

	public KeyPermanentlyInvalidatedException(String message) {
		super(message);
	}

	public KeyPermanentlyInvalidatedException(String message, Throwable cause) {
		super(message, cause);
	}
}
