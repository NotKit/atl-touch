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
