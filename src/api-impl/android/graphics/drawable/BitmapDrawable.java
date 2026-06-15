package android.graphics.drawable;

import android.content.res.Resources;
import android.content.res.Resources.Theme;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.Gravity;
import android.atl.GskCanvas;
import com.android.internal.R;
import java.io.IOException;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

public class BitmapDrawable extends Drawable {

	private Bitmap bitmap;
	private int gravity = Gravity.FILL;
	private final Rect dstRect = new Rect();

	public BitmapDrawable() {
	}

	public BitmapDrawable(Resources res, Bitmap bitmap) {
		this.bitmap = bitmap;
		if (bitmap != null)
			setPaintable(bitmap.getGdkTexture());
	}

	public BitmapDrawable(Bitmap bitmap) {
		this.bitmap = bitmap;
		if (bitmap != null)
			setPaintable(bitmap.getGdkTexture());
	}

	public Bitmap getBitmap() {
		return bitmap;
	}

	public void inflate(Resources r, XmlPullParser parser, AttributeSet attrs, Theme theme)
	    throws XmlPullParserException, IOException {
		final TypedArray a = obtainAttributes(r, theme, attrs, R.styleable.BitmapDrawable);
		if (a.hasValue(R.styleable.BitmapDrawable_src)) {
			BitmapDrawable dr = null;
			try {
				// TODO: getDrawable() might pick up another <bitmap> XML,
				// but we should reject anything other than image files.
				dr = (BitmapDrawable)a.getDrawable(R.styleable.BitmapDrawable_src);
			} catch (java.lang.Exception e) {
				e.printStackTrace();
			}
			if (dr != null)
				bitmap = dr.bitmap;
		}
		gravity = a.getInt(R.styleable.BitmapDrawable_gravity, Gravity.FILL);
		a.recycle();
		if (bitmap == null)
			throw new XmlPullParserException("<bitmap> needs a valid `src' attribute");
		setPaintable(bitmap.getGdkTexture());
	}

	@Override
	public int getIntrinsicWidth() {
		if (bitmap == null)
			return 0;
		return bitmap.getWidth();
	}

	@Override
	public int getIntrinsicHeight() {
		if (bitmap == null)
			return 0;
		return bitmap.getHeight();
	}

	public void setTileModeX(Shader.TileMode mode) {}

	public Paint getPaint() {
		return new Paint();
	}

	public void setGravity(int g) {
		if (gravity != g) {
			gravity = g;
			invalidateSelf();
		}
	}

	public int getGravity() {
		return gravity;
	}

	@Override
	public void draw(Canvas canvas) {
		if (bitmap == null)
			return;

		Rect bounds = getBounds();
		// Gravity.FILL stretches to the full bounds (the default), which matches
		// the base Drawable behaviour. Any other gravity positions the bitmap at
		// its intrinsic size within the bounds (e.g. android:gravity="center").
		Gravity.apply(gravity, bitmap.getWidth(), bitmap.getHeight(), bounds, dstRect);

		if (canvas instanceof GskCanvas) {
			canvas.translate(dstRect.left, dstRect.top);
			native_draw(paintable, ((GskCanvas)canvas).snapshot, dstRect.width(), dstRect.height());
			canvas.translate(-dstRect.left, -dstRect.top);
		}
	}
}
