package android.hardware.camera2.params;

import android.util.Rational;

/** A 3x3 rational matrix, stored row-major as the HAL's numerator/denominator pairs. */
public final class ColorSpaceTransform {

	private static final int ROWS = 3;
	private static final int COLUMNS = 3;
	private static final int COUNT = ROWS * COLUMNS;

	/* (numerator, denominator) per element, which is the HAL's own layout */
	private final int[] elements = new int[COUNT * 2];

	public ColorSpaceTransform(Rational[] elements) {
		if (elements == null || elements.length != COUNT)
			throw new IllegalArgumentException("a colour space transform has " + COUNT + " elements");
		for (int i = 0; i < COUNT; i++) {
			this.elements[i * 2] = elements[i].getNumerator();
			this.elements[i * 2 + 1] = elements[i].getDenominator();
		}
	}

	public ColorSpaceTransform(int[] elements) {
		if (elements == null || elements.length != COUNT * 2)
			throw new IllegalArgumentException("a colour space transform has " + COUNT * 2 + " ints");
		System.arraycopy(elements, 0, this.elements, 0, COUNT * 2);
	}

	public Rational getElement(int column, int row) {
		if (column < 0 || column >= COLUMNS || row < 0 || row >= ROWS)
			throw new IllegalArgumentException("no element at (" + column + ", " + row + ")");
		int i = (row * COLUMNS + column) * 2;
		return new Rational(elements[i], elements[i + 1]);
	}

	public void copyElements(Rational[] destination, int offset) {
		for (int i = 0; i < COUNT; i++)
			destination[offset + i] = new Rational(elements[i * 2], elements[i * 2 + 1]);
	}

	public void copyElements(int[] destination, int offset) {
		System.arraycopy(elements, 0, destination, offset, COUNT * 2);
	}

	@Override
	public boolean equals(Object other) {
		return other instanceof ColorSpaceTransform &&
		    java.util.Arrays.equals(elements, ((ColorSpaceTransform)other).elements);
	}

	@Override
	public int hashCode() {
		return java.util.Arrays.hashCode(elements);
	}

	@Override
	public String toString() {
		StringBuilder out = new StringBuilder("ColorSpaceTransform(");

		for (int row = 0; row < ROWS; row++) {
			out.append('[');
			for (int column = 0; column < COLUMNS; column++) {
				if (column > 0)
					out.append(", ");
				out.append(getElement(column, row));
			}
			out.append(']');
		}
		return out.append(')').toString();
	}
}
