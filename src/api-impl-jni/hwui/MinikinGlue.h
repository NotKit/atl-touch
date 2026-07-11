#include <stdint.h>
#pragma once

/*
 * Bridges AndroidPaint (SkPaint+SkFont held by android.graphics.Paint) and
 * android::Typeface to minikin's shaping/measurement entry points.
 */

#include <minikin/Layout.h>
#include <minikin/MinikinPaint.h>

#include <include/core/SkCanvas.h>

#include "../graphics/AndroidPaint.h"
#include "MinikinSkia.h"
#include "Typeface.h"

namespace android {

/* the default typeface, building the fontconfig collection on first use */
const Typeface *typeface_init_default(void);
Typeface *typeface_create_named(const char *name);
std::shared_ptr<minikin::FontFamily> typeface_family_from_file(const char *path);

minikin::MinikinPaint minikin_paint_for(const AndroidPaint *paint);

/* measure UTF-16 text with shaping and font fallback; per-unit advances go
 * to `advances` when non-null (sized count) */
float minikin_measure_text(const AndroidPaint *paint, const uint16_t *text, int count,
                           minikin::Bidi bidi, float *advances);

void minikin_draw_text(SkCanvas *canvas, const AndroidPaint *paint, const uint16_t *text,
                       int count, minikin::Bidi bidi, float x, float y);

} // namespace android
