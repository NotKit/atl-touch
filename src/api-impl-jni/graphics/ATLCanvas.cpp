#include "ATLCanvas.h"

#include "include/core/SkBBHFactory.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPixelRef.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/ports/SkFontMgr_fontconfig.h"

#include <mutex>
#include <unordered_map>

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
	/* R-tree of op bounds: replaying a partially-visible recording (an
	 * offscreen list row, a damage-clipped frame) then skips the culled ops */
	static SkRTreeFactory rtree_factory;
	atl_canvas->canvas = atl_canvas->recorder->beginRecording(SkRect::MakeLTRB(-1e9f, -1e9f, 1e9f, 1e9f), &rtree_factory);
	return atl_canvas;
}

ATLCanvas *ATLCanvas::new_gpu(GrDirectContext *context, int width, int height)
{
	sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(context, skgpu::Budgeted::kNo,
	                                                    atl_image_info(width, height), 1,
	                                                    kTopLeft_GrSurfaceOrigin, nullptr);
	if (!surface)
		return nullptr;
	surface->getCanvas()->clear(SK_ColorTRANSPARENT);
	ATLCanvas *atl_canvas = new ATLCanvas();
	atl_canvas->surface = surface;
	atl_canvas->canvas = surface->getCanvas();
	return atl_canvas;
}

ATLCanvas::~ATLCanvas()
{
	if (recorder)
		delete recorder; // owns the recording canvas
	else if (!surface) // a surface owns its canvas
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

/* GPU draws: Ganesh caches uploaded textures by SkImage uniqueID, so a fresh
 * wrapper per draw would re-upload every bitmap every frame. Cache one image
 * per bitmap, refreshed when its generation ID changes (bumped by the JNI
 * draw entry points, getPixelsPtr, and skia's own erase/writePixels). */
struct CachedBitmapImage {
	uint32_t generation;
	sk_sp<SkImage> image;
};
static std::unordered_map<SkBitmap *, CachedBitmapImage> bitmap_image_cache;
static std::mutex bitmap_image_cache_lock;

static sk_sp<SkImage> atl_image_cached(SkBitmap *bitmap)
{
	std::lock_guard<std::mutex> guard(bitmap_image_cache_lock);
	uint32_t generation = bitmap->getGenerationID();
	CachedBitmapImage &entry = bitmap_image_cache[bitmap];
	if (!entry.image || entry.generation != generation) {
		SkPixmap pixmap;
		if (!bitmap->peekPixels(&pixmap)) {
			bitmap_image_cache.erase(bitmap);
			return nullptr;
		}
		/* pin the pixel ref: Ganesh may not read the pixels until flush, by
		 * which time the java Bitmap could already have been freed */
		bitmap->pixelRef()->ref();
		entry.image = SkImages::RasterFromPixmap(
		    pixmap,
		    [](const void *, void *pixel_ref) { ((SkPixelRef *)pixel_ref)->unref(); },
		    bitmap->pixelRef());
		entry.generation = generation;
	}
	return entry.image;
}

extern "C" void atl_bitmap_cache_drop(void *sk_bitmap)
{
	std::lock_guard<std::mutex> guard(bitmap_image_cache_lock);
	bitmap_image_cache.erase((SkBitmap *)sk_bitmap);
}

sk_sp<SkImage> atl_image_for_draw(ATLCanvas *atl_canvas, SkBitmap *bitmap)
{
	if (atl_canvas->is_recording())
		/* by-ref like AOSP display lists: no pixel copy per re-record. A view
		 * showing changing pixels must invalidate() (and thus re-record), so
		 * the recording never outlives the content it references. */
		return atl_image_cached(bitmap);
	if (atl_canvas->is_gpu())
		return atl_image_cached(bitmap); // stable uniqueID -> texture cache hits
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

/* --- GPU (Ganesh) rendering --- */

static GrGLFuncPtr atl_gl_get_proc(void *ctx, const char name[])
{
	return (GrGLFuncPtr)((void *(*)(const char *))ctx)(name);
}

extern "C" void *atl_gpu_context_create(void *(*get_proc)(const char *name))
{
	/* the assembled interface probes GL_VERSION itself, so the same call works
	 * on desktop GL and on GLES2 (hybris) contexts */
	sk_sp<const GrGLInterface> interface = GrGLMakeAssembledInterface((void *)get_proc, atl_gl_get_proc);
	if (!interface)
		return nullptr;
	return GrDirectContexts::MakeGL(interface).release();
}

extern "C" void atl_gpu_context_reset(void *context)
{
	((GrDirectContext *)context)->resetContext();
}

extern "C" void *atl_canvas_new_gpu(void *context, int width, int height)
{
	return ATLCanvas::new_gpu((GrDirectContext *)context, width, height);
}

extern "C" void atl_canvas_gpu_present(void *context, void *atl_canvas, int width, int height)
{
	GrDirectContext *direct_context = (GrDirectContext *)context;
	ATLCanvas *canvas = (ATLCanvas *)atl_canvas;
	/* released before the next frame draws into the surface, so this never
	 * triggers a copy-on-write of the backing texture */
	sk_sp<SkImage> frame = canvas->surface->makeImageSnapshot();
	if (!frame)
		return;
	GrGLFramebufferInfo fb_info;
	fb_info.fFBOID = 0;
	fb_info.fFormat = 0x8058; // GL_RGBA8
	GrBackendRenderTarget target = GrBackendRenderTargets::MakeGL(width, height, 1, 0, fb_info);
	sk_sp<SkSurface> backbuffer = SkSurfaces::WrapBackendRenderTarget(
	    direct_context, target, kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, nullptr, nullptr);
	if (!backbuffer) {
		fprintf(stderr, "atl_canvas_gpu_present: wrapping the default framebuffer failed\n");
		return;
	}
	SkPaint paint;
	paint.setBlendMode(SkBlendMode::kSrc); // replicate the raster path's blend-less blit
	backbuffer->getCanvas()->drawImage(frame, 0, 0, SkSamplingOptions(), &paint);
	direct_context->flushAndSubmit();
}

