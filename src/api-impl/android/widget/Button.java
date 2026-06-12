package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

public class Button extends TextView {

	private Drawable compoundDrawableLeft;

	public Button(Context context) {
		this(context, null);
	}

	public Button(Context context, AttributeSet attributeSet) {
		this(context, attributeSet, 0);
	}

	public Button(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public Button(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);

		TypedArray a = context.obtainStyledAttributes(attrs, com.android.internal.R.styleable.TextView, 0, 0);
		if (a.hasValue(com.android.internal.R.styleable.TextView_text)) {
			setText(a.getText(com.android.internal.R.styleable.TextView_text));
		}

		a.recycle();
	}

	@Override
	public void setCompoundDrawables(Drawable left, Drawable top, Drawable right, Drawable bottom) {
		compoundDrawableLeft = left;
	}
}
