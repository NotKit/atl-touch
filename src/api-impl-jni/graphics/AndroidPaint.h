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
	uint32_t locale_list_id = 0;

	/* android.graphics.Paint state not representable in SkPaint/SkFont */
	int flags = 0;
	bool underline = false;
	bool strike_thru = false;
	bool elegant_text_height = false;
	int start_hyphen_edit = 0;
	int end_hyphen_edit = 0;
	float shadow_radius = 0;
	float shadow_dx = 0;
	float shadow_dy = 0;
	SkColor shadow_color = 0;
};
