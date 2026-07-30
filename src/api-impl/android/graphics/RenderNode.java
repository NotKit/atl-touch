package android.graphics;

/**
 * API 29 display list. We have no display-list recording, so a node records
 * nothing and never has a display list — apps guard every use of one with
 * hasDisplayList() or an SDK_INT check and fall back to drawing directly.
 *
 * Unrelated to android.view.RenderNode, which is the framework's own handle on a
 * view's node in the scene graph.
 */
public final class RenderNode {

	private static long nextUniqueId = 1;

	private final String name;
	private final long uniqueId;
	private int left, top, right, bottom;

	public RenderNode(String name) {
		this.name = name;
		synchronized (RenderNode.class) {
			this.uniqueId = nextUniqueId++;
		}
	}

	public long getUniqueId() {
		return uniqueId;
	}

	public RecordingCanvas beginRecording(int width, int height) {
		return new RecordingCanvas();
	}

	public RecordingCanvas beginRecording() {
		return beginRecording(getWidth(), getHeight());
	}

	public void endRecording() {}

	public void discardDisplayList() {}

	public boolean hasDisplayList() {
		return false;
	}

	public boolean setPosition(int left, int top, int right, int bottom) {
		this.left = left;
		this.top = top;
		this.right = right;
		this.bottom = bottom;
		return true;
	}

	public boolean setPosition(Rect position) {
		return setPosition(position.left, position.top, position.right, position.bottom);
	}

	public int getLeft() {
		return left;
	}

	public int getTop() {
		return top;
	}

	public int getRight() {
		return right;
	}

	public int getBottom() {
		return bottom;
	}

	public int getWidth() {
		return right - left;
	}

	public int getHeight() {
		return bottom - top;
	}

	public boolean setAlpha(float alpha) {
		return true;
	}

	public boolean setTranslationX(float translationX) {
		return true;
	}

	public boolean setTranslationY(float translationY) {
		return true;
	}

	public boolean setScaleX(float scaleX) {
		return true;
	}

	public boolean setScaleY(float scaleY) {
		return true;
	}

	public boolean setClipToBounds(boolean clipToBounds) {
		return true;
	}

	public boolean setClipToOutline(boolean clipToOutline) {
		return true;
	}

	public boolean setOutline(Outline outline) {
		return true;
	}

	public boolean setRenderEffect(RenderEffect renderEffect) {
		return true;
	}

	public boolean setUseCompositingLayer(boolean forceToLayer, Paint paint) {
		return true;
	}

	public boolean setHasOverlappingRendering(boolean hasOverlappingRendering) {
		return true;
	}

	public boolean setElevation(float lift) {
		return true;
	}

	public boolean setProjectBackwards(boolean shouldProject) {
		return true;
	}

	public boolean setProjectionReceiver(boolean shouldReceive) {
		return true;
	}

	@Override
	public String toString() {
		return "RenderNode(" + name + ")";
	}
}
