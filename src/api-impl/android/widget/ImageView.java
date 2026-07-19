package android.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ColorFilter;
import android.graphics.Matrix;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.View;

public class ImageView extends View {

	private Bitmap bitmap = null;
	private ScaleType scaleType = ScaleType.FIT_CENTER;
	private Drawable drawable = null;
	private ColorFilter colorFilter = null;

	public ImageView(Context context) {
		this(context, null);
	}

	public ImageView(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	public ImageView(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public ImageView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);

		haveCustomMeasure = false;
		TypedArray a = context.obtainStyledAttributes(attrs, com.android.internal.R.styleable.ImageView, defStyleAttr, defStyleRes);
		if (a.hasValue(com.android.internal.R.styleable.ImageView_tint))
			colorFilter = new PorterDuffColorFilter(a.getColor(com.android.internal.R.styleable.ImageView_tint, 0),
			                                        PorterDuff.Mode.values()[a.getInt(com.android.internal.R.styleable.ImageView_tintMode, PorterDuff.Mode.SRC_IN.nativeInt)]);
		drawable = a.getDrawable(com.android.internal.R.styleable.ImageView_src);
		if (drawable != null) {
			drawable.setCallback(this);
			if (colorFilter != null)
				drawable.setColorFilter(colorFilter);
		}
		if (a.hasValue(com.android.internal.R.styleable.ImageView_scaleType))
			setScaleType(scaletype_from_int[a.getInt(com.android.internal.R.styleable.ImageView_scaleType, 3 /*CENTER*/)]);
		a.recycle();
	}

	public void setImageResource(final int resid) {
		if (Context.this_application.getResources().getString(resid).endsWith(".xml")) {
			setImageDrawable(getContext().getDrawable(resid));
			return;
		}
		bitmap = BitmapFactory.decodeResource(Context.this_application.getResources(), resid);
		requestLayout();
		invalidate();
	}
	public void setAdjustViewBounds(boolean adjustViewBounds) {}

	public void setScaleType(ScaleType scaleType) {
		this.scaleType = scaleType;
		invalidate();
	}

	public ScaleType getScaleType() {
		return scaleType;
	}

	public Drawable getDrawable() {
		return drawable;
	}

	public void setImageDrawable(Drawable drawable) {
		if (drawable != null && colorFilter != null) {
			drawable = drawable.mutate();
			drawable.setColorFilter(colorFilter);
		}
		if (this.drawable != null) {
			this.drawable.setCallback(null);
		}
		this.drawable = drawable;
		if (drawable != null) {
			drawable.setCallback(this);
		}
		bitmap = null;
		requestLayout();
		invalidate();
	}

	public void setImageMatrix(Matrix matrix) {}

	public void setImageBitmap(Bitmap bitmap) {
		this.bitmap = bitmap;
		this.drawable = null;
		requestLayout();
		invalidate();
	}

	/**
	 * Options for scaling the bounds of an image to the bounds of this view.
	 */
	public enum ScaleType {
		/**
		 * Scale using the image matrix when drawing. The image matrix can be set using
		 * {@link ImageView#setImageMatrix(Matrix)}. From XML, use this syntax:
		 * <code>android:scaleType="matrix"</code>.
		 */
		MATRIX(0),
		/**
		 * Scale the image using {@link Matrix.ScaleToFit#FILL}.
		 * From XML, use this syntax: <code>android:scaleType="fitXY"</code>.
		 */
		FIT_XY(1),
		/**
		 * Scale the image using {@link Matrix.ScaleToFit#START}.
		 * From XML, use this syntax: <code>android:scaleType="fitStart"</code>.
		 */
		FIT_START(2),
		/**
		 * Scale the image using {@link Matrix.ScaleToFit#CENTER}.
		 * From XML, use this syntax:
		 * <code>android:scaleType="fitCenter"</code>.
		 */
		FIT_CENTER(3),
		/**
		 * Scale the image using {@link Matrix.ScaleToFit#END}.
		 * From XML, use this syntax: <code>android:scaleType="fitEnd"</code>.
		 */
		FIT_END(4),
		/**
		 * Center the image in the view, but perform no scaling.
		 * From XML, use this syntax: <code>android:scaleType="center"</code>.
		 */
		CENTER(5),
		/**
		 * Scale the image uniformly (maintain the image's aspect ratio) so
		 * that both dimensions (width and height) of the image will be equal
		 * to or larger than the corresponding dimension of the view
		 * (minus padding). The image is then centered in the view.
		 * From XML, use this syntax: <code>android:scaleType="centerCrop"</code>.
		 */
		CENTER_CROP(6),
		/**
		 * Scale the image uniformly (maintain the image's aspect ratio) so
		 * that both dimensions (width and height) of the image will be equal
		 * to or less than the corresponding dimension of the view
		 * (minus padding). The image is then centered in the view.
		 * From XML, use this syntax: <code>android:scaleType="centerInside"</code>.
		 */
		CENTER_INSIDE(7);

		ScaleType(int ni) {
			nativeInt = ni;
		}
		final int nativeInt;
	}
	private final ScaleType[] scaletype_from_int = {
		ScaleType.MATRIX,
		ScaleType.FIT_XY,
		ScaleType.FIT_START,
		ScaleType.FIT_CENTER,
		ScaleType.FIT_END,
		ScaleType.CENTER,
		ScaleType.CENTER_CROP,
		ScaleType.CENTER_INSIDE,
	};

	public final void setColorFilter(int color, PorterDuff.Mode mode) {
		setColorFilter(new PorterDuffColorFilter(color, mode));
	}

	public void setImageTintList(ColorStateList tint) {
		if (tint == null)
			colorFilter = null;
		else
			colorFilter = new PorterDuffColorFilter(tint.getDefaultColor(), PorterDuff.Mode.SRC_IN);
		setImageDrawable(drawable);
	}

	public void setImageAlpha(int alpha) {}

	public void setMaxWidth(int width) {}

	public void setMaxHeight(int height) {}

	public void setImageState(int[] state, boolean merge) {}

	public ColorStateList getImageTintList() {
		return null;
	}

	public PorterDuff.Mode getImageTintMode() {
		return null;
	}

	public void setImageTintMode(PorterDuff.Mode tintMode) {}

	public void setCropToPadding(boolean crop) {}

	public void setColorFilter(int color) {
		setColorFilter(color, PorterDuff.Mode.SRC_ATOP);
	}

	public final void clearColorFilter() {
		setColorFilter((ColorFilter)null);
	}

	public void setColorFilter(ColorFilter cf) {
		if (colorFilter == cf)
			return;
		colorFilter = cf;
		if (drawable != null)
			drawable.setColorFilter(cf);
		invalidate();
	}

	public Matrix getImageMatrix() {
		return Matrix.IDENTITY_MATRIX;
	}

	private int contentWidth() {
		if (bitmap != null)
			return bitmap.getWidth();
		if (drawable != null)
			return Math.max(0, drawable.getIntrinsicWidth());
		return 0;
	}

	private int contentHeight() {
		if (bitmap != null)
			return bitmap.getHeight();
		if (drawable != null)
			return Math.max(0, drawable.getIntrinsicHeight());
		return 0;
	}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		setMeasuredDimension(resolveSize(Math.max(contentWidth() + paddingLeft + paddingRight, getSuggestedMinimumWidth()), widthMeasureSpec),
		                     resolveSize(Math.max(contentHeight() + paddingTop + paddingBottom, getSuggestedMinimumHeight()), heightMeasureSpec));
	}

	@Override
	public void onDraw(Canvas canvas) {
		int innerWidth = getWidth() - paddingLeft - paddingRight;
		int innerHeight = getHeight() - paddingTop - paddingBottom;
		int contentWidth = contentWidth();
		int contentHeight = contentHeight();
		if (innerWidth <= 0 || innerHeight <= 0 || contentWidth <= 0 || contentHeight <= 0)
			return;
		float scaleX;
		float scaleY;
		switch (scaleType) {
			case FIT_XY:
				scaleX = (float)innerWidth / contentWidth;
				scaleY = (float)innerHeight / contentHeight;
				break;
			case CENTER:
				scaleX = scaleY = 1;
				break;
			case CENTER_CROP:
				scaleX = scaleY = Math.max((float)innerWidth / contentWidth, (float)innerHeight / contentHeight);
				break;
			case CENTER_INSIDE:
				scaleX = scaleY = Math.min(1, Math.min((float)innerWidth / contentWidth, (float)innerHeight / contentHeight));
				break;
			case FIT_START:
			case FIT_CENTER:
			case FIT_END:
			default:
				scaleX = scaleY = Math.min((float)innerWidth / contentWidth, (float)innerHeight / contentHeight);
				break;
		}
		int drawWidth = (int)(contentWidth * scaleX);
		int drawHeight = (int)(contentHeight * scaleY);
		int x = paddingLeft;
		int y = paddingTop;
		if (scaleType != ScaleType.FIT_START) {
			x += (innerWidth - drawWidth) / 2;
			y += (innerHeight - drawHeight) / 2;
		}
		canvas.save();
		canvas.clipRect(paddingLeft, paddingTop, paddingLeft + innerWidth, paddingTop + innerHeight);
		if (bitmap != null) {
			android.graphics.Paint paint = null;
			if (colorFilter != null) {
				paint = new android.graphics.Paint();
				paint.setColorFilter(colorFilter);
			}
			canvas.drawBitmap(bitmap, new android.graphics.Rect(0, 0, contentWidth, contentHeight),
			                  new android.graphics.Rect(x, y, x + drawWidth, y + drawHeight), paint);
		} else if (drawable != null) {
			// Match AOSP ImageView: the drawable's bounds are always its intrinsic size
			// anchored at the origin, and positioning/scaling is applied to the canvas.
			// Offsetting the bounds rect instead breaks drawables (e.g. DrawerArrowDrawable)
			// that compute their content position from bounds assuming a (0,0) origin.
			drawable.setBounds(0, 0, contentWidth, contentHeight);
			canvas.translate(x, y);
			canvas.scale(scaleX, scaleY);
			drawable.draw(canvas);
		}
		canvas.restore();
	}
}
