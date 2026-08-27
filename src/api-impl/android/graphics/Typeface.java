package android.graphics;

import android.content.res.AssetManager;

public class Typeface {

	/** @hide */
	@java.lang.annotation.Retention(java.lang.annotation.RetentionPolicy.SOURCE)
	public @interface Style {}

	// Style
	public static final int NORMAL = 0;
	public static final int BOLD = 1;
	public static final int ITALIC = 2;
	public static final int BOLD_ITALIC = 3;

	/** Read the weight or the italic bit out of the font's own tables. */
	public static final int RESOLVE_BY_FONT_TABLE = -1;

	public long native_instance = 0; // android::Typeface*; directly accessed by androidx
	private int style;
	private int weight;

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

	/* the style and the weight are whatever the native face resolved to, not
	 * what the caller asked for: a 500 face asked for as NORMAL is still a 500 */
	private Typeface(long native_instance) {
		this.native_instance = native_instance;
		this.style = nativeGetStyle(native_instance);
		this.weight = nativeGetWeight(native_instance);
	}

	public Typeface() {
		this(nativeCreateNamed(null, NORMAL));
	}

	public static Typeface create(String familyName, int style) {
		return new Typeface(nativeCreateNamed(familyName, style));
	}

	public static Typeface create(Typeface family, int style) {
		long base = family != null ? family.native_instance : 0;
		return new Typeface(nativeCreateRelative(base, style));
	}

	public static Typeface create(Typeface family, int weight, boolean italic) {
		long base = family != null ? family.native_instance : 0;
		return new Typeface(nativeCreateFromTypefaceWithExactStyle(base, weight, italic));
	}

	public int getWeight() {
		return weight;
	}

	public static Typeface createFromFile(String path) {
		return createFromFile(path, RESOLVE_BY_FONT_TABLE, RESOLVE_BY_FONT_TABLE);
	}

	/* weight/italic override the file's own OS/2 values, so the face the family
	 * holds is declared as the caller says it is and needs no synthesis */
	private static Typeface createFromFile(String path, int weight, int italic) {
		long instance = path != null ? nativeCreateFromFile(path, weight, italic) : 0;
		return instance != 0 ? new Typeface(instance) : DEFAULT;
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
		public static final int NORMAL_WEIGHT = 400;
		public static final int BOLD_WEIGHT = 700;

		private final String path;
		private int weight = RESOLVE_BY_FONT_TABLE;
		private int italic = RESOLVE_BY_FONT_TABLE;

		public Builder(String path) {
			this.path = path;
		}

		public Builder(java.io.File path) {
			this.path = path.getAbsolutePath();
		}

		public Builder(AssetManager mgr, String path) {
			this.path = mgr.extractAsset(path);
		}

		public Builder setFontVariationSettings(String settings) {
			return this;
		}

		public Builder setWeight(int weight) {
			this.weight = weight;
			return this;
		}

		public Builder setItalic(boolean italic) {
			this.italic = italic ? 1 : 0;
			return this;
		}

		public Typeface build() {
			return createFromFile(path, weight, italic);
		}
	}

	private static native long nativeCreateNamed(String familyName, int style);
	private static native long nativeCreateRelative(long base, int style);
	private static native long nativeCreateFromTypefaceWithExactStyle(long base, int weight, boolean italic);
	private static native long nativeCreateFromFile(String path, int weight, int italic);
	private static native int nativeGetStyle(long native_instance);
	private static native int nativeGetWeight(long native_instance);
}
