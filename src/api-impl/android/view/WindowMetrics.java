package android.view;

import android.graphics.Rect;

/**
 * WindowMetrics (API 30+): the bounds and insets of a window.
 * androidx.window's WindowMetricsCalculator and various size-aware libraries
 * read getBounds() at startup.
 */
public final class WindowMetrics {

	private final Rect bounds;
	private final WindowInsets windowInsets;
	private final float density;

	public WindowMetrics(Rect bounds, WindowInsets windowInsets) {
		this(bounds, windowInsets, 1.0f);
	}

	public WindowMetrics(Rect bounds, WindowInsets windowInsets, float density) {
		this.bounds = bounds;
		this.windowInsets = windowInsets;
		this.density = density;
	}

	public Rect getBounds() {
		return bounds;
	}

	public WindowInsets getWindowInsets() {
		return windowInsets;
	}

	public float getDensity() {
		return density;
	}
}
