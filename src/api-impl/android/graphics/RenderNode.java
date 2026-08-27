package android.graphics;

import android.view.DisplayListCanvas;

/**
 * The public API 29 RenderNode. It owns no display list of its own: everything
 * is delegated to {@link android.view.RenderNode}, which is what ATL's Skia
 * display-list machinery is built on.
 *
 * The one thing this class has to add is the position rectangle. The older
 * class folds position into translation; here they are separate properties
 * that AOSP adds together, so the composed value is what gets pushed down.
 */
public class RenderNode {

	private static long nextUniqueId = 1;

	private final android.view.RenderNode node = new android.view.RenderNode();
	private final String name;
	private final long uniqueId;

	private int left, top, right, bottom;
	private float translationX, translationY, translationZ;
	/* AOSP pivots at the node's centre until a pivot is set explicitly */
	private boolean pivotExplicitlySet = false;
	private int ambientShadowColor = 0xff000000;
	private int spotShadowColor = 0xff000000;
	private RenderEffect renderEffect;
	private boolean forceDarkAllowed = true;
	private boolean projectBackwards = false;
	private boolean projectionReceiver = false;
	private DisplayListCanvas currentRecording;

	public RenderNode(String name) {
		this.name = name;
		synchronized (RenderNode.class) {
			uniqueId = nextUniqueId++;
		}
	}

	/** @hide the node this one delegates to, for Canvas.drawRenderNode() */
	android.view.RenderNode getDelegate() {
		return node;
	}

	// --- display list ---

	public RecordingCanvas beginRecording(int width, int height) {
		if (currentRecording != null)
			throw new IllegalStateException("Recording currently in progress - missing #endRecording() call?");
		currentRecording = node.start(width, height);
		return (RecordingCanvas)currentRecording;
	}

	public RecordingCanvas beginRecording() {
		return beginRecording(getWidth(), getHeight());
	}

	public boolean isRecording() {
		return currentRecording != null;
	}

	public void endRecording() {
		if (currentRecording == null)
			throw new IllegalStateException("No recording in progress, forgot to call #beginRecording()?");
		DisplayListCanvas canvas = currentRecording;
		currentRecording = null;
		node.end(canvas);
	}

	public boolean hasDisplayList() {
		return node.isValid();
	}

	public void discardDisplayList() {
		node.discardDisplayList();
	}

	public int computeApproximateMemoryUsage() {
		return 0;
	}

	public long getUniqueId() {
		return uniqueId;
	}

	// --- position ---

	public boolean setPosition(int left, int top, int right, int bottom) {
		boolean changed = this.left != left || this.top != top
		               || this.right != right || this.bottom != bottom;
		this.left = left;
		this.top = top;
		this.right = right;
		this.bottom = bottom;
		/* sets the delegate's size, and its translation to (left, top) */
		node.setLeftTopRightBottom(left, top, right, bottom);
		syncTranslation();
		syncImplicitPivot();
		return changed;
	}

	public boolean setPosition(Rect position) {
		return setPosition(position.left, position.top, position.right, position.bottom);
	}

	public boolean offsetLeftAndRight(int offset) {
		if (offset == 0)
			return false;
		left += offset;
		right += offset;
		node.setTranslationX(left + translationX);
		return true;
	}

	public boolean offsetTopAndBottom(int offset) {
		if (offset == 0)
			return false;
		top += offset;
		bottom += offset;
		node.setTranslationY(top + translationY);
		return true;
	}

	public int getLeft() { return left; }
	public int getTop() { return top; }
	public int getRight() { return right; }
	public int getBottom() { return bottom; }
	public int getWidth() { return right - left; }
	public int getHeight() { return bottom - top; }

	private void syncTranslation() {
		node.setTranslationX(left + translationX);
		node.setTranslationY(top + translationY);
	}

	private void syncImplicitPivot() {
		if (!pivotExplicitlySet) {
			node.setPivotX(getWidth() / 2.0f);
			node.setPivotY(getHeight() / 2.0f);
		}
	}

	// --- transform ---

	public boolean setTranslationX(float translationX) {
		if (this.translationX == translationX)
			return false;
		this.translationX = translationX;
		node.setTranslationX(left + translationX);
		return true;
	}

	public float getTranslationX() { return translationX; }

	public boolean setTranslationY(float translationY) {
		if (this.translationY == translationY)
			return false;
		this.translationY = translationY;
		node.setTranslationY(top + translationY);
		return true;
	}

	public float getTranslationY() { return translationY; }

	/** Z translation is stored but not rendered: ATL draws no shadows and does no Z reordering. */
	public boolean setTranslationZ(float translationZ) {
		boolean changed = this.translationZ != translationZ;
		this.translationZ = translationZ;
		return changed;
	}

	public float getTranslationZ() { return translationZ; }

	public boolean setElevation(float elevation) { return node.setElevation(elevation); }
	public float getElevation() { return node.getElevation(); }

	public boolean setRotationZ(float rotation) { return node.setRotation(rotation); }
	public float getRotationZ() { return node.getRotation(); }

	public boolean setRotationX(float rotationX) { return node.setRotationX(rotationX); }
	public float getRotationX() { return node.getRotationX(); }

	public boolean setRotationY(float rotationY) { return node.setRotationY(rotationY); }
	public float getRotationY() { return node.getRotationY(); }

	public boolean setScaleX(float scaleX) { return node.setScaleX(scaleX); }
	public float getScaleX() { return node.getScaleX(); }

	public boolean setScaleY(float scaleY) { return node.setScaleY(scaleY); }
	public float getScaleY() { return node.getScaleY(); }

	public boolean setPivotX(float pivotX) {
		pivotExplicitlySet = true;
		return node.setPivotX(pivotX);
	}

	public float getPivotX() { return node.getPivotX(); }

	public boolean setPivotY(float pivotY) {
		pivotExplicitlySet = true;
		return node.setPivotY(pivotY);
	}

	public float getPivotY() { return node.getPivotY(); }

	public boolean isPivotExplicitlySet() { return pivotExplicitlySet; }

	public boolean resetPivot() {
		if (!pivotExplicitlySet)
			return false;
		pivotExplicitlySet = false;
		syncImplicitPivot();
		return true;
	}

	public boolean setCameraDistance(float distance) { return node.setCameraDistance(distance); }
	public float getCameraDistance() { return node.getCameraDistance(); }

	/** The transform only, without the position offset — that is the parent's business. */
	public void getMatrix(Matrix outMatrix) {
		outMatrix.reset();
		outMatrix.setTranslate(translationX, translationY);
		outMatrix.preRotate(getRotationZ(), getPivotX(), getPivotY());
		outMatrix.preScale(getScaleX(), getScaleY(), getPivotX(), getPivotY());
	}

	public void getInverseMatrix(Matrix outMatrix) {
		getMatrix(outMatrix);
		outMatrix.invert(outMatrix);
	}

	// --- appearance ---

	public boolean setAlpha(float alpha) { return node.setAlpha(alpha); }
	public float getAlpha() { return node.getAlpha(); }

	public boolean setClipToBounds(boolean clipToBounds) { return node.setClipToBounds(clipToBounds); }
	public boolean getClipToBounds() { return node.getClipToBounds(); }

	public boolean setClipToOutline(boolean clipToOutline) { return node.setClipToOutline(clipToOutline); }
	public boolean getClipToOutline() { return node.getClipToOutline(); }

	public boolean setOutline(Outline outline) { return node.setOutline(outline); }
	public boolean hasIdentityMatrix() {
		return translationX == 0 && translationY == 0
		    && getRotationZ() == 0 && getRotationX() == 0 && getRotationY() == 0
		    && getScaleX() == 1 && getScaleY() == 1;
	}

	public boolean setHasOverlappingRendering(boolean hasOverlappingRendering) {
		return node.setHasOverlappingRendering(hasOverlappingRendering);
	}

	public boolean hasOverlappingRendering() { return node.hasOverlappingRendering(); }

	/** ATL always draws a display list straight into its parent; there is no separate layer to composite. */
	public boolean setUseCompositingLayer(boolean forceToLayer, Paint paint) {
		node.setLayerPaint(paint);
		return false;
	}

	public boolean getUseCompositingLayer() { return false; }

	/** Stored only: ATL's renderer draws no elevation shadows. */
	public boolean setAmbientShadowColor(int color) {
		boolean changed = ambientShadowColor != color;
		ambientShadowColor = color;
		return changed;
	}

	public int getAmbientShadowColor() { return ambientShadowColor; }

	public boolean setSpotShadowColor(int color) {
		boolean changed = spotShadowColor != color;
		spotShadowColor = color;
		return changed;
	}

	public int getSpotShadowColor() { return spotShadowColor; }

	/** Stored only — see {@link RenderEffect}, ATL applies none of them. */
	public boolean setRenderEffect(RenderEffect renderEffect) {
		boolean changed = this.renderEffect != renderEffect;
		this.renderEffect = renderEffect;
		return changed;
	}

	public boolean setForceDarkAllowed(boolean allow) {
		boolean changed = forceDarkAllowed != allow;
		forceDarkAllowed = allow;
		return changed;
	}

	public boolean isForceDarkAllowed() { return forceDarkAllowed; }

	/** Projection is a shadow/ripple feature ATL does not implement; the flag is inert. */
	public boolean setProjectBackwards(boolean shouldProject) {
		boolean changed = projectBackwards != shouldProject;
		projectBackwards = shouldProject;
		return changed;
	}

	public boolean setProjectionReceiver(boolean shouldReceive) {
		boolean changed = projectionReceiver != shouldReceive;
		projectionReceiver = shouldReceive;
		return changed;
	}

	@Override
	public String toString() {
		return "RenderNode(" + name + ")";
	}
}
