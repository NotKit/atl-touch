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

	// following methods are overridden to prevent calling incompatible methods from superclasses
	@Override
	public void setOnClickListener(final OnClickListener l) {}
	@Override
	public void setTextColor(int color) {}
	@Override
	public void setTextSize(float size) {}
}
