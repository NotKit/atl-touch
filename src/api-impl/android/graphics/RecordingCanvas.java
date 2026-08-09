package android.graphics;

import android.view.DisplayListCanvas;

/**
 * The canvas handed out by {@link RenderNode#beginRecording()}. AOSP has
 * android.view.DisplayListCanvas extending this; ATL's DisplayListCanvas
 * predates it, so the inheritance runs the other way round here.
 */
public class RecordingCanvas extends DisplayListCanvas {

	/** @hide wraps a native recording ATLCanvas owned by the RenderNode */
	public RecordingCanvas(long nativeCanvas) {
		super(nativeCanvas, true);
	}
}
