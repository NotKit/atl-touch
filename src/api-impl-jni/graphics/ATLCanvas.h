#ifndef ATL_CANVAS_H
#define ATL_CANVAS_H

/*
 * ATLCanvas: the native object behind android.atl.GskCanvas's `snapshot` field
 * (kept under that name for now) and android.graphics.Bitmap's drawing context.
 *
 * It always exposes an SkCanvas, backed by one of:
 *  - a raster SkBitmap it owns (widget rendering, SurfaceView, paintables)
 *  - an external SkBitmap owned by a java Bitmap
 *  - an SkPictureRecorder (RenderNode display list recording)
 *
 * ATLNode is the native object behind android.view.RenderNode handles: a
 * patchable display list built on SkDrawable so that child nodes can be
 * swapped without re-recording the parent (the GSK render node tree allowed
 * structural patching; SkPicture alone does not).
 */

#ifdef __cplusplus

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkDrawable.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkPictureRecorder.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"

#include <vector>

class GrDirectContext;
struct ATLNode;

struct ATLCanvas {
	SkCanvas *canvas = nullptr; // valid in all modes; not owned in recording/GPU mode
	SkBitmap *bitmap = nullptr;
	bool owns_bitmap = false;
	SkPictureRecorder *recorder = nullptr;
	sk_sp<SkSurface> surface; // GPU mode: persistent Ganesh render target
	std::vector<sk_sp<ATLNode>> stubs; // stub nodes recorded into a display list

	static ATLCanvas *new_raster(int width, int height);
	static ATLCanvas *for_bitmap(SkBitmap *bitmap);
	static ATLCanvas *new_recording(void);
	static ATLCanvas *new_gpu(GrDirectContext *context, int width, int height);
	bool is_recording() const { return recorder != nullptr; }
	bool is_gpu() const { return surface != nullptr; }
	/* the GPU path caches an SkImage per bitmap keyed by generation ID; draws
	 * INTO a bitmap-backed canvas must bump it or those caches go stale */
	void mark_dirty() { if (bitmap) bitmap->notifyPixelsChanged(); }
	~ATLCanvas();
};

struct ATLNode : public SkDrawable {
	/* content node: a finished recording plus the stubs referenced by it */
	sk_sp<SkDrawable> recording;
	std::vector<sk_sp<ATLNode>> stubs;

	/* stub node: live indirection to a child node, retargetable via patching */
	bool is_stub = false;
	sk_sp<ATLNode> target;

	/* wrapper node: transform and/or clip applied around a child node */
	sk_sp<ATLNode> child;
	SkMatrix matrix = SkMatrix::I();
	bool has_clip = false;
	SkRect clip = SkRect::MakeEmpty();

	/* retarget all stubs (recursively reachable ones included) currently
	 * pointing at old_child (or being old_child itself) to new_child;
	 * returns true if anything was patched */
	bool patch(ATLNode *old_child, ATLNode *new_child);

protected:
	void onDraw(SkCanvas *canvas) override;
	SkRect onGetBounds() override { return SkRect::MakeLTRB(-1e9f, -1e9f, 1e9f, 1e9f); }
};

/* wrap a bitmap for drawing onto the given canvas: zero-copy for immediate
 * raster canvases, copying for recording canvases (the picture outlives the
 * draw call, so it must not borrow pixels) */
sk_sp<SkImage> atl_image_for_draw(ATLCanvas *atl_canvas, SkBitmap *bitmap);

sk_sp<SkFontMgr> atl_fontmgr(void);
sk_sp<SkTypeface> atl_default_typeface(void);

extern "C" {
#endif

/* C bridge for windowing code */
void *atl_canvas_new_raster(int width, int height);
void atl_canvas_free(void *atl_canvas);
/* direct access to a raster canvas's RGBA pixels (premultiplied) */
const void *atl_canvas_get_pixels(void *atl_canvas, int *width, int *height, int *stride);
/* clip a persistent raster canvas to this frame's damage rect and clear it */
void atl_canvas_begin_frame(void *atl_canvas, int left, int top, int right, int bottom);
/* pop the damage clip (and any unbalanced app saves) after the frame */
void atl_canvas_end_frame(void *atl_canvas);

/* --- GPU (Ganesh) rendering; the GL context must be current on this thread ---
 * get_proc resolves GL symbols (glfwGetProcAddress); returns NULL on failure */
void *atl_gpu_context_create(void *(*get_proc)(const char *name));
/* mark all cached GL state stale (call after foreign GL use, e.g. WebView) */
void atl_gpu_context_reset(void *context);
/* persistent GPU canvas; returns NULL if the surface can't be created */
void *atl_canvas_new_gpu(void *context, int width, int height);
/* flush the frame and blit the GPU canvas onto the default framebuffer */
void atl_canvas_gpu_present(void *context, void *atl_canvas, int width, int height);
/* forget the cached GPU image for a bitmap about to be freed */
void atl_bitmap_cache_drop(void *sk_bitmap);

#ifdef __cplusplus
}
#endif

#endif
