package android.graphics;

public class ColorMatrixColorFilter extends ColorFilter {
	private final ColorMatrix mMatrix = new ColorMatrix();

	public ColorMatrixColorFilter(ColorMatrix matrix) {
		mMatrix.set(matrix);
	}

	public ColorMatrixColorFilter(float[] array) {
		if (array.length < 20)
			throw new ArrayIndexOutOfBoundsException();
		mMatrix.set(array);
	}

	public ColorMatrix getColorMatrix() { return mMatrix; }

	@Override
	public long getNativeInstance() {
		try {
			return native_CreateColorMatrixFilter(mMatrix.getArray());
		} catch (UnsatisfiedLinkError e) {
			return 0;
		}
	}

	private static native long native_CreateColorMatrixFilter(float[] array);
}
