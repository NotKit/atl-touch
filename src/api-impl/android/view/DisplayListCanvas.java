package android.view;

import android.graphics.Canvas;

/**
 * A Canvas wrapping a native ATLCanvas that something else owns (the windowing
 * code, or a RenderNode recording that is consumed by RenderNode.end()).
 */
public class DisplayListCanvas extends Canvas {

	public DisplayListCanvas(long nativeCanvas) {
		super(nativeCanvas, false);
	}

	@Override
	public boolean isHardwareAccelerated() {
		return true;
	}
}
