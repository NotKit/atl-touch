package android.widget;

import android.content.Context;
import android.util.AttributeSet;

public class HorizontalScrollView extends FrameLayout {

	public HorizontalScrollView(Context context) {
		super(context);
	}

	public HorizontalScrollView(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	private boolean mSmoothScrollingEnabled = true;

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
