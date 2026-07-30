package android.graphics;

/**
 * API 29 canvas handed out by {@link RenderNode#beginRecording}. Recording is not
 * implemented, so drawing into one goes nowhere; apps only get here after
 * hasDisplayList() told them a node is usable.
 */
public final class RecordingCanvas extends Canvas {

	RecordingCanvas() {
		super();
	}

	@Override
	public boolean isHardwareAccelerated() {
		return true;
	}
}
