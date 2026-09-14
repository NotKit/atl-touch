package android.util;

public class SizeF {
	private final float width;
	private final float height;

	public SizeF(float width, float height) {
		this.width = width;
		this.height = height;
	}

	public float getWidth() {
		return width;
	}

	public float getHeight() {
		return height;
	}

	@Override
	public boolean equals(Object other) {
		if (this == other)
			return true;
		if (!(other instanceof SizeF))
			return false;
		SizeF size = (SizeF)other;
		return width == size.width && height == size.height;
	}

	@Override
	public int hashCode() {
		return Float.floatToIntBits(width) ^ Float.floatToIntBits(height);
	}

	@Override
	public String toString() {
		return width + "x" + height;
	}
}
