package android.widget;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

public class CheckedTextView extends TextView {

	public CheckedTextView(Context context) {
		super(context);
	}

	public CheckedTextView(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	public void setChecked(boolean checked) {}

	private Drawable checkMarkDrawable;

	public void setCheckMarkDrawable(Drawable d) { this.checkMarkDrawable = d; }

	public Drawable getCheckMarkDrawable() { return checkMarkDrawable; }
}
