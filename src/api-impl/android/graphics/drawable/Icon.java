package android.graphics.drawable;

import android.content.Context;
import android.graphics.Bitmap;

public class Icon {

	public static Icon createWithResource(String packageName, int resourceId) {
		return new Icon();
	}

	public static Icon createWithResource(Context context, int resourceId) {
		return createWithResource(context != null ? context.getPackageName() : null, resourceId);
	}

	public static Icon createWithBitmap(Bitmap bitmap) {
		return new Icon();
	}

	/* no adaptive-icon masking here; the bitmap is taken as-is */
	public static Icon createWithAdaptiveBitmap(Bitmap bitmap) {
		return createWithBitmap(bitmap);
	}
	public Drawable loadDrawable(Context context) { return new ColorDrawable(); }
}
