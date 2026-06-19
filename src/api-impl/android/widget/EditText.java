package android.widget;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.Editable;
import android.text.SpannableStringBuilder;
import android.text.TextWatcher;
import android.util.AttributeSet;

public class EditText extends TextView {
	private String text = "";
	private CharSequence hint = "";

	public EditText(Context context) {
		super(context);
		initDefaultSize();
	}

	public EditText(Context context, AttributeSet attrs) {
		super(context, attrs);
		initDefaultSize();
	}

	public EditText(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
		initDefaultSize();
	}

	public EditText(Context context, AttributeSet attrs, int defStyle, int defStyleRes) {
		super(context, attrs, defStyle, defStyleRes);
		initDefaultSize();
	}

	/**
	 * Give the field a touch-target-sized minimum height and vertically center its
	 * content. ATL doesn't resolve the editbox/OutlinedBox style padding that would
	 * normally supply this, so without a floor a single-line field collapses to the
	 * bare text height (~23px) and is effectively invisible/unusable. Only applied
	 * when nothing more specific was set.
	 */
	private void initDefaultSize() {
		if (getMinimumHeight() <= 0)
			setMinimumHeight((int)(48 * getResources().getDisplayMetrics().density));
		setGravity(android.view.Gravity.CENTER_VERTICAL | android.view.Gravity.START);
	}

	protected String native_getText(long widget) {
		return text;
	}
	protected void native_addTextChangedListener(long widget, TextWatcher watcher) {}
	protected void native_removeTextChangedListener(long widget, TextWatcher watcher) {}
	protected void native_setOnEditorActionListener(long widget, OnEditorActionListener l) {}
	protected void native_setText(long widget, String text) {
		this.text = text == null ? "" : text;
	}
	protected void native_setHint(long widget, CharSequence s) {
		this.hint = s == null ? "" : s;
	}
	protected CharSequence native_getHint(long widget) {
		return hint;
	}

	public Editable getText() {
		CharSequence text = native_getText(widget);
		return new SpannableStringBuilder(text == null ? "" : text);
	}

	public Editable getEditableText() {
		CharSequence text = native_getText(widget);
		return new SpannableStringBuilder(text == null ? "" : text);
	}

	@Override
	public void setText(CharSequence text) {
		native_setText(widget, text == null ? "" : text.toString());
	}
	@Override
	public void setTextSize(float size) {}

	@Override
	public void removeTextChangedListener(TextWatcher watcher) {
		native_removeTextChangedListener(widget, watcher);
	}

	@Override
	public void addTextChangedListener(TextWatcher watcher) {
		native_addTextChangedListener(widget, watcher);
	}

	@Override
	public void setOnEditorActionListener(OnEditorActionListener l) {
		native_setOnEditorActionListener(widget, l);
	}

	@Override
	public void setCompoundDrawables(Drawable left, Drawable top, Drawable right, Drawable bottom) {}

	@Override
	public void setHint(CharSequence s) {
		native_setHint(widget, s == null ? "" : s.toString());
	}

	@Override
	public CharSequence getHint() {
		return native_getHint(widget);
	}

	public void selectAll() {}
}
