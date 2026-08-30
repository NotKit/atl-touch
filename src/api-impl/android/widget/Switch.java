package android.widget;

import android.content.Context;
import android.util.AttributeSet;

public class Switch extends CompoundButton {

	public Switch(Context context) {
		super(context);
	}

	public Switch(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	// AppCompat/Material inflate their widgets through the three-arg form;
	// without it a themed layout dies with NoSuchMethodError halfway
	// through the FragmentManager transaction that was inflating it.
	public Switch(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public Switch(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);
	}

	public void setTextOn(CharSequence text) {}
	public void setTextOff(CharSequence text) {}
}
