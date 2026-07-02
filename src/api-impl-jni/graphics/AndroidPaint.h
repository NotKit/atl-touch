#pragma once

#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"

#include <pango/pango.h>

#include <string>

namespace android {
struct Typeface;
}

struct AndroidPaint {
	SkPaint paint;
	SkFont font;
	/* matches both android.graphics.Paint.Align ordinals and SkTextUtils::Align */
	int text_align = 0;
	/* kept in sync with the font size for android_text_Layout.cpp, which
	 * still uses Pango for line breaking and metrics */
	PangoFontDescription *pango_font = nullptr;

	/* minikin shaping state (see hwui/MinikinGlue.h); typeface is owned by
	 * the Java android.graphics.Typeface holding it */
	android::Typeface *typeface = nullptr;
	float letter_spacing = 0;
	float word_spacing = 0;
	std::string font_feature_settings;
};
