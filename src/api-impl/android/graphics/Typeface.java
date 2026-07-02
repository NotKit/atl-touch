package android.graphics;

import android.content.res.AssetManager;

public class Typeface {

	// Style
	public static final int NORMAL = 0;
	public static final int BOLD = 1;
	public static final int ITALIC = 2;
	public static final int BOLD_ITALIC = 3;

	public long native_instance = 0; // android::Typeface*; directly accessed by androidx
	private int style;

	/**
	 * The default NORMAL typeface object
	 */
	public static final Typeface DEFAULT = create((String)null, NORMAL);
	/**
	 * The default BOLD typeface object. Note: this may be not actually be
	 * bold, depending on what fonts are installed. Call getStyle() to know
	 * for sure.
	 */
	public static final Typeface DEFAULT_BOLD = create((String)null, BOLD);
	/**
	 * The NORMAL style of the default sans serif typeface.
	 */
	public static final Typeface SANS_SERIF = create("sans-serif", NORMAL);
	/**
	 * The NORMAL style of the default serif typeface.
	 */
	public static final Typeface SERIF = create("serif", NORMAL);
	/**
	 * The NORMAL style of the default monospace typeface.
	 */
	public static final Typeface MONOSPACE = create("monospace", NORMAL);

	private Typeface(long native_instance, int style) {
		this.native_instance = native_instance;
		this.style = style;
	}

	public Typeface() {
		this(nativeCreateNamed(null, NORMAL), NORMAL);
	}

	public static Typeface create(String familyName, int style) {
		return new Typeface(nativeCreateNamed(familyName, style), style);
	}

	public static Typeface create(Typeface family, int style) {
		long base = family != null ? family.native_instance : 0;
		return new Typeface(nativeCreateRelative(base, style), style);
	}

	public static Typeface createFromFile(String path) {
		long instance = nativeCreateFromFile(path);
		return instance != 0 ? new Typeface(instance, NORMAL) : DEFAULT;
	}

	public static Typeface createFromAsset(AssetManager mgr, String path) {
		String extractedPath = mgr.extractAsset(path);
		return extractedPath != null ? createFromFile(extractedPath) : DEFAULT;
	}

	public boolean isSupportedAxes(int axis) {
		return false; // variation axes not wired up yet
	}

	public int getStyle() {
		return style;
	}

	public boolean isBold() {
		return (style & BOLD) != 0;
	}

	public boolean isItalic() {
		return (style & ITALIC) != 0;
	}

	public static Typeface defaultFromStyle(int style) {
		switch (style) {
			case BOLD: return DEFAULT_BOLD;
			case NORMAL: return DEFAULT;
			default: return create((String)null, style);
		}
	}

	public static Typeface createFromTypefaceWithVariation(Typeface family, java.util.List<android.graphics.fonts.FontVariationAxis> axes) {
		// TODO: apply the variation axes through minikin
		return family != null ? family : DEFAULT;
	}

	public static Typeface createFromFamiliesWithDefault(FontFamily[] families) {
		return DEFAULT;
	}

	public static Typeface createFromFamiliesWithDefault(FontFamily[] families, int dummy1, int dummy2) {
		return createFromFamiliesWithDefault(families);
	}

	public static class Builder {
		private final String path;

		public Builder(AssetManager mgr, String path) {
			this.path = mgr.extractAsset(path);
		}

		public Builder setFontVariationSettings(String settings) {
			return this;
		}

		public Typeface build() {
			return path != null ? createFromFile(path) : DEFAULT;
		}
	}

	private static native long nativeCreateNamed(String familyName, int style);
	private static native long nativeCreateRelative(long base, int style);
	private static native long nativeCreateFromFile(String path);
}
