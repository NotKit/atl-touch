package android.view;

import android.graphics.Insets;
import android.graphics.Path;
import android.graphics.Rect;

import java.util.Collections;
import java.util.List;

public class DisplayCutout {
	public static final DisplayCutout NO_CUTOUT = new DisplayCutout();

	public Insets getWaterfallInsets() {
		return Insets.NONE;
	}

	public int getSafeInsetBottom() {
		return 0;
	}

	public int getSafeInsetLeft() {
		return 0;
	}

	public int getSafeInsetRight() {
		return 0;
	}

	public int getSafeInsetTop() {
		return 0;
	}

	/* ATL's window has no cutout at all, so there is no path and no bounds */
	public Path getCutoutPath() {
		return null;
	}

	public List<Rect> getBoundingRects() {
		return Collections.emptyList();
	}

	public Rect getBoundingRectLeft() {
		return new Rect();
	}

	public Rect getBoundingRectTop() {
		return new Rect();
	}

	public Rect getBoundingRectRight() {
		return new Rect();
	}

	public Rect getBoundingRectBottom() {
		return new Rect();
	}
}
