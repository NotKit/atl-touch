package android.hardware.camera2.params;

/**
 * The per-channel vignetting gains of one frame, on a rows x columns grid.
 *
 * The HAL ships them as one float array of 4 gains per grid point, RGGB, in
 * row-major order; the shading map size comes from the same metadata bag.
 */
public final class LensShadingMap {

	public static final float MINIMUM_GAIN_FACTOR = 1.0f;

	private final float[] elements;
	private final int rows;
	private final int columns;

	public LensShadingMap(float[] elements, int rows, int columns) {
		if (rows <= 0 || columns <= 0)
			throw new IllegalArgumentException("a shading map needs a positive size");
		if (elements == null || elements.length < rows * columns * RggbChannelVector.COUNT)
			throw new IllegalArgumentException("a " + rows + "x" + columns +
			    " shading map needs " + rows * columns * RggbChannelVector.COUNT + " gains");
		this.elements = elements.clone();
		this.rows = rows;
		this.columns = columns;
	}

	public int getRowCount() {
		return rows;
	}

	public int getColumnCount() {
		return columns;
	}

	public int getGainFactorCount() {
		return rows * columns * RggbChannelVector.COUNT;
	}

	public float getGainFactor(int colorChannel, int column, int row) {
		if (colorChannel < 0 || colorChannel >= RggbChannelVector.COUNT)
			throw new IllegalArgumentException("no such colour channel " + colorChannel);
		if (column < 0 || column >= columns || row < 0 || row >= rows)
			throw new IllegalArgumentException("no gain at (" + column + ", " + row + ")");
		return elements[(row * columns + column) * RggbChannelVector.COUNT + colorChannel];
	}

	public RggbChannelVector getGainFactorVector(int column, int row) {
		return new RggbChannelVector(getGainFactor(RggbChannelVector.RED, column, row),
		    getGainFactor(RggbChannelVector.GREEN_EVEN, column, row),
		    getGainFactor(RggbChannelVector.GREEN_ODD, column, row),
		    getGainFactor(RggbChannelVector.BLUE, column, row));
	}

	public void copyGainFactors(float[] destination, int offset) {
		System.arraycopy(elements, 0, destination, offset, getGainFactorCount());
	}

	@Override
	public boolean equals(Object other) {
		if (!(other instanceof LensShadingMap))
			return false;
		LensShadingMap o = (LensShadingMap)other;
		return rows == o.rows && columns == o.columns &&
		    java.util.Arrays.equals(elements, o.elements);
	}

	@Override
	public int hashCode() {
		return (rows * 31 + columns) * 31 + java.util.Arrays.hashCode(elements);
	}

	@Override
	public String toString() {
		return "LensShadingMap(" + columns + "x" + rows + ")";
	}
}
