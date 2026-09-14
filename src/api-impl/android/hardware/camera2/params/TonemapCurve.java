package android.hardware.camera2.params;

import android.graphics.PointF;

/**
 * A per-channel tone mapping curve, as (input, output) point pairs.
 *
 * ATL's backends do not apply it — the object exists so an app can build one,
 * set it on a request and read it back.
 */
public final class TonemapCurve {

	public static final int CHANNEL_RED = 0;
	public static final int CHANNEL_GREEN = 1;
	public static final int CHANNEL_BLUE = 2;

	public static final float LEVEL_BLACK = 0.0f;
	public static final float LEVEL_WHITE = 1.0f;

	public static final int POINT_SIZE = 2;

	private final float[][] curves;

	public TonemapCurve(float[] red, float[] green, float[] blue) {
		if (red == null || green == null || blue == null)
			throw new NullPointerException("curves must not be null");
		curves = new float[][] {red.clone(), green.clone(), blue.clone()};
	}

	public int getPointCount(int colorChannel) {
		return curve(colorChannel).length / POINT_SIZE;
	}

	public PointF getPoint(int colorChannel, int index) {
		float[] curve = curve(colorChannel);

		if (index < 0 || index >= curve.length / POINT_SIZE)
			throw new IllegalArgumentException("point index out of range: " + index);
		return new PointF(curve[index * POINT_SIZE], curve[index * POINT_SIZE + 1]);
	}

	public void copyColorCurve(int colorChannel, float[] destination, int offset) {
		float[] curve = curve(colorChannel);

		System.arraycopy(curve, 0, destination, offset, curve.length);
	}

	private float[] curve(int colorChannel) {
		if (colorChannel < 0 || colorChannel > CHANNEL_BLUE)
			throw new IllegalArgumentException("bad colour channel: " + colorChannel);
		return curves[colorChannel];
	}

	@Override
	public String toString() {
		return "TonemapCurve(" + getPointCount(CHANNEL_RED) + " point(s) per channel)";
	}
}
