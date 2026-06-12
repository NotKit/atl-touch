#include "ATLCanvas.h"
#include "NinePatchPaintable.h"

#include "include/core/SkColorFilter.h"
#include "include/core/SkSamplingOptions.h"

enum {
	// The 9 patch segment is not a solid color.
	NO_COLOR = 0x00000001,
	// The 9 patch segment is completely transparent.
	TRANSPARENT_COLOR = 0x00000000
};

/* same layout logic as ninepatch_paintable_snapshot() in NinePatchPaintable.c,
 * targeting an SkCanvas instead of a GtkSnapshot */
void ninepatch_paintable_draw_skia(NinePatchPaintable *ninepatch, void *atl_canvas_ptr, double width, double height)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)atl_canvas_ptr;
	SkCanvas *canvas = atl_canvas->canvas;
	struct Res_png_9patch *chunk = ninepatch->chunk;
	if (!chunk)
		return;

	SkBitmap *bitmap = (SkBitmap *)atl_skbitmap_from_gdk_texture(ninepatch->texture);
	sk_sp<SkImage> image = atl_image_for_draw(atl_canvas, bitmap);
	if (!image)
		return;

	SkPaint paint;
	if (ninepatch->tint)
		paint.setColorFilter(SkColorFilters::Blend((SkColor)(uint32_t)ninepatch->tint, SkBlendMode::kSrcIn));
	SkSamplingOptions sampling(SkFilterMode::kLinear);

	int32_t *xDivs = (int32_t *)((char *)chunk + chunk->xDivsOffset);
	int32_t *yDivs = (int32_t *)((char *)chunk + chunk->yDivsOffset);
	int32_t *color = (int32_t *)((char *)chunk + chunk->colorsOffset);
	float strech_factor_width = (width - (ninepatch->width - ninepatch->strechy_width)) / ninepatch->strechy_width;
	float strech_factor_height = (height - (ninepatch->height - ninepatch->strechy_height)) / ninepatch->strechy_height;

	float rect_y = 0, rect_height = 0;
	for (int j = 0; j < chunk->numYDivs + 1; j++, rect_y += rect_height) {
		int ydiv_start = j ? yDivs[j - 1] : 0;
		int ydiv_end = (j == chunk->numYDivs) ? ninepatch->height : yDivs[j];
		float actual_stretch_factor_height = (j % 2) ? strech_factor_height : 1;
		rect_height = (ydiv_end - ydiv_start) * actual_stretch_factor_height;
		if (!rect_height) // skip empty sections
			continue;
		float rect_x = 0, rect_width = 0;
		for (int i = 0; i < chunk->numXDivs + 1; i++, rect_x += rect_width) {
			int xdiv_start = i ? xDivs[i - 1] : 0;
			int xdiv_end = (i == chunk->numXDivs) ? ninepatch->width : xDivs[i];
			float actual_stretch_factor_width = (i % 2) ? strech_factor_width : 1;
			rect_width = (xdiv_end - xdiv_start) * actual_stretch_factor_width;
			if (!rect_width) // skip empty sections
				continue;
			SkRect rect = SkRect::MakeXYWH(rect_x, rect_y, rect_width, rect_height);
			if (*color == NO_COLOR) {
				SkRect texture_bounds = SkRect::MakeXYWH(rect_x - xdiv_start * actual_stretch_factor_width,
				                                         rect_y - ydiv_start * actual_stretch_factor_height,
				                                         ninepatch->width * actual_stretch_factor_width,
				                                         ninepatch->height * actual_stretch_factor_height);
				canvas->save();
				canvas->clipRect(rect);
				canvas->drawImageRect(image, texture_bounds, sampling, &paint);
				canvas->restore();
			} else if (*color != TRANSPARENT_COLOR) {
				SkPaint color_paint(paint);
				color_paint.setColor((SkColor)(uint32_t)*color);
				canvas->drawRect(rect, color_paint);
			}
			color++;
		}
	}
}
