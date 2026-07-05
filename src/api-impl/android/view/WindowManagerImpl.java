package android.view;

import android.graphics.Rect;
import android.util.Slog;

public class WindowManagerImpl implements WindowManager, ViewManager {

	private static final String TAG = "WindowManagerImpl";

	public android.view.Display getDefaultDisplay() {
		return new android.view.Display();
	}

	@Override
	public WindowMetrics getCurrentWindowMetrics() {
		return new WindowMetrics(new Rect(0, 0, Display.window_width, Display.window_height), WindowInsets.CONSUMED);
	}

	@Override
	public WindowMetrics getMaximumWindowMetrics() {
		return getCurrentWindowMetrics();
	}

	@Override
	public void addView(View view, android.view.ViewGroup.LayoutParams params) {
		Slog.v(TAG, "addView(" + view + ", " + params + ") called");
		ViewRootImpl root = WindowManagerGlobal.getActiveViewRoot();
		if (root == null) {
			Slog.e(TAG, "addView: no active view root to attach to");
			return;
		}
		WindowManager.LayoutParams windowParams = params instanceof WindowManager.LayoutParams
		    ? (WindowManager.LayoutParams)params
		    : new WindowManager.LayoutParams(params.width, params.height, 0, 0, 0);
		view.setLayoutParams(windowParams);
		root.addPanel(view, windowParams, null);
	}

	@Override
	public void updateViewLayout(View view, android.view.ViewGroup.LayoutParams params) {
		Slog.v(TAG, "updateViewLayout(" + view + ", " + params + ") called");
		ViewRootImpl root = view.getViewRootImpl();
		if (root == null)
			return;
		WindowManager.LayoutParams windowParams = params instanceof WindowManager.LayoutParams
		    ? (WindowManager.LayoutParams)params
		    : new WindowManager.LayoutParams(params.width, params.height, 0, 0, 0);
		view.setLayoutParams(windowParams);
		root.updatePanel(view, windowParams);
	}

	@Override
	public void removeView(View view) {
		ViewRootImpl root = view.getViewRootImpl();
		if (root != null)
			root.removePanel(view);
	}

	@Override
	public void removeViewImmediate(View view) {
		removeView(view);
	}
}
