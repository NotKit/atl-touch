package com.android.internal.util;

/**
 * Simple static methods to validate method arguments (subset of the AOSP
 * modules-utils Preconditions used by lifted framework sources).
 */
public class Preconditions {

	public static void checkArgument(boolean expression) {
		if (!expression)
			throw new IllegalArgumentException();
	}

	public static void checkArgument(boolean expression, final Object errorMessage) {
		if (!expression)
			throw new IllegalArgumentException(String.valueOf(errorMessage));
	}

	public static void checkArgument(boolean expression, final String messageTemplate,
	                                  final Object... messageArgs) {
		if (!expression)
			throw new IllegalArgumentException(String.format(messageTemplate, messageArgs));
	}

	public static <T> T checkNotNull(final T reference) {
		if (reference == null)
			throw new NullPointerException();
		return reference;
	}

	public static <T> T checkNotNull(final T reference, final Object errorMessage) {
		if (reference == null)
			throw new NullPointerException(String.valueOf(errorMessage));
		return reference;
	}

	public static void checkState(final boolean expression) {
		if (!expression)
			throw new IllegalStateException();
	}

	public static void checkState(final boolean expression, String message) {
		if (!expression)
			throw new IllegalStateException(message);
	}

	public static int checkArgumentNonnegative(final int value) {
		if (value < 0)
			throw new IllegalArgumentException();
		return value;
	}

	public static int checkArgumentNonnegative(final int value, final String errorMessage) {
		if (value < 0)
			throw new IllegalArgumentException(errorMessage);
		return value;
	}

	public static float checkArgumentNonNegative(final float value, final String errorMessage) {
		if (value < 0)
			throw new IllegalArgumentException(errorMessage);
		return value;
	}

	public static int checkArgumentInRange(int value, int lower, int upper, String valueName) {
		if (value < lower || value > upper)
			throw new IllegalArgumentException(String.format(
			    "%s is out of range of [%d, %d] (too %s)", valueName, lower, upper,
			    value < lower ? "low" : "high"));
		return value;
	}

	public static float checkArgumentInRange(float value, float lower, float upper, String valueName) {
		if (value < lower || value > upper)
			throw new IllegalArgumentException(String.format(
			    "%s is out of range of [%f, %f] (too %s)", valueName, lower, upper,
			    value < lower ? "low" : "high"));
		return value;
	}
}
