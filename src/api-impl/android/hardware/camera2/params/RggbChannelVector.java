package android.hardware.camera2.params;

/** Four per-colour-channel gains in the sensor's RGGB order. */
public final class RggbChannelVector {

	public static final int COUNT = 4;
	public static final int RED = 0;
	public static final int GREEN_EVEN = 1;
	public static final int GREEN_ODD = 2;
	public static final int BLUE = 3;

	private final float[] components;

	public RggbChannelVector(float red, float greenEven, float greenOdd, float blue) {
		components = new float[] {red, greenEven, greenOdd, blue};
		for (float value : components)
			if (Float.isNaN(value))
				throw new IllegalArgumentException("a channel gain must be a number");
	}

	public float getRed() {
		return components[RED];
	}

	public float getGreenEven() {
		return components[GREEN_EVEN];
	}

	public float getGreenOdd() {
		return components[GREEN_ODD];
	}

	public float getBlue() {
		return components[BLUE];
	}

	public float getComponent(int colorChannel) {
		if (colorChannel < 0 || colorChannel >= COUNT)
			throw new IllegalArgumentException("no such colour channel " + colorChannel);
		return components[colorChannel];
	}

	public void copyTo(float[] destination, int offset) {
		System.arraycopy(components, 0, destination, offset, COUNT);
	}

	@Override
	public boolean equals(Object other) {
		if (!(other instanceof RggbChannelVector))
			return false;
		RggbChannelVector o = (RggbChannelVector)other;
		for (int i = 0; i < COUNT; i++)
			if (components[i] != o.components[i])
				return false;
		return true;
	}

	@Override
	public int hashCode() {
		return java.util.Arrays.hashCode(components);
	}

	@Override
	public String toString() {
		return "RggbChannelVector(R:" + getRed() + ", G_even:" + getGreenEven() +
		    ", G_odd:" + getGreenOdd() + ", B:" + getBlue() + ")";
	}
}
