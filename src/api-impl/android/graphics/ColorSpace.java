package android.graphics;

public class ColorSpace {
	/** native SkColorSpace handle; 0 = treat as sRGB */
	public long getNativeInstance() {
		return 0;
	}


	public static enum Named {
		SRGB,
	}

	public static ColorSpace get(Named named) {
		return new ColorSpace();
	}
}
