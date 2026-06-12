package android.widget;

import android.content.Context;
import android.util.AttributeSet;

public class CheckBox extends CompoundButton {

	public CheckBox(Context context) {
		super(context);
	}

	public CheckBox(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	public void setLines(int lines) {}

	@Override
	public void setText(CharSequence text) {
		super.setText(text);
	}

	// following methods are overridden to prevent calling incompatible methods from superclasses
	@Override
	public void setOnClickListener(final OnClickListener l) {}
	@Override
	public void setTextColor(int color) {}
	@Override
	public void setTextSize(float size) {}
}
