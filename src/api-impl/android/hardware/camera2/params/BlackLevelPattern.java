package android.hardware.camera2.params;

/** The four black levels of a 2x2 colour filter block, in RGGB order. */
public final class BlackLevelPattern {

	public static final int COUNT = 4;

	/* AOSP's name for the array, because camera apps patch it by reflection */
	private final int[] mCfaOffsets;

	public BlackLevelPattern(int[] offsets) {
		if (offsets == null || offsets.length < COUNT)
			throw new IllegalArgumentException("a black level pattern has " + COUNT + " offsets");
		this.mCfaOffsets = new int[] {offsets[0], offsets[1], offsets[2], offsets[3]};
	}

	public int getOffsetForIndex(int column, int row) {
		if (column < 0 || column > 1 || row < 0 || row > 1)
			throw new IllegalArgumentException("no offset at (" + column + ", " + row + ")");
		return mCfaOffsets[row * 2 + column];
	}

	public void copyTo(int[] destination, int offset) {
		System.arraycopy(mCfaOffsets, 0, destination, offset, COUNT);
	}

	@Override
	public boolean equals(Object other) {
		return other instanceof BlackLevelPattern &&
		    java.util.Arrays.equals(mCfaOffsets, ((BlackLevelPattern)other).mCfaOffsets);
	}

	@Override
	public int hashCode() {
		return java.util.Arrays.hashCode(mCfaOffsets);
	}

	@Override
	public String toString() {
		return "BlackLevelPattern([" + mCfaOffsets[0] + ", " + mCfaOffsets[1] + "], [" +
		    mCfaOffsets[2] + ", " + mCfaOffsets[3] + "])";
	}
}
