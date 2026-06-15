package android.graphics;

public class LinearGradient extends Shader {

	public LinearGradient(float x0, float y0, float x1, float y1, int[] colors, float[] positions, TileMode mode) {
		init(native_create(x0, y0, x1, y1, colors, positions, mode != null ? mode.ordinal() : 0));
	}

	/* wide-gamut Color longs are not backed natively yet; falls back to a flat fill */
	public LinearGradient(float x0, float y0, float x1, float y1, long[] colors, float[] positions, TileMode tile) {}

	public LinearGradient(float x0, float y0, float x1, float y1, int color0, int color1, TileMode tile) {
		this(x0, y0, x1, y1, new int[] {color0, color1}, null, tile);
	}

	private static native long native_create(float x0, float y0, float x1, float y1, int[] colors, float[] positions, int tileMode);
}
