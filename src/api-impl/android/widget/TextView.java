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
import android.text.StaticLayout;
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
	private boolean single_line = false;
	private int max_lines = Integer.MAX_VALUE;

	// Telegram's EditTextBoldCursor reflects on TextView.mEditor. Provide the
	// field so getDeclaredField("mEditor") resolves; its cursor reflection then
	// reads a null editor and degrades to a no-op, instead of invoking get() on
	// a null Field (which segfaults under AOT rather than throwing an NPE).
	private Object mEditor = null;

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
					setRawTextSize(aa.getDimensionPixelSize(com.android.internal.R.styleable.TextAppearance_textSize, 10));
				}
				aa.recycle();
			}
			if (a.hasValue(com.android.internal.R.styleable.TextView_textColor)) {
				setTextColor(a.getColorStateList(com.android.internal.R.styleable.TextView_textColor));
			}
			if (a.hasValue(com.android.internal.R.styleable.TextView_textSize)) {
				setRawTextSize(a.getDimensionPixelSize(com.android.internal.R.styleable.TextView_textSize, 10));
			}

			if (a.hasValue(com.android.internal.R.styleable.TextView_textStyle)) {
				int textStyle = a.getInt(com.android.internal.R.styleable.TextView_textStyle, 0);
				setTypeface(getTypeface(), textStyle);
			}

			if (a.hasValue(com.android.internal.R.styleable.TextView_textAllCaps)) {
				boolean allCaps = a.getBoolean(com.android.internal.R.styleable.TextView_textAllCaps, false);
				setAllCaps(allCaps);
			}

			if (a.hasValue(com.android.internal.R.styleable.TextView_singleLine))
				single_line = a.getBoolean(com.android.internal.R.styleable.TextView_singleLine, false);
			if (a.hasValue(com.android.internal.R.styleable.TextView_maxLines))
				max_lines = a.getInt(com.android.internal.R.styleable.TextView_maxLines, Integer.MAX_VALUE);
			if (a.hasValue(com.android.internal.R.styleable.TextView_lines))
				max_lines = a.getInt(com.android.internal.R.styleable.TextView_lines, Integer.MAX_VALUE);

			Drawable left = a.getDrawable(com.android.internal.R.styleable.TextView_drawableLeft);
			Drawable top = a.getDrawable(com.android.internal.R.styleable.TextView_drawableTop);
			Drawable right = a.getDrawable(com.android.internal.R.styleable.TextView_drawableRight);
			Drawable bottom = a.getDrawable(com.android.internal.R.styleable.TextView_drawableBottom);
			// LTR only: start/end alias left/right and win over them
			Drawable start = a.getDrawable(com.android.internal.R.styleable.TextView_drawableStart);
			Drawable end = a.getDrawable(com.android.internal.R.styleable.TextView_drawableEnd);
			if (start != null)
				left = start;
			if (end != null)
				right = end;
			if (a.hasValue(com.android.internal.R.styleable.TextView_drawableTint))
				drawableTint = a.getColorStateList(com.android.internal.R.styleable.TextView_drawableTint);
			drawablePadding = a.getDimensionPixelSize(com.android.internal.R.styleable.TextView_drawablePadding, drawablePadding);
			if (left != null || top != null || right != null || bottom != null)
				setCompoundDrawablesWithIntrinsicBounds(left, top, right, bottom);
		} catch (java.lang.Exception e) {
			System.out.println("exception while inflating TextView:");
			e.printStackTrace();
		}

		a.recycle();
	}

	private Layout text_layout;

	private float spacing_mult = 1.0f;
	private float spacing_add = 0.0f;

	private Layout makeLayout(int width) {
		BoringLayout.Metrics boring = BoringLayout.isBoring(text, paint);
		if (boring != null && (single_line || boring.width <= width)) {
			return BoringLayout.make(text, paint, Math.max(width, boring.width), getLayoutAlignment(),
			                         spacing_mult, spacing_add, boring, include_padding);
		}
		if (single_line) // don't wrap: lay out at the text's full width
			width = Math.max(width, (int)Math.ceil(Layout.getDesiredWidth(text, paint)));
		return StaticLayout.Builder.obtain(text, 0, text.length(), paint, width)
		    .setAlignment(getLayoutAlignment())
		    .setLineSpacing(spacing_add, spacing_mult)
		    .setUseLineSpacingFromFallbacks(fallbackLineSpacing)
		    .setIncludePad(include_padding)
		    .setBreakStrategy(break_strategy)
		    .setHyphenationFrequency(hyphenation_frequency)
		    .setMaxLines(single_line ? 1 : max_lines)
		    .build();
	}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		int widthMode = MeasureSpec.getMode(widthMeasureSpec);
		int widthSize = MeasureSpec.getSize(widthMeasureSpec);
		final int hPadding = getCompoundPaddingLeft() + getCompoundPaddingRight();
		final int vPadding = getCompoundPaddingTop() + getCompoundPaddingBottom();
		int desiredWidth = (int)Math.ceil(Layout.getDesiredWidth(text, paint)) + hPadding;
		int width;
		if (widthMode == MeasureSpec.EXACTLY)
			width = widthSize;
		else if (widthMode == MeasureSpec.AT_MOST)
			width = Math.min(Math.max(desiredWidth, getSuggestedMinimumWidth()), widthSize);
		else
			width = Math.max(desiredWidth, getSuggestedMinimumWidth());
		text_layout = makeLayout(Math.max(0, width - hPadding));
		/* the layout may hold more lines than are visible (maxLines/singleLine) */
		int visibleLines = Math.min(text_layout.getLineCount(), single_line ? 1 : max_lines);
		int desiredHeight = (visibleLines < text_layout.getLineCount()
		    ? text_layout.getLineTop(visibleLines) : text_layout.getHeight()) + vPadding;
		/* a left/right drawable may be taller than the text */
		if (drawableLeft != null)
			desiredHeight = Math.max(desiredHeight, drawableLeft.getBounds().height() + paddingTop + paddingBottom);
		if (drawableRight != null)
			desiredHeight = Math.max(desiredHeight, drawableRight.getBounds().height() + paddingTop + paddingBottom);
		setMeasuredDimension(width, resolveSize(Math.max(desiredHeight, getSuggestedMinimumHeight()), heightMeasureSpec));
	}

	/* draw the compound drawables like AOSP: left/right at the raw view
	 * padding, centered vertically within the content area; top/bottom
	 * centered horizontally */
	private void drawCompoundDrawables(Canvas canvas) {
		if (drawableLeft == null && drawableTop == null && drawableRight == null && drawableBottom == null)
			return;
		final int compoundPaddingLeft = getCompoundPaddingLeft();
		final int compoundPaddingTop = getCompoundPaddingTop();
		final int compoundPaddingRight = getCompoundPaddingRight();
		final int compoundPaddingBottom = getCompoundPaddingBottom();
		final int vspace = getHeight() - compoundPaddingBottom - compoundPaddingTop;
		final int hspace = getWidth() - compoundPaddingRight - compoundPaddingLeft;

		if (drawableLeft != null) {
			canvas.save();
			canvas.translate(paddingLeft,
			                 compoundPaddingTop + (vspace - drawableLeft.getBounds().height()) / 2);
			drawableLeft.draw(canvas);
			canvas.restore();
		}
		if (drawableRight != null) {
			canvas.save();
			canvas.translate(getWidth() - paddingRight - drawableRight.getBounds().width(),
			                 compoundPaddingTop + (vspace - drawableRight.getBounds().height()) / 2);
			drawableRight.draw(canvas);
			canvas.restore();
		}
		if (drawableTop != null) {
			canvas.save();
			canvas.translate(compoundPaddingLeft + (hspace - drawableTop.getBounds().width()) / 2,
			                 paddingTop);
			drawableTop.draw(canvas);
			canvas.restore();
		}
		if (drawableBottom != null) {
			canvas.save();
			canvas.translate(compoundPaddingLeft + (hspace - drawableBottom.getBounds().width()) / 2,
			                 getHeight() - paddingBottom - drawableBottom.getBounds().height());
			drawableBottom.draw(canvas);
			canvas.restore();
		}
	}

	@Override
	public void onDraw(Canvas canvas) {
		drawCompoundDrawables(canvas);
		if (text_layout == null)
			text_layout = makeLayout(Math.max(0, getWidth() - getCompoundPaddingLeft() - getCompoundPaddingRight()));
		canvas.save();
		canvas.translate(getLayoutOffsetX(), getLayoutOffsetY());
		text_layout.draw(canvas);
		canvas.restore();
	}

	/** Where onDraw() puts the text layout horizontally; subclasses place carets by it. */
	protected float getLayoutOffsetX() {
		float tx = getCompoundPaddingLeft();
		int innerWidth = getWidth() - getCompoundPaddingLeft() - getCompoundPaddingRight();
		int horizontalGravity = gravity & Gravity.HORIZONTAL_GRAVITY_MASK;
		if (horizontalGravity == Gravity.CENTER_HORIZONTAL || horizontalGravity == Gravity.RIGHT) {
			int desired = (int)Math.ceil(Layout.getDesiredWidth(text, paint));
			int free = Math.max(0, innerWidth - desired);
			tx += horizontalGravity == Gravity.CENTER_HORIZONTAL ? free / 2 : free;
		}
		return tx;
	}

	/** Where onDraw() puts the text layout vertically. */
	protected float getLayoutOffsetY() {
		return verticalOffset(getHeight());
	}

	private float verticalOffset(int height) {
		float ty = getCompoundPaddingTop();
		if (text_layout == null)
			return ty;
		int innerHeight = height - getCompoundPaddingTop() - getCompoundPaddingBottom();
		int verticalGravity = gravity & Gravity.VERTICAL_GRAVITY_MASK;
		if (verticalGravity == Gravity.CENTER_VERTICAL)
			ty += Math.max(0, (innerHeight - text_layout.getHeight()) / 2);
		else if (verticalGravity == Gravity.BOTTOM)
			ty += Math.max(0, innerHeight - text_layout.getHeight());
		return ty;
	}

	/* must mirror the vertical offsets applied in onDraw(); uses the measured
	 * height because LinearLayout asks for the baseline during its measure pass */
	@Override
	public int getBaseline() {
		if (text_layout == null)
			return super.getBaseline();
		return (int)verticalOffset(getMeasuredHeight()) + text_layout.getLineBaseline(0);
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


	public void setTextSize(float size) {
		setTextSize(TypedValue.COMPLEX_UNIT_SP, size);
	}
	public void setTextSize(int unit, float size) {
		setRawTextSize(TypedValue.applyDimension(unit, size, getResources().getDisplayMetrics()));
	}
	private void setRawTextSize(float size) {
		if (size == paint.getTextSize())
			return;
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
		if (style > 0) {
			if (tf == null) {
				tf = Typeface.defaultFromStyle(style);
			} else {
				tf = Typeface.create(tf, style);
			}

			setTypeface(tf);
			// now compute what (if any) algorithmic styling is needed
			int typefaceStyle = tf != null ? tf.getStyle() : 0;
			int need = style & ~typefaceStyle;
			paint.setFakeBoldText((need & Typeface.BOLD) != 0);
			paint.setTextSkewX((need & Typeface.ITALIC) != 0 ? -0.25f : 0);
		} else {
			// style == NORMAL: derive the normal face of the family so a
			// previously bold/italic typeface is reset, not kept as-is.
			// Material's NavigationMenuItemView unchecks items with
			// setTypeface(getTypeface(), NORMAL), passing the now-bold face.
			if (tf != null)
				tf = Typeface.create(tf, Typeface.NORMAL);
			paint.setFakeBoldText(false);
			paint.setTextSkewX(0);
			setTypeface(tf);
		}
	}
	public void setTypeface(Typeface tf) {
		if (paint.getTypeface() != tf) {
			paint.setTypeface(tf);
			text_layout = null;
			requestLayout();
			invalidate();
		}
	}
	public void setLineSpacing(float add, float mult) {
		if (spacing_add != add || spacing_mult != mult) {
			spacing_add = add;
			spacing_mult = mult;
			text_layout = null;
			requestLayout();
			invalidate();
		}
	}
	public final void setLinksClickable(boolean whether) {}

	// Whether fallback fonts (used for glyphs missing from the primary typeface,
	// e.g. emoji/CJK) may expand a line's ascent/descent so consecutive lines
	// don't overlap. AOSP defaults this true for targetSdk >= P; StaticLayout
	// honours it (see setUseLineSpacingFromFallbacks below).
	private boolean fallbackLineSpacing = true;
	public void setFallbackLineSpacing(boolean enabled) {
		if (fallbackLineSpacing != enabled) {
			fallbackLineSpacing = enabled;
			text_layout = null;
			requestLayout();
			invalidate();
		}
	}
	public boolean isFallbackLineSpacing() { return fallbackLineSpacing; }

	private int inputType = 0x00000001; // InputType.TYPE_CLASS_TEXT

	public void setInputType(int type) {
		inputType = type;
	}

	public int getInputType() {
		return inputType;
	}
	public void setFilters(InputFilter[] filters) {}
	public void setCursorVisible(boolean visible) {}

	private boolean showSoftInputOnFocus = true;
	public void setShowSoftInputOnFocus(boolean show) {
		showSoftInputOnFocus = show;
	}
	public final boolean getShowSoftInputOnFocus() {
		return showSoftInputOnFocus;
	}
	public void setImeOptions(int imeOptions) {}

	public void setImeHintLocales(android.os.LocaleList hintLocales) {}

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

	public void setSingleLine() {
		setSingleLine(true);
	}

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
				setRawTextSize(aa.getDimensionPixelSize(com.android.internal.R.styleable.TextAppearance_textSize, 10));
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

	public void setMaxLines(int maxLines) {
		if (max_lines == maxLines)
			return;
		max_lines = maxLines;
		text_layout = null;
		requestLayout();
		invalidate();
	}

	public void setMinWidth(int minWidth) {}
	public void setMaxWidth(int maxWidth) {}

	public Typeface getTypeface() {
		return paint.getTypeface();
	}

	public float getTextSize() {
		return paint.getTextSize();
	}

	public int getGravity() {
		return gravity;
	}

	/* compound drawables (AOSP TextView.Drawables, LTR-only: start==left, end==right) */
	private Drawable drawableLeft = null;
	private Drawable drawableTop = null;
	private Drawable drawableRight = null;
	private Drawable drawableBottom = null;
	private int drawablePadding = 0;
	private ColorStateList drawableTint = null;

	/* the compound padding is the view padding plus the space taken by the
	 * drawable (its bounds) and the drawable padding on that edge */
	public int getCompoundPaddingLeft() {
		return paddingLeft + (drawableLeft != null ? drawablePadding + drawableLeft.getBounds().width() : 0);
	}

	public int getCompoundPaddingRight() {
		return paddingRight + (drawableRight != null ? drawablePadding + drawableRight.getBounds().width() : 0);
	}

	public int getCompoundPaddingTop() {
		return paddingTop + (drawableTop != null ? drawablePadding + drawableTop.getBounds().height() : 0);
	}

	public int getCompoundPaddingBottom() {
		return paddingBottom + (drawableBottom != null ? drawablePadding + drawableBottom.getBounds().height() : 0);
	}

	public CharSequence getText() {
		return text;
	};

	public void setCompoundDrawablePadding(int pad) {
		if (pad != drawablePadding) {
			drawablePadding = pad;
			text_layout = null;
			requestLayout();
			invalidate();
		}
	}

	public int getCompoundDrawablePadding() {
		return drawablePadding;
	}

	private Drawable applyCompoundTint(Drawable dr) {
		if (dr != null) {
			dr.setCallback(this);
			dr.setState(getDrawableState());
			if (drawableTint != null)
				dr.setTintList(drawableTint);
		}
		return dr;
	}

	public void setCompoundDrawables(Drawable left, Drawable top, Drawable right, Drawable bottom) {
		drawableLeft = applyCompoundTint(left);
		drawableTop = applyCompoundTint(top);
		drawableRight = applyCompoundTint(right);
		drawableBottom = applyCompoundTint(bottom);
		text_layout = null;
		requestLayout();
		invalidate();
	}

	public void setCompoundDrawablesWithIntrinsicBounds(Drawable left, Drawable top, Drawable right, Drawable bottom) {
		if (left != null)
			left.setBounds(0, 0, left.getIntrinsicWidth(), left.getIntrinsicHeight());
		if (top != null)
			top.setBounds(0, 0, top.getIntrinsicWidth(), top.getIntrinsicHeight());
		if (right != null)
			right.setBounds(0, 0, right.getIntrinsicWidth(), right.getIntrinsicHeight());
		if (bottom != null)
			bottom.setBounds(0, 0, bottom.getIntrinsicWidth(), bottom.getIntrinsicHeight());
		setCompoundDrawables(left, top, right, bottom);
	}

	public void setCompoundDrawablesWithIntrinsicBounds(int left, int top, int right, int bottom) {
		final Context context = getContext();
		setCompoundDrawablesWithIntrinsicBounds(
		    left != 0 ? context.getDrawable(left) : null,
		    top != 0 ? context.getDrawable(top) : null,
		    right != 0 ? context.getDrawable(right) : null,
		    bottom != 0 ? context.getDrawable(bottom) : null);
	}

	public void setCompoundDrawablesRelativeWithIntrinsicBounds(Drawable start, Drawable top, Drawable end, Drawable bottom) {
		setCompoundDrawablesWithIntrinsicBounds(start, top, end, bottom);
	}

	public void setCompoundDrawablesRelativeWithIntrinsicBounds(int start, int top, int end, int bottom) {
		setCompoundDrawablesWithIntrinsicBounds(start, top, end, bottom);
	}

	public Drawable[] getCompoundDrawables() {
		return new Drawable[] {drawableLeft, drawableTop, drawableRight, drawableBottom};
	}

	public Drawable[] getCompoundDrawablesRelative() {
		return getCompoundDrawables();
	}

	public void setCompoundDrawableTintList(ColorStateList tint) {
		drawableTint = tint;
		if (drawableLeft != null)
			drawableLeft.setTintList(tint);
		if (drawableTop != null)
			drawableTop.setTintList(tint);
		if (drawableRight != null)
			drawableRight.setTintList(tint);
		if (drawableBottom != null)
			drawableBottom.setTintList(tint);
		invalidate();
	}

	public ColorStateList getCompoundDrawableTintList() {
		return drawableTint;
	}

	@Override
	protected void drawableStateChanged() {
		super.drawableStateChanged();
		final int[] state = getDrawableState();
		final Drawable[] drawables = {drawableLeft, drawableTop, drawableRight, drawableBottom};
		for (Drawable dr : drawables) {
			if (dr != null && dr.isStateful() && dr.setState(state))
				invalidate();
		}
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


	public final void setTransformationMethod(TransformationMethod method) {}

	public InputFilter[] getFilters() { return new InputFilter[0]; }

	public int getMaxLines() { return max_lines; }

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
			text_layout = makeLayout(Math.max(0, getWidth() - getCompoundPaddingLeft() - getCompoundPaddingRight()));
		return text_layout;
	}

	public int getCurrentTextColor() {
		return paint.getColor();
	}

	public void setSingleLine(boolean singleLine) {
		if (single_line == singleLine)
			return;
		single_line = singleLine;
		text_layout = null;
		requestLayout();
		invalidate();
	}

	public void setHint(int resId) {
		setHint(getContext().getResources().getText(resId));
	}

	public float getLetterSpacing() { return 0.f; }

	public boolean getLinksClickable() { return true; }

	public boolean isTextSelectable() { return true; }

	public void setHint(CharSequence s) {}

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

	public void setCustomInsertionActionModeCallback(ActionMode.Callback actionModeCallback) {}

	public int getExtendedPaddingTop() { return 0; }

	public void setRawInputType(int type) {}

	public TextUtils.TruncateAt getEllipsize() { return null; }

	public void setLines(int lines) {}

	public int getMinLines() {
		return -1;
	}

	public void setMinLines(int lines) {}

	public void setSelectAllOnFocus(boolean selectAllOnFocus) {}

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
