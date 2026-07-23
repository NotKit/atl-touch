package android.view;

import android.graphics.Canvas;

/**
 * A Canvas wrapping a native ATLCanvas that something else owns (the windowing
 * code, or a RenderNode recording that is consumed by RenderNode.end()).
 */
public class DisplayListCanvas extends Canvas {

	private final boolean recording;

	public DisplayListCanvas(long nativeCanvas) {
		this(nativeCanvas, false);
	}

	public DisplayListCanvas(long nativeCanvas, boolean recording) {
		super(nativeCanvas, false);
		this.recording = recording;
	}

	/** true for a RenderNode recording canvas (as opposed to the window's frame canvas) */
	public boolean isRecording() {
		return recording;
	}

	@Override
	public boolean isHardwareAccelerated() {
		return true;
	}
}
