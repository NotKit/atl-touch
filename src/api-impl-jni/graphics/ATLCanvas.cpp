#include "ATLCanvas.h"

#include <gtk/gtk.h>

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

extern "C" GdkTexture *atl_skbitmap_to_gdk_texture(void *skbitmap)
{
	SkBitmap *bitmap = (SkBitmap *)skbitmap;
	GBytes *bytes = g_bytes_new(bitmap->getPixels(), bitmap->computeByteSize());
	GdkTexture *texture = gdk_memory_texture_new(bitmap->width(), bitmap->height(),
	                                             GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, bytes, bitmap->rowBytes());
	g_bytes_unref(bytes);
	return texture;
}

extern "C" GdkTexture *atl_canvas_to_gdk_texture(void *atl_canvas)
{
	return atl_skbitmap_to_gdk_texture(((ATLCanvas *)atl_canvas)->bitmap);
}

extern "C" void *atl_skbitmap_from_gdk_texture(GdkTexture *texture)
{
	SkBitmap *bitmap = (SkBitmap *)g_object_get_data(G_OBJECT(texture), "atl-skbitmap");
	if (bitmap)
		return bitmap;
	bitmap = new SkBitmap();
	bitmap->allocPixels(atl_image_info(gdk_texture_get_width(texture), gdk_texture_get_height(texture)));
	GdkTextureDownloader *downloader = gdk_texture_downloader_new(texture);
	gdk_texture_downloader_set_format(downloader, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);
	gdk_texture_downloader_download_into(downloader, (guchar *)bitmap->getPixels(), bitmap->rowBytes());
	gdk_texture_downloader_free(downloader);
	g_object_set_data_full(G_OBJECT(texture), "atl-skbitmap", bitmap,
	                       [](gpointer data) { delete (SkBitmap *)data; });
	return bitmap;
}

