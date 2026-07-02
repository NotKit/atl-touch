package android.graphics.fonts;

public class FontVariationAxis {
	private float style_value;
	private String tag_string;
	public FontVariationAxis(String tagString, float styleValue) {
		style_value = styleValue;
		tag_string = tagString;
	}

	public static FontVariationAxis[] fromFontVariationSettings(String settings) {
		FontVariationAxis arr[] = {};
		return arr;
	}

	public int getOpenTypeTagValue() {
		return (tag_string.charAt(0) << 24) | (tag_string.charAt(1) << 16) |
		       (tag_string.charAt(2) << 8) | tag_string.charAt(3);
	}

	public float getStyleValue() {
		return style_value;
	}

	public String getTag() {
		return tag_string;
	}

	public String toFontVariationSettings(FontVariationAxis[] axes) {
		return "";
	}
}
