package android.view;

import android.atl.GskCanvas;
import android.graphics.Canvas;

/**
 * ViewRootImpl: the bridge between a window (one native ATLSceneWidget) and
 * the pure-Java view hierarchy. The native side calls in for layout, drawing
 * and input; the Java side schedules frames through it.
 */
public class ViewRootImpl implements ViewParent {

	public long scene; // ATLSceneWidget*, set by native code on attach
	public Window window;
	private View view;
	private int width;
	private int height;

	public ViewRootImpl(Window window) {
		this.window = window;
	}

	public void setView(View view) {
		if (this.view == view)
			return;
		if (this.view != null)
			this.view.dispatchDetachedFromWindow();
		this.view = view;
		if (view != null) {
			view.parent = this;
			view.viewRootImpl = this;
			view.dispatchAttachedToWindow();
			requestLayout();
		}
	}

	public View getView() {
		return view;
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	/* called from native (ATLSceneWidget size_allocate) */
	protected void performLayout(int width, int height) {
		this.width = width;
		this.height = height;
		if (view == null)
			return;
		view.measure(View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
		             View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
		view.layout(0, 0, width, height);
	}

	/* called from native (ATLSceneWidget snapshot); canvas_ptr is an ATLCanvas* */
	protected void performDraw(long canvas_ptr, int width, int height) {
		if (view == null)
			return;
		if (view.isLayoutRequested() || width != this.width || height != this.height)
			performLayout(width, height);
		Canvas canvas = new GskCanvas(canvas_ptr);
		view.draw(canvas);
	}

	/* called from native (ATLSceneWidget event controllers) */
	protected boolean dispatchTouchEvent(MotionEvent event) {
		return view != null && view.dispatchTouchEvent(event);
	}

	protected boolean dispatchKeyEvent(KeyEvent event) {
		return view != null && view.dispatchKeyEvent(event);
	}

	public void invalidate() {
		if (scene != 0)
			nativeInvalidate(scene);
	}

	public void requestLayout() {
		if (scene != 0)
			nativeRequestLayout(scene);
	}

	private static native void nativeInvalidate(long scene);
	private static native void nativeRequestLayout(long scene);

	/* --- ViewParent --- */

	@Override
	public ViewParent getParent() {
		return null;
	}

	@Override
	public boolean isLayoutRequested() {
		return false;
	}

	@Override
	public void requestDisallowInterceptTouchEvent(boolean disallowIntercept) {}

	@Override
	public boolean onStartNestedScroll(View child, View target, int nestedScrollAxes) {
		return false;
	}

	@Override
	public boolean onNestedPreFling(View target, float velocityX, float velocityY) {
		return false;
	}

	@Override
	public boolean onNestedFling(View target, float velocityX, float velocityY, boolean consumed) {
		return false;
	}

	@Override
	public void onNestedScrollAccepted(View child, View target, int nestedScrollAxes) {}

	@Override
	public void onNestedPreScroll(View target, int dx, int dy, int[] consumed) {}

	@Override
	public void onNestedScroll(View target, int dxConsumed, int dyConsumed, int dxUnconsumed, int dyUnconsumed) {}

	@Override
	public void onStopNestedScroll(View target) {}

	@Override
	public void onDescendantInvalidated(View child, View target) {}
}
