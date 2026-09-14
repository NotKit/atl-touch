package android.widget;

import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.animation.AnimationUtils;

/*
 * AOSP-shaped, without the edge glow: the child is measured with an
 * UNSPECIFIED width so it can be wider than the viewport, and scrolling
 * animates through an OverScroller from computeScroll(). A subclass that
 * only overrides onLayout/onInterceptTouchEvent (Google Camera's mode
 * carousel does) gets the same geometry it would on Android.
 */
public class HorizontalScrollView extends FrameLayout {
	private static final int ANIMATED_SCROLL_GAP = 250;
	private static final float MAX_SCROLL_FACTOR = 0.5f;
	private static final int INVALID_POINTER = -1;

	private final Rect mTempRect = new Rect();
	private final OverScroller mScroller;
	private final VelocityTracker mVelocityTracker = VelocityTracker.obtain();
	private long mLastScroll;
	private int mLastMotionX;
	private int mActivePointerId = INVALID_POINTER;
	private boolean mIsBeingDragged;
	private boolean mIsLayoutDirty = true;
	private boolean mFillViewport;
	private boolean mSmoothScrollingEnabled = true;
	private final int mTouchSlop;
	private final int mMinimumVelocity;
	private final int mMaximumVelocity;

	public HorizontalScrollView(Context context) {
		this(context, null);
	}

	public HorizontalScrollView(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	public HorizontalScrollView(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public HorizontalScrollView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);
		mScroller = new OverScroller(context);
		ViewConfiguration configuration = ViewConfiguration.get(context);
		mTouchSlop = configuration.getScaledTouchSlop();
		mMinimumVelocity = configuration.getScaledMinimumFlingVelocity();
		mMaximumVelocity = configuration.getScaledMaximumFlingVelocity();
	}

	public int getMaxScrollAmount() {
		return (int)(MAX_SCROLL_FACTOR * getWidth());
	}

	public boolean isFillViewport() {
		return mFillViewport;
	}

	public void setFillViewport(boolean fillViewport) {
		if (fillViewport != mFillViewport) {
			mFillViewport = fillViewport;
			requestLayout();
		}
	}

	public boolean isSmoothScrollingEnabled() {
		return mSmoothScrollingEnabled;
	}

	public void setSmoothScrollingEnabled(boolean smoothScrollingEnabled) {
		mSmoothScrollingEnabled = smoothScrollingEnabled;
	}

	public void setHorizontalScrollBarEnabled(boolean enabled) {}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		super.onMeasure(widthMeasureSpec, heightMeasureSpec);
		if (!mFillViewport || MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.UNSPECIFIED)
			return;
		if (getChildCount() > 0) {
			View child = getChildAt(0);
			LayoutParams lp = (LayoutParams)child.getLayoutParams();
			int widthPadding = getPaddingLeft() + getPaddingRight() + lp.leftMargin + lp.rightMargin;
			int heightPadding = getPaddingTop() + getPaddingBottom() + lp.topMargin + lp.bottomMargin;
			int desiredWidth = getMeasuredWidth() - widthPadding;
			if (child.getMeasuredWidth() < desiredWidth) {
				child.measure(MeasureSpec.makeMeasureSpec(desiredWidth, MeasureSpec.EXACTLY),
				              getChildMeasureSpec(heightMeasureSpec, heightPadding, lp.height));
			}
		}
	}

	@Override
	protected void measureChild(View child, int parentWidthMeasureSpec, int parentHeightMeasureSpec) {
		ViewGroup.LayoutParams lp = child.getLayoutParams();
		int horizontalPadding = getPaddingLeft() + getPaddingRight();
		int childWidthMeasureSpec = MeasureSpec.makeMeasureSpec(
		    Math.max(0, MeasureSpec.getSize(parentWidthMeasureSpec) - horizontalPadding), MeasureSpec.UNSPECIFIED);
		int childHeightMeasureSpec = getChildMeasureSpec(parentHeightMeasureSpec,
		                                                 getPaddingTop() + getPaddingBottom(), lp.height);
		child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
	}

	@Override
	protected void measureChildWithMargins(View child, int parentWidthMeasureSpec, int widthUsed,
	                                       int parentHeightMeasureSpec, int heightUsed) {
		MarginLayoutParams lp = (MarginLayoutParams)child.getLayoutParams();
		int childHeightMeasureSpec = getChildMeasureSpec(parentHeightMeasureSpec,
		    getPaddingTop() + getPaddingBottom() + lp.topMargin + lp.bottomMargin + heightUsed, lp.height);
		int usedTotal = getPaddingLeft() + getPaddingRight() + lp.leftMargin + lp.rightMargin + widthUsed;
		int childWidthMeasureSpec = MeasureSpec.makeMeasureSpec(
		    Math.max(0, MeasureSpec.getSize(parentWidthMeasureSpec) - usedTotal), MeasureSpec.UNSPECIFIED);
		child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
	}

	@Override
	protected void onLayout(boolean changed, int l, int t, int r, int b) {
		int childWidth = 0;
		int childMargins = 0;
		if (getChildCount() > 0) {
			childWidth = getChildAt(0).getMeasuredWidth();
			LayoutParams lp = (LayoutParams)getChildAt(0).getLayoutParams();
			childMargins = lp.leftMargin + lp.rightMargin;
		}
		int available = r - l - getPaddingLeftWithForeground() - getPaddingRightWithForeground() - childMargins;
		layoutChildren(l, t, r, b, childWidth > available);
		mIsLayoutDirty = false;
		// the content may have shrunk under the current scroll position
		scrollTo(getScrollX(), getScrollY());
	}

	@Override
	public void requestLayout() {
		mIsLayoutDirty = true;
		super.requestLayout();
	}

	private int getScrollRange() {
		if (getChildCount() == 0)
			return 0;
		return Math.max(0, getChildAt(0).getWidth() - (getWidth() - getPaddingLeft() - getPaddingRight()));
	}

	@Override
	public void scrollTo(int x, int y) {
		// we rely on the fact that View.scrollBy calls scrollTo
		if (getChildCount() > 0) {
			View child = getChildAt(0);
			x = clamp(x, getWidth() - getPaddingRight() - getPaddingLeft(), child.getWidth());
			y = clamp(y, getHeight() - getPaddingBottom() - getPaddingTop(), child.getHeight());
			if (x != getScrollX() || y != getScrollY())
				super.scrollTo(x, y);
		}
	}

	private static int clamp(int n, int my, int child) {
		if (my >= child || n < 0)
			return 0;
		if ((my + n) > child)
			return child - my;
		return n;
	}

	@Override
	protected void onOverScrolled(int scrollX, int scrollY, boolean clampedX, boolean clampedY) {
		super.scrollTo(scrollX, scrollY);
	}

	private void doScrollX(int delta) {
		if (delta == 0)
			return;
		if (mSmoothScrollingEnabled)
			smoothScrollBy(delta, 0);
		else
			scrollBy(delta, 0);
	}

	public final void smoothScrollBy(int dx, int dy) {
		if (getChildCount() == 0)
			return;
		long duration = AnimationUtils.currentAnimationTimeMillis() - mLastScroll;
		if (duration > ANIMATED_SCROLL_GAP) {
			int width = getWidth() - getPaddingRight() - getPaddingLeft();
			int right = getChildAt(0).getWidth();
			int maxX = Math.max(0, right - width);
			int scrollX = getScrollX();
			dx = Math.max(0, Math.min(scrollX + dx, maxX)) - scrollX;
			mScroller.startScroll(scrollX, getScrollY(), dx, 0);
			postInvalidateOnAnimation();
		} else {
			if (!mScroller.isFinished())
				mScroller.abortAnimation();
			scrollBy(dx, dy);
		}
		mLastScroll = AnimationUtils.currentAnimationTimeMillis();
	}

	public final void smoothScrollTo(int x, int y) {
		smoothScrollBy(x - getScrollX(), y - getScrollY());
	}

	@Override
	public void computeScroll() {
		if (!mScroller.computeScrollOffset())
			return;
		int oldX = getScrollX();
		int oldY = getScrollY();
		int x = mScroller.getCurrX();
		int y = mScroller.getCurrY();
		if (oldX != x || oldY != y)
			overScrollBy(x - oldX, y - oldY, oldX, oldY, getScrollRange(), 0, 0, 0, false);
		postInvalidateOnAnimation();
	}

	public void fling(int velocityX) {
		if (getChildCount() == 0)
			return;
		int width = getWidth() - getPaddingRight() - getPaddingLeft();
		int right = getChildAt(0).getRight() - getPaddingLeft();
		int maxScroll = Math.max(0, right - width);
		mScroller.fling(getScrollX(), getScrollY(), velocityX, 0, 0, maxScroll, 0, 0, 0, 0);
		postInvalidateOnAnimation();
	}

	@Override
	protected int computeHorizontalScrollRange() {
		int contentWidth = getWidth() - getPaddingLeft() - getPaddingRight();
		if (getChildCount() == 0)
			return contentWidth;
		return getChildAt(0).getRight();
	}

	@Override
	protected int computeHorizontalScrollOffset() {
		return Math.max(0, getScrollX());
	}

	private boolean scrollRect(int direction, int left, int right) {
		int containerLeft = getScrollX();
		int containerRight = containerLeft + getWidth();
		if (left >= containerLeft && right <= containerRight)
			return false;
		doScrollX(direction == View.FOCUS_LEFT ? left - containerLeft : right - containerRight);
		return true;
	}

	public boolean pageScroll(int direction) {
		int width = getWidth();
		if (direction == View.FOCUS_RIGHT) {
			mTempRect.left = getScrollX() + width;
			if (getChildCount() > 0 && mTempRect.left + width > getChildAt(0).getRight())
				mTempRect.left = getChildAt(0).getRight() - width;
		} else {
			mTempRect.left = Math.max(0, getScrollX() - width);
		}
		mTempRect.right = mTempRect.left + width;
		return scrollRect(direction, mTempRect.left, mTempRect.right);
	}

	public boolean fullScroll(int direction) {
		int width = getWidth();
		mTempRect.left = 0;
		mTempRect.right = width;
		if (direction == View.FOCUS_RIGHT && getChildCount() > 0) {
			mTempRect.right = getChildAt(0).getRight();
			mTempRect.left = mTempRect.right - width;
		}
		return scrollRect(direction, mTempRect.left, mTempRect.right);
	}

	public boolean arrowScroll(int direction) {
		int delta = getMaxScrollAmount();
		if (direction == View.FOCUS_LEFT)
			delta = -delta;
		int target = Math.max(0, Math.min(getScrollX() + delta, getScrollRange()));
		if (target == getScrollX())
			return false;
		doScrollX(target - getScrollX());
		return true;
	}

	protected int computeScrollDeltaToGetChildRectOnScreen(Rect rect) {
		if (getChildCount() == 0)
			return 0;
		int width = getWidth();
		int screenLeft = getScrollX();
		int screenRight = screenLeft + width;
		int scrollXDelta = 0;
		if (rect.right > screenRight && rect.left > screenLeft) {
			scrollXDelta += rect.width() > width ? rect.left - screenLeft : rect.right - screenRight;
			scrollXDelta = Math.min(scrollXDelta, getChildAt(0).getRight() - screenRight);
		} else if (rect.left < screenLeft && rect.right < screenRight) {
			scrollXDelta -= rect.width() > width ? screenRight - rect.right : screenLeft - rect.left;
			scrollXDelta = Math.max(scrollXDelta, -getScrollX());
		}
		return scrollXDelta;
	}

	@Override
	public void requestChildFocus(View child, View focused) {
		if (focused != null && !mIsLayoutDirty) {
			focused.getDrawingRect(mTempRect);
			offsetDescendantRectToMyCoords(focused, mTempRect);
			int delta = computeScrollDeltaToGetChildRectOnScreen(mTempRect);
			if (delta != 0)
				scrollBy(delta, 0);
		}
		super.requestChildFocus(child, focused);
	}

	private boolean inChild(int x, int y) {
		if (getChildCount() == 0)
			return false;
		int scrollX = getScrollX();
		View child = getChildAt(0);
		return !(y < child.getTop() || y >= child.getBottom()
		         || x < child.getLeft() - scrollX || x >= child.getRight() - scrollX);
	}

	@Override
	public boolean onInterceptTouchEvent(MotionEvent ev) {
		int action = ev.getAction();
		if (action == MotionEvent.ACTION_MOVE && mIsBeingDragged)
			return true;
		switch (action & MotionEvent.ACTION_MASK) {
		case MotionEvent.ACTION_MOVE: {
			if (mActivePointerId == INVALID_POINTER)
				break;
			int pointerIndex = ev.findPointerIndex(mActivePointerId);
			if (pointerIndex == -1)
				break;
			int x = (int)ev.getX(pointerIndex);
			if (Math.abs(x - mLastMotionX) > mTouchSlop) {
				mIsBeingDragged = true;
				mLastMotionX = x;
				mVelocityTracker.addMovement(ev);
				ViewParent parent = getParent();
				if (parent != null)
					parent.requestDisallowInterceptTouchEvent(true);
			}
			break;
		}
		case MotionEvent.ACTION_DOWN: {
			int x = (int)ev.getX();
			if (!inChild(x, (int)ev.getY())) {
				mIsBeingDragged = false;
				break;
			}
			mLastMotionX = x;
			mActivePointerId = ev.getPointerId(0);
			mVelocityTracker.clear();
			mVelocityTracker.addMovement(ev);
			// a touch while flinging starts a drag straight away
			mIsBeingDragged = !mScroller.isFinished();
			break;
		}
		case MotionEvent.ACTION_CANCEL:
		case MotionEvent.ACTION_UP:
			mIsBeingDragged = false;
			mActivePointerId = INVALID_POINTER;
			if (mScroller.springBack(getScrollX(), getScrollY(), 0, getScrollRange(), 0, 0))
				postInvalidateOnAnimation();
			break;
		case MotionEvent.ACTION_POINTER_DOWN: {
			int index = ev.getActionIndex();
			mLastMotionX = (int)ev.getX(index);
			mActivePointerId = ev.getPointerId(index);
			break;
		}
		case MotionEvent.ACTION_POINTER_UP:
			onSecondaryPointerUp(ev);
			mLastMotionX = (int)ev.getX(ev.findPointerIndex(mActivePointerId));
			break;
		}
		return mIsBeingDragged;
	}

	@Override
	public boolean onTouchEvent(MotionEvent ev) {
		mVelocityTracker.addMovement(ev);
		switch (ev.getAction() & MotionEvent.ACTION_MASK) {
		case MotionEvent.ACTION_DOWN: {
			if (getChildCount() == 0)
				return false;
			if (!mScroller.isFinished()) {
				ViewParent parent = getParent();
				if (parent != null)
					parent.requestDisallowInterceptTouchEvent(true);
				mScroller.abortAnimation();
			}
			mLastMotionX = (int)ev.getX();
			mActivePointerId = ev.getPointerId(0);
			break;
		}
		case MotionEvent.ACTION_MOVE: {
			int activePointerIndex = ev.findPointerIndex(mActivePointerId);
			if (activePointerIndex == -1)
				break;
			int x = (int)ev.getX(activePointerIndex);
			int deltaX = mLastMotionX - x;
			if (!mIsBeingDragged && Math.abs(deltaX) > mTouchSlop) {
				ViewParent parent = getParent();
				if (parent != null)
					parent.requestDisallowInterceptTouchEvent(true);
				mIsBeingDragged = true;
				deltaX += deltaX > 0 ? -mTouchSlop : mTouchSlop;
			}
			if (mIsBeingDragged) {
				mLastMotionX = x;
				overScrollBy(deltaX, 0, getScrollX(), 0, getScrollRange(), 0, 0, 0, true);
			}
			break;
		}
		case MotionEvent.ACTION_UP:
			if (mIsBeingDragged) {
				mVelocityTracker.computeCurrentVelocity(1000, mMaximumVelocity);
				int initialVelocity = (int)mVelocityTracker.getXVelocity(mActivePointerId);
				if (getChildCount() > 0) {
					if (Math.abs(initialVelocity) > mMinimumVelocity)
						fling(-initialVelocity);
					else if (mScroller.springBack(getScrollX(), getScrollY(), 0, getScrollRange(), 0, 0))
						postInvalidateOnAnimation();
				}
				mActivePointerId = INVALID_POINTER;
				mIsBeingDragged = false;
			}
			break;
		case MotionEvent.ACTION_CANCEL:
			if (mIsBeingDragged && getChildCount() > 0) {
				if (mScroller.springBack(getScrollX(), getScrollY(), 0, getScrollRange(), 0, 0))
					postInvalidateOnAnimation();
				mActivePointerId = INVALID_POINTER;
				mIsBeingDragged = false;
			}
			break;
		case MotionEvent.ACTION_POINTER_UP:
			onSecondaryPointerUp(ev);
			break;
		}
		return true;
	}

	private void onSecondaryPointerUp(MotionEvent ev) {
		int pointerIndex = ev.getActionIndex();
		if (ev.getPointerId(pointerIndex) == mActivePointerId) {
			int newPointerIndex = pointerIndex == 0 ? 1 : 0;
			mLastMotionX = (int)ev.getX(newPointerIndex);
			mActivePointerId = ev.getPointerId(newPointerIndex);
			mVelocityTracker.clear();
		}
	}

	@Override
	public boolean shouldDelayChildPressedState() {
		return true;
	}
}
