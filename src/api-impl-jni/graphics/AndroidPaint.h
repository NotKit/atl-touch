#pragma once

#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"

#include <pango/pango.h>

struct AndroidPaint {
	SkPaint paint;
	SkFont font;
	/* matches both android.graphics.Paint.Align ordinals and SkTextUtils::Align */
	int text_align = 0;
	/* kept in sync with the font size for android_text_Layout.cpp, which
	 * still uses Pango for line breaking and metrics */
	PangoFontDescription *pango_font = nullptr;
};
