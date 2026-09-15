package android.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.util.AttributeSet;
import android.view.View;

/*
 * Ported from AOSP's ProgressBar, minus the parts nothing here can carry:
 * tiling, tinting, RTL mirroring and the accessibility events.
 *
 * The one deliberate difference is the indeterminate animation. AOSP drives it
 * with an AlphaAnimation whose transformation it samples each frame; atlas's
 * android.view.animation.Animation is a stub with no getTransformation(), so
 * the level ramp is computed from the drawing clock instead. Same visual
 * result, no dependency on a framework piece that does not exist yet.
 */
public class ProgressBar extends View {

	private static final int MAX_LEVEL = 10000;

	/* android:indeterminateBehavior — "repeat" restarts the ramp, "cycle"
	 * runs it back down again. AOSP spells these Animation.RESTART/REVERSE. */
	private static final int BEHAVIOR_REPEAT = 1;
	private static final int BEHAVIOR_CYCLE = 2;

	/* kept protected and under these names because SeekBar writes them directly */
	protected int max = 100;
	protected int min = 0;
	protected int progress = 0;

	private boolean indeterminate = false;
	private boolean onlyIndeterminate = false;
	private Drawable indeterminateDrawable;
	private Drawable progressDrawable;
	private Drawable currentDrawable;

	private int duration = 4000;
	private int behavior = BEHAVIOR_REPEAT;
	private long animationStart = -1;
	private boolean hasAnimation = false;
	private boolean shouldStartAnimationDrawable = false;

	private int minWidth = 24;
	private int maxWidth = 48;
	private int minHeight = 24;
	private int maxHeight = 48;

	public ProgressBar(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
		haveCustomMeasure = false;
		TypedArray a = context.obtainStyledAttributes(attrs, com.android.internal.R.styleable.ProgressBar, defStyle, 0);

		minWidth = a.getDimensionPixelSize(com.android.internal.R.styleable.ProgressBar_minWidth, minWidth);
		maxWidth = a.getDimensionPixelSize(com.android.internal.R.styleable.ProgressBar_maxWidth, maxWidth);
		minHeight = a.getDimensionPixelSize(com.android.internal.R.styleable.ProgressBar_minHeight, minHeight);
		maxHeight = a.getDimensionPixelSize(com.android.internal.R.styleable.ProgressBar_maxHeight, maxHeight);

		behavior = a.getInt(com.android.internal.R.styleable.ProgressBar_indeterminateBehavior, behavior);
		duration = a.getInt(com.android.internal.R.styleable.ProgressBar_indeterminateDuration, duration);
		onlyIndeterminate = a.getBoolean(com.android.internal.R.styleable.ProgressBar_indeterminateOnly, onlyIndeterminate);

		Drawable progress = a.getDrawable(com.android.internal.R.styleable.ProgressBar_progressDrawable);
		if (progress != null)
			setProgressDrawable(progress);
		setIndeterminateDrawable(a.getDrawable(com.android.internal.R.styleable.ProgressBar_indeterminateDrawable));

		max = a.getInt(com.android.internal.R.styleable.ProgressBar_max, max);
		this.progress = a.getInt(com.android.internal.R.styleable.ProgressBar_progress, this.progress);

		indeterminate = onlyIndeterminate
		             || a.getBoolean(com.android.internal.R.styleable.ProgressBar_indeterminate, false)
		             || a.getBoolean(com.android.internal.R.styleable.ProgressBar_indeterminateOnly, false);
		a.recycle();

		swapCurrentDrawable(indeterminate ? indeterminateDrawable : progressDrawable);
		if (indeterminate)
			startAnimation();
		else
			refreshProgress();
		native_setIndeterminate(indeterminate);

		/* FIXME hack: NewPipe expects this to not be null, but for some reason it is */
		if (indeterminateDrawable == null)
			indeterminateDrawable = new android.graphics.drawable.ColorDrawable(0);
	}

	public ProgressBar(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	public ProgressBar(Context context) {
		this(context, null, 0);
	}

	protected void native_setProgress(long widget, float fraction) {}

	public void native_setIndeterminate(boolean indeterminate) {}

	public boolean isIndeterminate() {
		return indeterminate;
	}

	public void setIndeterminate(boolean indeterminate) {
		if ((!onlyIndeterminate || !this.indeterminate) && indeterminate != this.indeterminate) {
			this.indeterminate = indeterminate;
			if (indeterminate) {
				swapCurrentDrawable(indeterminateDrawable);
				startAnimation();
			} else {
				swapCurrentDrawable(progressDrawable);
				stopAnimation();
				refreshProgress();
			}
		}
		native_setIndeterminate(indeterminate);
	}

	public Drawable getProgressDrawable() {
		return progressDrawable;
	}

	public Drawable getIndeterminateDrawable() {
		return indeterminateDrawable;
	}

	public void setProgressDrawable(Drawable d) {
		if (progressDrawable == d)
			return;
		if (progressDrawable != null)
			progressDrawable.setCallback(null);
		progressDrawable = d;
		if (d != null) {
			d.setCallback(this);
			if (d.isStateful())
				d.setState(getDrawableState());
			updateDrawableBounds(getWidth(), getHeight());
		}
		if (!indeterminate) {
			swapCurrentDrawable(d);
			refreshProgress();
		}
		requestLayout();
		invalidate();
	}

	public void setIndeterminateDrawable(Drawable d) {
		if (indeterminateDrawable == d)
			return;
		if (indeterminateDrawable != null)
			indeterminateDrawable.setCallback(null);
		indeterminateDrawable = d;
		if (d != null) {
			d.setCallback(this);
			if (d.isStateful())
				d.setState(getDrawableState());
			updateDrawableBounds(getWidth(), getHeight());
		}
		if (indeterminate) {
			swapCurrentDrawable(d);
			startAnimation();
		}
		invalidate();
	}

	private void swapCurrentDrawable(Drawable newDrawable) {
		Drawable oldDrawable = currentDrawable;

		currentDrawable = newDrawable;
		if (oldDrawable != currentDrawable) {
			if (oldDrawable != null)
				oldDrawable.setVisible(false, false);
			if (currentDrawable != null)
				currentDrawable.setVisible(getWindowVisibility() == VISIBLE && isShown(), false);
		}
	}

	public void setMax(int max) {
		if (max < min)
			max = min;
		this.max = max;
		if (progress > max)
			progress = max;
		refreshProgress();
	}

	public int getMax() {
		return max;
	}

	/* API 26: a progress bar can start somewhere other than zero, and the
	 * fraction the widget is drawn with is relative to (max - min) */
	public void setMin(int min) {
		if (min > max)
			min = max;
		this.min = min;
		if (progress < min)
			progress = min;
		refreshProgress();
	}

	public int getMin() {
		return min;
	}

	private float fraction() {
		int span = max - min;

		return span <= 0 ? 0 : (progress - min) / (float)span;
	}

	public void setProgress(int progress) {
		if (progress > max)
			progress = max;
		else if (progress < min)
			progress = min;
		this.progress = progress;
		refreshProgress();
	}

	public void setProgress(int progress, boolean animate) {
		setProgress(progress);
	}

	public void setSecondaryProgress(int secondaryProgress) {}

	public int getProgress() {
		return progress;
	}

	public void incrementProgressBy(int diff) {
		setProgress(progress + diff);
	}

	public int getSecondaryProgress() {
		return 0;
	}

	private void refreshProgress() {
		float fraction = fraction();

		native_setProgress(widget, fraction);
		setVisualProgress(android.R.id.progress, fraction);
	}

	/* the level goes on the layer the id names, so a layer-list can keep a
	 * static background under a progress layer that moves; a drawable without
	 * that layer takes the level whole */
	private void setVisualProgress(int id, float fraction) {
		Drawable d = currentDrawable;

		if (d instanceof LayerDrawable) {
			Drawable layer = ((LayerDrawable)d).findDrawableByLayerId(id);
			if (layer != null)
				d = layer;
		}
		if (d != null)
			d.setLevel((int)(fraction * MAX_LEVEL));
		else
			invalidate();
	}

	private void startAnimation() {
		if (getVisibility() != VISIBLE || getWindowVisibility() != VISIBLE)
			return;

		if (indeterminateDrawable instanceof Animatable) {
			shouldStartAnimationDrawable = true;
			hasAnimation = false;
		} else {
			hasAnimation = true;
			animationStart = -1;
		}
		postInvalidate();
	}

	private void stopAnimation() {
		hasAnimation = false;
		if (indeterminateDrawable instanceof Animatable) {
			((Animatable)indeterminateDrawable).stop();
			shouldStartAnimationDrawable = false;
		}
		postInvalidate();
	}

	@Override
	protected void onSizeChanged(int w, int h, int oldw, int oldh) {
		updateDrawableBounds(w, h);
	}

	private void updateDrawableBounds(int w, int h) {
		/* onDraw translates to the padding box, so the bounds start at 0,0 */
		w -= paddingRight + paddingLeft;
		h -= paddingTop + paddingBottom;
		if (w < 0)
			w = 0;
		if (h < 0)
			h = 0;

		if (indeterminateDrawable != null)
			indeterminateDrawable.setBounds(0, 0, w, h);
		if (progressDrawable != null)
			progressDrawable.setBounds(0, 0, w, h);
	}

	@Override
	public void onDraw(Canvas canvas) {
		super.onDraw(canvas);
		drawTrack(canvas);
	}

	private void drawTrack(Canvas canvas) {
		Drawable d = currentDrawable;

		if (d == null)
			return;

		int saveCount = canvas.save();
		canvas.translate(paddingLeft, paddingTop);

		if (hasAnimation) {
			d.setLevel((int)(indeterminateFraction(getDrawingTime()) * MAX_LEVEL));
			postInvalidateOnAnimation();
		}
		d.draw(canvas);
		canvas.restoreToCount(saveCount);

		if (shouldStartAnimationDrawable && d instanceof Animatable) {
			((Animatable)d).start();
			shouldStartAnimationDrawable = false;
		}
	}

	/* where the indeterminate ramp stands at this instant: 0..1 repeating, or
	 * 0..1..0 when android:indeterminateBehavior is "cycle" */
	private float indeterminateFraction(long time) {
		if (duration <= 0)
			return 0f;
		if (animationStart < 0)
			animationStart = time;

		long elapsed = time - animationStart;
		float position = (elapsed % duration) / (float)duration;

		if (behavior == BEHAVIOR_CYCLE) {
			boolean backwards = ((elapsed / duration) & 1) != 0;
			if (backwards)
				position = 1f - position;
		}
		return position;
	}

	@Override
	protected void drawableStateChanged() {
		super.drawableStateChanged();
		updateDrawableState();
	}

	private void updateDrawableState() {
		int[] state = getDrawableState();
		boolean changed = false;

		if (progressDrawable != null && progressDrawable.isStateful())
			changed |= progressDrawable.setState(state);
		if (indeterminateDrawable != null && indeterminateDrawable.isStateful())
			changed |= indeterminateDrawable.setState(state);
		if (changed)
			invalidate();
	}

	@Override
	protected boolean verifyDrawable(Drawable who) {
		return who == progressDrawable || who == indeterminateDrawable || super.verifyDrawable(who);
	}

	@Override
	public void jumpDrawablesToCurrentState() {
		super.jumpDrawablesToCurrentState();
		if (progressDrawable != null)
			progressDrawable.jumpToCurrentState();
		if (indeterminateDrawable != null)
			indeterminateDrawable.jumpToCurrentState();
	}

	public void setIndeterminateTintList(ColorStateList tint) {}

	public void setIndeterminateTintMode(PorterDuff.Mode tintMode) {}

	public void setProgressTintList(ColorStateList tint) {}
}
