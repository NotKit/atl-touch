package android.graphics;

public class BitmapShader extends Shader {

	Bitmap bitmap;

	public BitmapShader(Bitmap bitmap, TileMode tileX, TileMode tileY) {
		this.bitmap = bitmap;
		init(native_create(bitmap.getTexture(), tileX.ordinal(), tileY.ordinal()));
	}

	private static native long native_create(long bitmap, int tileX, int tileY);
}
