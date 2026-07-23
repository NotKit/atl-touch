package android.view;

public interface ViewParent {
	public abstract ViewParent getParent();

	/** AOSP software invalidation: propagate a child's damage rect to the root. */
	public void invalidateChild(View child, android.graphics.Rect dirty);

	public ViewParent invalidateChildInParent(int[] location, android.graphics.Rect dirty);

	public boolean isLayoutRequested();

	public void requestLayout();

	public void requestDisallowInterceptTouchEvent(boolean disallowIntercept);

	public abstract boolean onStartNestedScroll(View child, View target, int nestedScrollAxes);

	public boolean onNestedPreFling(View target, float velocityX, float velocityY);

	public boolean onNestedFling(View target, float velocityX, float velocityY, boolean consumed);

	public void onNestedScrollAccepted(View child, View target, int nestedScrollAxes);

	public void onNestedPreScroll(View target, int dx, int dy, int[] consumed);

	public void onNestedScroll(View target, int dxConsumed, int dyConsumed, int dxUnconsumed, int dyUnconsumed);

	public void onStopNestedScroll(View target);

	public void onDescendantInvalidated(View child, View target);
}
