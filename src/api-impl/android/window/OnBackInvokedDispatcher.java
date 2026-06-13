package android.window;

/**
 * API 33 predictive-back dispatcher. We don't implement predictive back yet;
 * callbacks are tracked but never invoked (the gesture/back key still routes
 * through the legacy onBackPressed path).
 */
public interface OnBackInvokedDispatcher {
	int PRIORITY_DEFAULT = 0;
	int PRIORITY_OVERLAY = 1000000;

	void registerOnBackInvokedCallback(int priority, OnBackInvokedCallback callback);

	void unregisterOnBackInvokedCallback(OnBackInvokedCallback callback);
}
