#include "ATLCanvas.h"

#include "include/core/SkColorSpace.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/ports/SkFontMgr_fontconfig.h"

static const SkImageInfo atl_image_info(int width, int height)
{
	return SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
}

ATLCanvas *ATLCanvas::new_raster(int width, int height)
{
	ATLCanvas *atl_canvas = new ATLCanvas();
	atl_canvas->bitmap = new SkBitmap();
	atl_canvas->bitmap->allocPixels(atl_image_info(width, height));
	atl_canvas->bitmap->eraseColor(SK_ColorTRANSPARENT);
	atl_canvas->owns_bitmap = true;
	atl_canvas->canvas = new SkCanvas(*atl_canvas->bitmap);
	return atl_canvas;
}

ATLCanvas *ATLCanvas::for_bitmap(SkBitmap *bitmap)
{
	ATLCanvas *atl_canvas = new ATLCanvas();
	atl_canvas->bitmap = bitmap;
	atl_canvas->canvas = new SkCanvas(*bitmap);
	return atl_canvas;
}

ATLCanvas *ATLCanvas::new_recording(void)
{
	ATLCanvas *atl_canvas = new ATLCanvas();
	atl_canvas->recorder = new SkPictureRecorder();
	atl_canvas->canvas = atl_canvas->recorder->beginRecording(SkRect::MakeLTRB(-1e9f, -1e9f, 1e9f, 1e9f));
	return atl_canvas;
}

ATLCanvas::~ATLCanvas()
{
	if (recorder)
		delete recorder; // owns the recording canvas
	else
		delete canvas;
	if (owns_bitmap)
		delete bitmap;
}

void ATLNode::onDraw(SkCanvas *canvas)
{
	if (is_stub) {
		if (target)
			target->draw(canvas);
		return;
	}
	if (child) {
		canvas->save();
		if (has_clip)
			canvas->clipRect(clip);
		canvas->concat(matrix);
		child->draw(canvas);
		canvas->restore();
		return;
	}
	if (recording)
		recording->draw(canvas);
}

bool ATLNode::patch(ATLNode *old_child, ATLNode *new_child)
{
	bool patched = false;
	for (auto &stub : stubs) {
		if (stub.get() == old_child || stub->target.get() == old_child) {
			stub->target = sk_ref_sp(new_child);
			stub->notifyDrawingChanged();
			patched = true;
		}
	}
	if (child)
		patched |= child->patch(old_child, new_child);
	return patched;
}

sk_sp<SkImage> atl_image_for_draw(ATLCanvas *atl_canvas, SkBitmap *bitmap)
{
	if (atl_canvas->is_recording())
		return bitmap->asImage(); // copies mutable pixels: safe beyond this call
	SkPixmap pixmap;
	if (!bitmap->peekPixels(&pixmap))
		return nullptr;
	return SkImages::RasterFromPixmap(pixmap, nullptr, nullptr);
}

sk_sp<SkFontMgr> atl_fontmgr(void)
{
	static sk_sp<SkFontMgr> fontmgr = SkFontMgr_New_FontConfig(nullptr);
	return fontmgr;
}

sk_sp<SkTypeface> atl_default_typeface(void)
{
	static sk_sp<SkTypeface> typeface = atl_fontmgr()->legacyMakeTypeface(nullptr, SkFontStyle());
	return typeface;
}

/* --- C bridge --- */

extern "C" void *atl_canvas_new_raster(int width, int height)
{
	return ATLCanvas::new_raster(width, height);
}

extern "C" void atl_canvas_free(void *atl_canvas)
{
	delete (ATLCanvas *)atl_canvas;
}

extern "C" const void *atl_canvas_get_pixels(void *atl_canvas, int *width, int *height, int *stride)
{
	SkBitmap *bitmap = ((ATLCanvas *)atl_canvas)->bitmap;
	*width = bitmap->width();
	*height = bitmap->height();
	*stride = (int)bitmap->rowBytes();
	return bitmap->getPixels();
}

/* Damage-region frame on a persistent raster canvas: clip to the damage rect
 * and clear just that region; pixels outside it survive from the last frame. */
extern "C" void atl_canvas_begin_frame(void *atl_canvas, int left, int top, int right, int bottom)
{
	SkCanvas *canvas = ((ATLCanvas *)atl_canvas)->canvas;
	canvas->save();
	canvas->clipRect(SkRect::MakeLTRB(left, top, right, bottom));
	canvas->clear(SK_ColorTRANSPARENT);
}

extern "C" void atl_canvas_end_frame(void *atl_canvas)
{
	/* pop the damage clip and anything an unbalanced app draw left behind;
	 * leftovers would otherwise accumulate on the reused canvas */
	((ATLCanvas *)atl_canvas)->canvas->restoreToCount(1);
}

