package android.widget;

import android.annotation.UnsupportedAppUsage;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.graphics.Canvas;
import android.text.BoringLayout;
import android.text.Editable;
import android.text.InputFilter;
import android.text.Layout;
import android.text.SpannableStringBuilder;
import android.text.TextDirectionHeuristic;
import android.text.TextDirectionHeuristics;
import android.text.TextPaint;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.method.BaseMovementMethod;
import android.text.method.KeyListener;
import android.text.method.MovementMethod;
import android.text.method.TransformationMethod;
import android.text.style.URLSpan;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.ActionMode;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;

public class TextView extends View {
	private ColorStateList colors = new ColorStateList(new int[][] {new int[0]}, new int[1]);
	private CharSequence text = "";
	private TextPaint paint = new TextPaint();
	private boolean include_padding = false;
	private int break_strategy = 0 /*BREAK_STRATEGY_SIMPLE*/;
	private int hyphenation_frequency = 0 /*HYPHENATION_FREQUENCY_NONE*/;
	private int gravity = Gravity.TOP | Gravity.START;

	public TextView(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	public TextView(Context context) {
		this(context, null);
	}

	public TextView(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public TextView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);

		TypedArray a = context.obtainStyledAttributes(attrs, com.android.internal.R.styleable.TextView, defStyleAttr, 0);
		try {
			if (a.hasValue(com.android.internal.R.styleable.TextView_gravity)) {
				setGravity(a.getInt(com.android.internal.R.styleable.TextView_gravity, gravity));
			}
			if (a.hasValue(com.android.internal.R.styleable.TextView_text)) {
				setText(a.getText(com.android.internal.R.styleable.TextView_text));
			}
			if (a.hasValue(com.android.internal.R.styleable.TextView_hint)) {
				setHint(a.getText(com.android.internal.R.styleable.TextView_hint));
			}

			int ap = a.getResourceId(com.android.internal.R.styleable.TextView_textAppearance, -1);
			if (ap != -1) {
				TypedArray aa = context.obtainStyledAttributes(ap, com.android.internal.R.styleable.TextAppearance);
				if (aa.hasValue(com.android.internal.R.styleable.TextAppearance_textColor)) {
					setTextColor(aa.getColorStateList(com.android.internal.R.styleable.TextAppearance_textColor));
				}
				if (aa.hasValue(com.android.internal.R.styleable.TextAppearance_textSize)) {
					setTextSize(aa.getDimensionPixelSize(com.android.internal.R.styleable.TextAppearance_textSize, 10));
				}
				aa.recycle();
			}
			if (a.hasValue(com.android.internal.R.styleable.TextView_textColor)) {
				setTextColor(a.getColorStateList(com.android.internal.R.styleable.TextView_textColor));
			}
			if (a.hasValue(com.android.internal.R.styleable.TextView_textSize)) {
				setTextSize(a.getDimensionPixelSize(com.android.internal.R.styleable.TextView_textSize, 10));
			}

			if (a.hasValue(com.android.internal.R.styleable.TextView_textStyle)) {
				int textStyle = a.getInt(com.android.internal.R.styleable.TextView_textStyle, 0);
				setTypeface(getTypeface(), textStyle);
			}

			if (a.hasValue(com.android.internal.R.styleable.TextView_textAllCaps)) {
				boolean allCaps = a.getBoolean(com.android.internal.R.styleable.TextView_textAllCaps, false);
				setAllCaps(allCaps);
			}
		} catch (java.lang.Exception e) {
			System.out.println("exception while inflating TextView:");
			e.printStackTrace();
		}

		a.recycle();
	}

	private Layout text_layout;

	private Layout makeLayout(int width) {
		return new BoringLayout(text, paint, width, getLayoutAlignment(), 1, 0, new BoringLayout.Metrics(), false);
	}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		int widthMode = MeasureSpec.getMode(widthMeasureSpec);
		int widthSize = MeasureSpec.getSize(widthMeasureSpec);
		int desiredWidth = (int)Math.ceil(Layout.getDesiredWidth(text, paint)) + paddingLeft + paddingRight;
		int width;
		if (widthMode == MeasureSpec.EXACTLY)
			width = widthSize;
		else if (widthMode == MeasureSpec.AT_MOST)
			width = Math.min(Math.max(desiredWidth, getSuggestedMinimumWidth()), widthSize);
		else
			width = Math.max(desiredWidth, getSuggestedMinimumWidth());
		text_layout = makeLayout(Math.max(0, width - paddingLeft - paddingRight));
		int desiredHeight = text_layout.getHeight() + paddingTop + paddingBottom;
		setMeasuredDimension(width, resolveSize(Math.max(desiredHeight, getSuggestedMinimumHeight()), heightMeasureSpec));
	}

	@Override
	public void onDraw(Canvas canvas) {
		if (text_layout == null)
			text_layout = makeLayout(Math.max(0, getWidth() - paddingLeft - paddingRight));
		float tx = paddingLeft;
		float ty = paddingTop;
		int innerWidth = getWidth() - paddingLeft - paddingRight;
		int innerHeight = getHeight() - paddingTop - paddingBottom;
		int verticalGravity = gravity & Gravity.VERTICAL_GRAVITY_MASK;
		if (verticalGravity == Gravity.CENTER_VERTICAL)
			ty += Math.max(0, (innerHeight - text_layout.getHeight()) / 2);
		else if (verticalGravity == Gravity.BOTTOM)
			ty += Math.max(0, innerHeight - text_layout.getHeight());
		int horizontalGravity = gravity & Gravity.HORIZONTAL_GRAVITY_MASK;
		if (horizontalGravity == Gravity.CENTER_HORIZONTAL) {
			int desired = (int)Math.ceil(Layout.getDesiredWidth(text, paint));
			tx += Math.max(0, (innerWidth - desired) / 2);
		} else if (horizontalGravity == Gravity.RIGHT) {
			int desired = (int)Math.ceil(Layout.getDesiredWidth(text, paint));
			tx += Math.max(0, innerWidth - desired);
		}
		canvas.save();
		canvas.translate(tx, ty);
		text_layout.draw(canvas);
		canvas.restore();
	}

	public void setGravity(int gravity) {
		this.gravity = gravity;
		invalidate();
	}

	public void setText(int resId) {
		setText(getContext().getResources().getText(resId));
	}

	public void setText(CharSequence text) {
		setText(text, BufferType.NORMAL);
	}

	public void setText(CharSequence text, BufferType type) {
		this.text = text == null ? "" : text;
		text_layout = null;
		if (!isLayoutRequested())
			requestLayout();
		invalidate();
	}

	public void setText(char[] text, int start, int len) {
		setText(new String(text, start, len));
	}


	public void setTextSize(int unit, float size) {
		if (unit != TypedValue.COMPLEX_UNIT_SP)
			System.out.println("setTextSize called with non-SP unit (" + unit + "), we don't currently handle that");
		setTextSize(size);
	}
	public void setTextSize(float size) {
		paint.setTextSize(size);
		text_layout = null;
		requestLayout();
		invalidate();
	}

	public void setTextColor(int color) {
		paint.setColor(color);
		invalidate();
	}
	public void setTextColor(ColorStateList colors) {
		if (colors != null) {
			this.colors = colors;
			paint.setColor(colors.getDefaultColor()); // TODO: do this properly
		}
	}
	public void setTypeface(Typeface tf, int style) {
		String[] classesToRemove = {"ATL-font-bold", "ATL-font-italic"};
		native_removeClasses(widget, classesToRemove);

		switch (style) {
			case Typeface.BOLD:
				native_addClass(widget, "ATL-font-bold");
				break;
			case Typeface.ITALIC:
				native_addClass(widget, "ATL-font-italic");
				break;
			case Typeface.BOLD_ITALIC:
				native_addClass(widget, "ATL-font-bold");
				native_addClass(widget, "ATL-font-italic");
				break;
			default:
				break;
		}
	}
	public void setTypeface(Typeface tf) {}
	public void setLineSpacing(float add, float mult) {}
	public final void setLinksClickable(boolean whether) {}

	public void setInputType(int type) {}
	public void setFilters(InputFilter[] filters) {}
	public void setCursorVisible(boolean visible) {}
	public void setImeOptions(int imeOptions) {}

	public final ColorStateList getTextColors() { return colors; }
	public static ColorStateList getTextColors(Context context, TypedArray attrs) { return new ColorStateList(new int[][] {new int[0]}, new int[1]); }

	public TextPaint getPaint() {
		return paint;
	}

	public void addTextChangedListener(TextWatcher watcher) {}
	public void removeTextChangedListener(TextWatcher watcher) {}
	public void setOnEditorActionListener(TextView.OnEditorActionListener l) {}

	public TransformationMethod getTransformationMethod() {
		return null;
	}

	public void setHintTextColor(ColorStateList colorStateList) {}
	public void setHintTextColor(int i) {}
	public void setLinkTextColor(ColorStateList colorStateList) {}

	public void setSingleLine() {}
	public void setSelection(int i) {}
	public void setSelection(int i, int j) {}

	public int getSelectionStart() {
		return 0;
	}

	public int getSelectionEnd() {
		return 0;
	}

	public void setEllipsize(TextUtils.TruncateAt truncateAt) {}

	public void setTextAppearance(Context context, int appearance) {
		applyTextAppearance(context, appearance);
	}

	private void applyTextAppearance(Context context, int appearance) {
		if (appearance == 0)
			return;
		TypedArray aa = context.obtainStyledAttributes(appearance, com.android.internal.R.styleable.TextAppearance);
		try {
			if (aa.hasValue(com.android.internal.R.styleable.TextAppearance_textColor))
				setTextColor(aa.getColorStateList(com.android.internal.R.styleable.TextAppearance_textColor));
			if (aa.hasValue(com.android.internal.R.styleable.TextAppearance_textSize))
				setTextSize(aa.getDimensionPixelSize(com.android.internal.R.styleable.TextAppearance_textSize, 10));
			if (aa.hasValue(com.android.internal.R.styleable.TextAppearance_textStyle))
				setTypeface(getTypeface(), aa.getInt(com.android.internal.R.styleable.TextAppearance_textStyle, 0));
			if (aa.hasValue(com.android.internal.R.styleable.TextAppearance_textAllCaps))
				setAllCaps(aa.getBoolean(com.android.internal.R.styleable.TextAppearance_textAllCaps, false));
		} catch (java.lang.Exception e) {
			e.printStackTrace();
		} finally {
			aa.recycle();
		}
	}

	public void setMaxLines(int maxLines) {}

	public void setMinWidth(int minWidth) {}
	public void setMaxWidth(int maxWidth) {}

	public Typeface getTypeface() { return null; }

	public float getTextSize() {
		return paint.getTextSize();
	}

	public int getGravity() {
		return gravity;
	}

	public int getCompoundPaddingTop() { return 0; }
	public int getCompoundPaddingBottom() { return 0; }

	public CharSequence getText() {
		return text;
	};

	public void setCompoundDrawablePadding(int pad) {}

	protected void native_setCompoundDrawables(long widget, long left, long top, long right, long bottom) {}

	// just to prevent garbage collection while native side uses it
	private Drawable drawableLeft = null;
	private Drawable drawableTop = null;
	private Drawable drawableRight = null;
	private Drawable drawableBottom = null;

	public void setCompoundDrawables(Drawable left, Drawable top, Drawable right, Drawable bottom) {
		native_setCompoundDrawables(widget,
		                            left != null ? left.paintable : 0,
		                            top != null ? top.paintable : 0,
		                            right != null ? right.paintable : 0,
		                            bottom != null ? bottom.paintable : 0);
		drawableLeft = left;
		drawableTop = top;
		drawableRight = right;
		drawableBottom = bottom;
	}

	public void setAllCaps(boolean allCaps) {
		String[] classesToRemove = {"ATL-text-uppercase"};
		native_removeClasses(widget, classesToRemove);

		if (allCaps) {
			native_addClass(widget, "ATL-text-uppercase");
		}
	}

	public void setSaveEnabled(boolean enabled) {}

	public final void setAutoLinkMask(int mask) {}

	public void setEditableFactory(Editable.Factory factory) {}

	public KeyListener getKeyListener() { return null; }

	public int getInputType() { return 0; }

	public final void setTransformationMethod(TransformationMethod method) {}

	public InputFilter[] getFilters() { return new InputFilter[0]; }

	public int getMaxLines() { return -1; }

	public void setCompoundDrawablesRelative(Drawable start, Drawable top, Drawable end, Drawable bottom) {
		setCompoundDrawables(start, top, end, bottom);
	}

	public int getLineCount() { return 1; }

	public URLSpan[] getUrls() { return new URLSpan[0]; }

	public void setMovementMethod(MovementMethod method) {}

	public void setTextIsSelectable(boolean selectable) {}

	public MovementMethod getMovementMethod() {
		return new BaseMovementMethod();
	}

	public CharSequence getHint() { return "HINT"; }

	public int getMinHeight() { return 0; }
	public int getMinWidth() { return 0; }
	public void setMinHeight(int minHeight) {}
	public void setMaxHeight(int maxHeight) {}

	public void setHorizontallyScrolling(boolean whether) {}
	public boolean getHorizontallyScrolling() {
		return false;
	}

	public static interface OnEditorActionListener {
		public abstract boolean onEditorAction(TextView v, int actionId, KeyEvent event);
	}

	public static enum BufferType {
		EDITABLE,
		NORMAL,
		SPANNABLE,
	}

	public Layout getLayout() {
		if (text_layout == null)
			text_layout = makeLayout(Math.max(0, getWidth() - paddingLeft - paddingRight));
		return text_layout;
	}

	public int getCurrentTextColor() {
		return paint.getColor();
	}

	public void setSingleLine(boolean singleLine) {}

	public int getCompoundPaddingLeft() { return 0; }

	public int getCompoundPaddingRight() { return 0; }

	public void setHint(int resId) {
		setHint(getContext().getResources().getText(resId));
	}

	public float getLetterSpacing() { return 0.f; }

	public void setCompoundDrawablesRelativeWithIntrinsicBounds(int start, int top, int end, int bottom) {}

	public void setCompoundDrawablesRelativeWithIntrinsicBounds(Drawable start, Drawable top, Drawable end, Drawable bottom) {}

	public boolean getLinksClickable() { return true; }

	public boolean isTextSelectable() { return true; }

	public void setCompoundDrawablesWithIntrinsicBounds(int left, int top, int right, int bottom) {}

	public void setCompoundDrawablesWithIntrinsicBounds(Drawable left, Drawable top, Drawable right, Drawable bottom) {}

	public void setHint(CharSequence s) {}

	public Drawable[] getCompoundDrawablesRelative() { return new Drawable[4]; }

	public Drawable[] getCompoundDrawables() { return new Drawable[4]; }

	public void setTextAppearance(int appearance) {
		applyTextAppearance(getContext(), appearance);
	}

	public int length() {
		return getText().length();
	}

	public void setHighlightColor(int color) {}

	public Editable getEditableText() {
		return new SpannableStringBuilder(getText());
	}

	public int getMaxWidth() { return 1000; }

	public void nullLayouts() {}

	public void setLinkTextColor(int color) {}

	public void setCustomSelectionActionModeCallback(ActionMode.Callback actionModeCallback) {}

	public int getExtendedPaddingTop() { return 0; }

	public void setRawInputType(int type) {}

	public TextUtils.TruncateAt getEllipsize() { return null; }

	public void setLines(int lines) {}

	public int getMinLines() {
		return -1;
	}

	public void setMinLines(int lines) {}

	public void setSelectAllOnFocus(boolean selectAllOnFocus) {}

	public int getCompoundDrawablePadding() { return 0; }

	public int getPaintFlags() { return 0; }

	public void setPaintFlags(int flags) {}

	public int getLineHeight() {
		return 10; // FIXME
	}

	public int getMaxHeight() {
		return -1;
	}

	public boolean isAllCaps() { return false; }

	public int getAutoSizeStepGranularity() {
		return -1;
	}

	public void setAutoSizeTextTypeUniformWithPresetSizes(int[] presetSizes, int unit) {}

	public void setCompoundDrawableTintList(ColorStateList tint) {}

	public int getHyphenationFrequency() {
		return hyphenation_frequency;
	}

	public void setHyphenationFrequency(int hyphenationFrequency) {
		hyphenation_frequency = hyphenationFrequency;
	}

	public boolean getIncludeFontPadding() { return include_padding; }

	public void setIncludeFontPadding(boolean includePadding) {
		include_padding = includePadding;
	}

	public float getLineSpacingExtra() { return 0.f; }

	public float getLineSpacingMultiplier() { return 1.f; }

	public TextDirectionHeuristic getTextDirectionHeuristic() {
		return TextDirectionHeuristics.LTR;
	}

	public Bundle getInputExtras(boolean key) {
		return new Bundle();
	}

	public void setError(CharSequence error) {
		System.out.println("ERROR: " + error);
	}

	public int getTotalPaddingLeft() { return 0; }

	public int getTotalPaddingTop() { return 0; }

	public int getTotalPaddingRight() { return 0; }

	public int getTotalPaddingBottom() { return 0; }

	public int getImeOptions() { return 0; }

	public void setShadowLayer(float radius, float dx, float dy, int color) {}

	public int getBreakStrategy() {
		return break_strategy;
	}

	public void setBreakStrategy(int strategy) {
		break_strategy = strategy;
	}

	public void clearComposingText() {}

	public void setKeyListener(KeyListener keyListener) {}

	public int getAutoLinkMask() { return 0; }

	public void setWidth(int width) {}
	public void setHeight(int height) {}

	public void setFreezesText(boolean freezesText) {}

	public void setLetterSpacing(float letterSpacing) {}

	public void setMarqueeRepeatLimit(int marqueeLimit) {}

	@UnsupportedAppUsage /* androidx ACTVAutoSizeHelper seems to love this */
	/* Copyright (C) 2006 The Android Open Source Project */
	private Layout.Alignment getLayoutAlignment() {
		Layout.Alignment alignment;
		switch (getTextAlignment()) {
			case TEXT_ALIGNMENT_GRAVITY:
				switch (gravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) {
					case Gravity.START:
						alignment = Layout.Alignment.ALIGN_NORMAL;
						break;
					case Gravity.END:
						alignment = Layout.Alignment.ALIGN_OPPOSITE;
						break;
					case Gravity.LEFT:
						alignment = Layout.Alignment.ALIGN_LEFT;
						break;
					case Gravity.RIGHT:
						alignment = Layout.Alignment.ALIGN_RIGHT;
						break;
					case Gravity.CENTER_HORIZONTAL:
						alignment = Layout.Alignment.ALIGN_CENTER;
						break;
					default:
						alignment = Layout.Alignment.ALIGN_NORMAL;
						break;
				}
				break;
			case TEXT_ALIGNMENT_TEXT_START:
				alignment = Layout.Alignment.ALIGN_NORMAL;
				break;
			case TEXT_ALIGNMENT_TEXT_END:
				alignment = Layout.Alignment.ALIGN_OPPOSITE;
				break;
			case TEXT_ALIGNMENT_CENTER:
				alignment = Layout.Alignment.ALIGN_CENTER;
				break;
			case TEXT_ALIGNMENT_VIEW_START:
				alignment = (getLayoutDirection() == LAYOUT_DIRECTION_RTL) ? Layout.Alignment.ALIGN_RIGHT : Layout.Alignment.ALIGN_LEFT;
				break;
			case TEXT_ALIGNMENT_VIEW_END:
				alignment = (getLayoutDirection() == LAYOUT_DIRECTION_RTL) ? Layout.Alignment.ALIGN_LEFT : Layout.Alignment.ALIGN_RIGHT;
				break;
			case TEXT_ALIGNMENT_INHERIT:
				// This should never happen as we have already resolved the text alignment
				// but better safe than sorry so we just fall through
			default:
				alignment = Layout.Alignment.ALIGN_NORMAL;
				break;
		}
		return alignment;
	}
}
