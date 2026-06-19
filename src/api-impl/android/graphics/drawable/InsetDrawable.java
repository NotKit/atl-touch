package android.graphics.drawable;

import android.content.res.Resources;
import android.graphics.Rect;

public class InsetDrawable extends DrawableWrapper {

	private int insetLeft, insetTop, insetRight, insetBottom;

	public InsetDrawable(Drawable drawable, int insetLeft, int insetTop, int insetRight, int insetBottom) {
		super(drawable);
		this.insetLeft = insetLeft;
		this.insetTop = insetTop;
		this.insetRight = insetRight;
		this.insetBottom = insetBottom;
	}

	public InsetDrawable(Drawable drawable, int inset) {
		this(drawable, inset, inset, inset, inset);
	}

	InsetDrawable() {
		super(new InsetState(null, null), null);
	}

	@Override
	public boolean getPadding(Rect padding) {
		boolean pad = super.getPadding(padding); // wrapped drawable's padding
		padding.left += insetLeft;
		padding.top += insetTop;
		padding.right += insetRight;
		padding.bottom += insetBottom;
		return pad || (insetLeft | insetTop | insetRight | insetBottom) != 0;
	}

	static final class InsetState extends DrawableWrapper.DrawableWrapperState {

		InsetState(DrawableWrapperState orig, Resources res) {
			super(orig, res);
		}

		@Override
		public Drawable newDrawable(Resources res) {
			// TODO Auto-generated method stub
			throw new UnsupportedOperationException("Unimplemented method 'newDrawable'");
		}
	}
}
