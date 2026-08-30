package android.widget;

import android.content.Context;
import android.util.AttributeSet;

public class RadioButton extends CompoundButton {

	public RadioButton(Context context) {
		super(context);
	}

	public RadioButton(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	// AppCompat/Material inflate their widgets through the three-arg form;
	// without it a themed layout dies with NoSuchMethodError halfway
	// through the FragmentManager transaction that was inflating it.
	public RadioButton(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public RadioButton(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);
	}

	// following methods are overridden to prevent calling incompatible methods from superclasses
	@Override
	public void setOnClickListener(final OnClickListener l) {}
	@Override
	public void setTextColor(int color) {}
	@Override
	public void setTextSize(float size) {}
}
