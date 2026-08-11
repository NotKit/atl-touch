package android.os;

import android.util.AndroidRuntimeException;

public class BadParcelableException extends AndroidRuntimeException {
	public BadParcelableException(String reason) {
		super(reason);
	}

	public BadParcelableException(Exception cause) {
		super(cause.toString());
	}
}
