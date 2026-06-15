package android.graphics;

public class RadialGradient extends Shader {

	public RadialGradient(float x, float y, float radius, int[] colors, float[] positions, android.graphics.Shader.TileMode tileMode) {
		init(native_create(x, y, radius, colors, positions, tileMode != null ? tileMode.ordinal() : 0));
	}

	private static native long native_create(float x, float y, float radius, int[] colors, float[] positions, int tileMode);
}
