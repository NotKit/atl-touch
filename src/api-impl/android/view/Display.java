package android.view;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.DisplayMetrics;

public final class Display {

	/* the window's size in pixels: what the launcher asked for until there is a
	 * window, then what the compositor actually granted */
	public static int window_width = 960;
	public static int window_height = 540;

	/**
	 * Publish the window's size. Every writer goes through here, because a size
	 * that arrives after Context's static initialiser has already built
	 * Configuration still has to reach Configuration: Resources hands it to
	 * AssetManager.setConfiguration, so resource qualifiers are chosen from it,
	 * and a screenWidthDp left at the size the launcher asked for means the app
	 * resolves resources for a screen it does not have.
	 */
	public static synchronized void setWindowSize(int width, int height) {
		if (width < 1 || height < 1)
			return;
		if (width == window_width && height == window_height)
			return;
		window_width = width;
		window_height = height;
		Context.onWindowSizeChanged();
	}

	// FIXME: what do we return here?
	// we don't want to hardcode this stuff, but at the same time the apps can cache it
	public void getMetrics(DisplayMetrics outMetrics) {
		outMetrics.widthPixels = this.window_width;
		outMetrics.heightPixels = this.window_height;
	}

	public void getRealMetrics(DisplayMetrics outMetrics) {
		getMetrics(outMetrics); // probably?
	}

	public int getWidth() {
		return window_width;
	}

	public int getHeight() {
		return window_height;
	}

	public int getRawWidth() {
		return window_width; // what's the difference?
	}

	public int getRawHeight() {
		return window_height; // what's the difference?
	}

	public int getRotation() {
		return 0 /*ROTATION_0*/;
	}

	public float getRefreshRate() {
		return 60; // FIXME
	}

	public float[] getSupportedRefreshRates() {
		return new float[] { getRefreshRate() };
	}

	public long getAppVsyncOffsetNanos() {
		return 0; // what else would we return here?
	}

	public int getDisplayId() {
		return 0;
	}

	public long getPresentationDeadlineNanos() {
		return 0; // what else...
	}

	public void getSize(Point size) {
		size.set(getWidth(), getHeight());
	}

	public void getRealSize(Point size) {
		getSize(size);
	}

	public void getRectSize(Rect rect) {
		rect.set(0, 0, getWidth(), getHeight());
	}

	public DisplayCutout getCutout() {
		return DisplayCutout.NO_CUTOUT;
	}

	public android.view.Display.HdrCapabilities getHdrCapabilities() { return null; }

	public boolean isHdr() { return false; }

	public float getHdrSdrRatio() { return 0.0f; }

	// PixelFormat.UNKNOWN made PixelFormat.getPixelFormatInfo() throw for
	// GeckoView's GeckoAppShell.getScreenDepth(). The toplevel is ARGB8888.
	public int getPixelFormat() { return android.graphics.PixelFormat.RGBA_8888; }

	public static class HdrCapabilities { 
	public float getDesiredMaxLuminance() { return 0.0f; }
}

	public static final int DEFAULT_DISPLAY = 0;
}
