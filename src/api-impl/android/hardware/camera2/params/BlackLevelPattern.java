package android.hardware.camera2.params;

/** The four black levels of a 2x2 colour filter block, in RGGB order. */
public final class BlackLevelPattern {

	public static final int COUNT = 4;

	private final int[] offsets;

	public BlackLevelPattern(int[] offsets) {
		if (offsets == null || offsets.length < COUNT)
			throw new IllegalArgumentException("a black level pattern has " + COUNT + " offsets");
		this.offsets = new int[] {offsets[0], offsets[1], offsets[2], offsets[3]};
	}

	public int getOffsetForIndex(int column, int row) {
		if (column < 0 || column > 1 || row < 0 || row > 1)
			throw new IllegalArgumentException("no offset at (" + column + ", " + row + ")");
		return offsets[row * 2 + column];
	}

	public void copyTo(int[] destination, int offset) {
		System.arraycopy(offsets, 0, destination, offset, COUNT);
	}

	@Override
	public boolean equals(Object other) {
		return other instanceof BlackLevelPattern &&
		    java.util.Arrays.equals(offsets, ((BlackLevelPattern)other).offsets);
	}

	@Override
	public int hashCode() {
		return java.util.Arrays.hashCode(offsets);
	}

	@Override
	public String toString() {
		return "BlackLevelPattern([" + offsets[0] + ", " + offsets[1] + "], [" +
		    offsets[2] + ", " + offsets[3] + "])";
	}
}
