package android.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

public class ScrollView extends ViewGroup {
	private boolean fillViewport;

	public ScrollView(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public ScrollView(Context context) {
		super(context);
	}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		int width = 0;
		int height = 0;
		if (getChildCount() > 0) {
			View child = getChildAt(0);
			LayoutParams lp = child.getLayoutParams();
			int childWidthMeasureSpec = getChildMeasureSpec(widthMeasureSpec, 0, lp.width);
			int childHeightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
			child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
			width = child.getMeasuredWidth();
			height = child.getMeasuredHeight();
		}
		setMeasuredDimension(resolveSize(width, widthMeasureSpec), resolveSize(height, heightMeasureSpec));

		// When fillViewport is set, stretch a shorter child to fill the viewport
		// height (like a real ScrollView), so content laid out to the bottom edge
		// isn't crammed against the top.
		if (fillViewport && MeasureSpec.getMode(heightMeasureSpec) != MeasureSpec.UNSPECIFIED
				&& getChildCount() > 0) {
			View child = getChildAt(0);
			int desiredHeight = getMeasuredHeight() - getPaddingTop() - getPaddingBottom();
			if (child.getMeasuredHeight() < desiredHeight) {
				LayoutParams lp = child.getLayoutParams();
				int childWidthMeasureSpec = getChildMeasureSpec(widthMeasureSpec, 0, lp.width);
				int childHeightMeasureSpec = MeasureSpec.makeMeasureSpec(desiredHeight, MeasureSpec.EXACTLY);
				child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
			}
		}
	}

	@Override
	protected void onLayout(boolean changed, int l, int t, int r, int b) {
		if (getChildCount() > 0) {
			View child = getChildAt(0);
			child.layout(0, 0, child.getMeasuredWidth(), child.getMeasuredHeight());
		}
	}

	/* AOSP ScrollView extends FrameLayout, so children get MarginLayoutParams */
	@Override
	public ViewGroup.LayoutParams generateLayoutParams(AttributeSet attrs) {
		return new FrameLayout.LayoutParams(getContext(), attrs);
	}

	@Override
	protected ViewGroup.LayoutParams generateDefaultLayoutParams() {
		return new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
	}

	public void setFillViewport(boolean fillViewport) {
		if (this.fillViewport != fillViewport) {
			this.fillViewport = fillViewport;
			requestLayout();
		}
	}

	public boolean isFillViewport() {
		return fillViewport;
	}

	public boolean fullScroll(int direction) {
		return true;
	}
}
