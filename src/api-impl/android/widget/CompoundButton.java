package android.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

public abstract class CompoundButton extends Button implements Checkable {
	Drawable button_drawable = null;
	public Drawable mButtonDrawable; // directly accessed by androidx
	private boolean checked = false;
	private OnCheckedChangeListener on_checked_change_listener = null;

	public CompoundButton(Context context) {
		this(context, null);
	}

	public CompoundButton(Context context, AttributeSet attributeSet) {
		this(context, attributeSet, 0);
	}

	public CompoundButton(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public CompoundButton(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);
	}

	public static interface OnCheckedChangeListener {
		public void onCheckedChanged(CompoundButton buttonView, boolean isChecked);
	}

	public void setOnCheckedChangeListener(OnCheckedChangeListener listener) {
		on_checked_change_listener = listener;
	}

	@Override
	public void setChecked(boolean checked) {
		if (this.checked != checked) {
			this.checked = checked;
			if (on_checked_change_listener != null)
				on_checked_change_listener.onCheckedChanged(this, checked);
			invalidate();
		}
	}

	@Override
	public boolean isChecked() {
		return checked;
	}

	@Override
	public boolean performClick() {
		toggle();
		return super.performClick();
	}

	public void toggle() {
		setChecked(!isChecked());
	}

	// following methods are overridden to prevent calling incompatible methods from superclasses
	@Override
	public void setOnClickListener(final OnClickListener l) {}
	@Override
	public void setTextColor(int color) {}
	@Override
	public void setTextSize(float size) {}
	@Override
	public CharSequence getText() {
		return "FIXME CompoundButton.getText()";
	}

	public void setButtonTintList(ColorStateList list) {
	}

	public void setButtonDrawable(Drawable drawable) {
		button_drawable = drawable;
	}

	public Drawable getButtonDrawable() {
		return button_drawable;
	}

	public ColorStateList getButtonTintList() {
		return null;
	}

	@Override
	public void setCompoundDrawables(Drawable left, Drawable top, Drawable right, Drawable bottom) {}

	public PorterDuff.Mode getButtonTintMode() {
		return null;
	}
}
