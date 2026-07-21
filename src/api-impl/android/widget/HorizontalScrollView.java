package android.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

public class HorizontalScrollView extends FrameLayout {

	public HorizontalScrollView(Context context) {
		super(context);
	}

	public HorizontalScrollView(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	private boolean mSmoothScrollingEnabled = true;

	@Override
	public void scrollTo(int x, int y) {
		// we rely on the fact that View.scrollBy calls scrollTo
		if (getChildCount() > 0) {
			View child = getChildAt(0);
			x = clamp(x, getWidth() - getPaddingRight() - getPaddingLeft(), child.getWidth());
			y = clamp(y, getHeight() - getPaddingBottom() - getPaddingTop(), child.getHeight());
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

	public void setHorizontalScrollBarEnabled(boolean enabled) {}

	public void setSmoothScrollingEnabled(boolean smoothScrollingEnabled) {
		mSmoothScrollingEnabled = smoothScrollingEnabled;
	}

	public boolean isSmoothScrollingEnabled() {
		return mSmoothScrollingEnabled;
	}

	public void smoothScrollTo(int x, int y) {}

	public void setFillViewport(boolean fillViewport) {}
}
