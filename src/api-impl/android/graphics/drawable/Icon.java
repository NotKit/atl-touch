package android.graphics.drawable;

import android.content.Context;
import android.graphics.Bitmap;

public class Icon {

	public static Icon createWithResource(String packageName, int resourceId) {
		return null;
	}

	public static Icon createWithResource(Context context, int resourceId) {
		return createWithResource(context != null ? context.getPackageName() : null, resourceId);
	}

	public static Icon createWithBitmap(Bitmap bitmap) {
		return null;
	}

	/* no adaptive-icon masking here; the bitmap is taken as-is */
	public static Icon createWithAdaptiveBitmap(Bitmap bitmap) {
		return createWithBitmap(bitmap);
	}
}
