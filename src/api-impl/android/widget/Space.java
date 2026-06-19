package android.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

public class Space extends View {

	public Space(Context context) {
		super(context);
	}

	public Space(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	public Space(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
	}

	/**
	 * Draw nothing.
	 */
	@Override
	public void draw(android.graphics.Canvas canvas) {}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		setMeasuredDimension(
		    getDefaultSize2(getSuggestedMinimumWidth(), widthMeasureSpec),
		    getDefaultSize2(getSuggestedMinimumHeight(), heightMeasureSpec));
	}

	/**
	 * Like {@link View#getDefaultSize(int, int)} but AT_MOST returns the
	 * desired (minimum) size rather than the full spec size, so a Space never
	 * expands to fill an AT_MOST constraint. This matches AOSP and is what lets
	 * a weight-0 spacer contribute 0 intrinsic width during measurement.
	 */
	private static int getDefaultSize2(int size, int measureSpec) {
		int specMode = MeasureSpec.getMode(measureSpec);
		int specSize = MeasureSpec.getSize(measureSpec);
		switch (specMode) {
			case MeasureSpec.UNSPECIFIED:
				return size;
			case MeasureSpec.AT_MOST:
				return Math.min(size, specSize);
			case MeasureSpec.EXACTLY:
			default:
				return specSize;
		}
	}
}
