package android.window;

/**
 * API 34 predictive-back callback: the animated form of
 * {@link OnBackInvokedCallback}.
 *
 * ATL runs no back gesture, so only onBackInvoked() is ever called and the
 * three animation callbacks stay at their defaults. Google Camera implements
 * this interface during Activity.onPostCreate, so a missing class here is a
 * NoClassDefFoundError that takes the rest of that callback's work with it.
 */
public interface OnBackAnimationCallback extends OnBackInvokedCallback {
	default void onBackStarted(BackEvent backEvent) {}

	default void onBackProgressed(BackEvent backEvent) {}

	default void onBackCancelled() {}
}
