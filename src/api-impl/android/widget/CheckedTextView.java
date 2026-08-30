package android.widget;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

public class CheckedTextView extends TextView implements Checkable {

	public CheckedTextView(Context context) {
		super(context);
	}

	public CheckedTextView(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	// AppCompat/Material inflate their widgets through the three-arg form;
	// without it a themed layout dies with NoSuchMethodError halfway
	// through the FragmentManager transaction that was inflating it.
	public CheckedTextView(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public CheckedTextView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);
	}

	private boolean checked;

	public void setChecked(boolean checked) {
		this.checked = checked;
	}

	public boolean isChecked() {
		return checked;
	}

	public void toggle() {
		setChecked(!checked);
	}

	private Drawable checkMarkDrawable;

	public void setCheckMarkDrawable(Drawable d) { this.checkMarkDrawable = d; }

	public Drawable getCheckMarkDrawable() { return checkMarkDrawable; }
}
