package android.util;

public class Size {

	private int width;
	private int height;

	public Size(int width, int height) {
		this.width = width;
		this.height = height;
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	/** "WxH", the form parseSize() reads back. */
	public static Size parseSize(String string) {
		int separator = string.indexOf('*');

		if (separator < 0)
			separator = string.indexOf('x');
		if (separator < 0)
			throw new NumberFormatException("a size is WxH, not \"" + string + "\"");
		return new Size(Integer.parseInt(string.substring(0, separator)),
		    Integer.parseInt(string.substring(separator + 1)));
	}

	@Override
	public boolean equals(Object other) {
		if (this == other)
			return true;
		if (!(other instanceof Size))
			return false;
		Size size = (Size)other;
		return width == size.width && height == size.height;
	}

	@Override
	public int hashCode() {
		return height ^ ((width << 16) | (width >>> 16));
	}

	@Override
	public String toString() {
		return width + "x" + height;
	}
}
