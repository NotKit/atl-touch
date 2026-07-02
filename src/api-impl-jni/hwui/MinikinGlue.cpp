#include "MinikinGlue.h"

#include <include/core/SkBlurTypes.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkTextBlob.h>
#include <minikin/FontCollection.h>

namespace android {

minikin::MinikinPaint minikin_paint_for(const AndroidPaint *paint)
{
	const Typeface *typeface = paint->typeface ? paint->typeface : typeface_init_default();
	minikin::MinikinPaint mp(typeface->fFontCollection);
	mp.size = paint->font.getSize();
	mp.scaleX = paint->font.getScaleX();
	mp.skewX = paint->font.getSkewX();
	mp.letterSpacing = paint->letter_spacing;
	mp.wordSpacing = paint->word_spacing;
	mp.fontFlags = MinikinFontSkia::packFontFlags(paint->font);
	mp.localeListId = paint->locale_list_id;
	mp.fontStyle = typeface->fStyle;
	mp.fontFeatureSettings = paint->font_feature_settings;
	return mp;
}

float minikin_measure_text(const AndroidPaint *paint, const uint16_t *text, int count,
                           minikin::Bidi bidi, float *advances)
{
	minikin::U16StringPiece str(text, count);
	minikin::Range range(0, count);
	return minikin::Layout::measureText(str, range, bidi, minikin_paint_for(paint),
	                                    minikin::StartHyphenEdit::NO_EDIT,
	                                    minikin::EndHyphenEdit::NO_EDIT, advances);
}

void minikin_draw_text(SkCanvas *canvas, const AndroidPaint *paint, const uint16_t *text,
                       int count, minikin::Bidi bidi, float x, float y)
{
	minikin::U16StringPiece str(text, count);
	minikin::Range range(0, count);
	minikin::Layout layout(str, range, bidi, minikin_paint_for(paint),
	                       minikin::StartHyphenEdit::NO_EDIT, minikin::EndHyphenEdit::NO_EDIT);

	/* android.graphics.Paint.Align: 0 LEFT, 1 CENTER, 2 RIGHT */
	if (paint->text_align == 1)
		x -= layout.getAdvance() / 2;
	else if (paint->text_align == 2)
		x -= layout.getAdvance();

	/* shadow layer: draw the same runs blurred, offset, in the shadow color */
	if (paint->shadow_radius > 0) {
		AndroidPaint shadow = *paint;
		shadow.shadow_radius = 0;
		shadow.text_align = 0; // x is already alignment-adjusted
		shadow.paint.setColor(paint->shadow_color);
		shadow.paint.setMaskFilter(SkMaskFilter::MakeBlur(
		    kNormal_SkBlurStyle, paint->shadow_radius * 0.5f));
		minikin_draw_text(canvas, &shadow, text, count, bidi,
		                  x + paint->shadow_dx, y + paint->shadow_dy);
	}

	/* emit one glyph run per contiguous (font, fakery) stretch */
	size_t glyph_count = layout.nGlyphs();
	size_t run_start = 0;
	while (run_start < glyph_count) {
		const minikin::Font *font = layout.getFont(run_start);
		minikin::FontFakery fakery = layout.getFakery(run_start);
		size_t run_end = run_start + 1;
		while (run_end < glyph_count && layout.getFont(run_end) == font &&
		       layout.getFakery(run_end) == fakery)
			run_end++;

		SkFont sk_font = paint->font;
		MinikinFontSkia::populateSkFont(&sk_font, font->typeface().get(), fakery);

		SkTextBlobBuilder builder;
		const SkTextBlobBuilder::RunBuffer &buffer =
		    builder.allocRunPos(sk_font, run_end - run_start);
		for (size_t i = run_start; i < run_end; i++) {
			buffer.glyphs[i - run_start] = layout.getGlyphId(i);
			buffer.pos[2 * (i - run_start)] = x + layout.getX(i);
			buffer.pos[2 * (i - run_start) + 1] = y + layout.getY(i);
		}
		canvas->drawTextBlob(builder.make(), 0, 0, paint->paint);
		run_start = run_end;
	}

	/* text decorations */
	if (paint->underline || paint->strike_thru) {
		SkFontMetrics metrics;
		paint->font.getMetrics(&metrics);
		float thickness;
		if (!metrics.hasUnderlineThickness(&thickness))
			thickness = paint->font.getSize() / 18.0f;
		SkPaint line_paint = paint->paint;
		line_paint.setStyle(SkPaint::kFill_Style);
		if (paint->underline) {
			float position;
			if (!metrics.hasUnderlinePosition(&position))
				position = paint->font.getSize() / 9.0f;
			canvas->drawRect(SkRect::MakeXYWH(x, y + position, layout.getAdvance(), thickness),
			                 line_paint);
		}
		if (paint->strike_thru) {
			float position = -paint->font.getSize() * (6.0f / 21.0f);
			canvas->drawRect(SkRect::MakeXYWH(x, y + position, layout.getAdvance(), thickness),
			                 line_paint);
		}
	}
}

} // namespace android
