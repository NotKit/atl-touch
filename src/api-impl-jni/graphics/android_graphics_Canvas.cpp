#include <math.h>
#include <new>

#include "../defines.h"
#include "ATLCanvas.h"
#include "AndroidPaint.h"
#include "../hwui/MinikinGlue.h"

#include "include/core/SkBitmap.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkColor.h"
#include "include/core/SkM44.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathMeasure.h"
#include "include/core/SkRRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkTileMode.h"
#include "include/core/SkVertices.h"
#include "include/utils/SkTextUtils.h"

extern "C" {
#include "../generated_headers/android_graphics_BaseCanvas.h"
#include "../generated_headers/android_graphics_Canvas.h"
}

/*
 * android.graphics.Canvas/BaseCanvas natives (AOSP surface) over ATLCanvas.
 * The jlong canvas handle is an ATLCanvas*; paint is AndroidPaint*;
 * path is SkPath*; matrix is SkMatrix*; bitmap is SkBitmap*.
 */

#define CANVAS(ptr) (((ATLCanvas *)_PTR(ptr))->canvas)

static const SkSamplingOptions atl_sampling(SkFilterMode::kLinear);

/* Android's legacy PorterDuff mode ints match SkBlendMode ordering up to kLighten */
static SkBlendMode porterduff_to_skia(jint mode)
{
	if (mode < 0 || mode > (jint)SkBlendMode::kLighten)
		return SkBlendMode::kSrcOver;
	return (SkBlendMode)mode;
}

/* --- lifecycle --- */

JNIEXPORT jlong JNICALL Java_android_graphics_Canvas_nInitRaster(JNIEnv *env, jclass, jlong bitmap_ptr)
{
	SkBitmap *bitmap = (SkBitmap *)_PTR(bitmap_ptr);
	ATLCanvas *atl_canvas = bitmap ? ATLCanvas::for_bitmap(bitmap) : ATLCanvas::new_raster(1, 1);
	return _INTPTR(atl_canvas);
}

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nSetBitmap(JNIEnv *env, jclass, jlong canvas_ptr, jlong bitmap_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(canvas_ptr);
	SkBitmap *bitmap = (SkBitmap *)_PTR(bitmap_ptr);
	ATLCanvas *fresh = bitmap ? ATLCanvas::for_bitmap(bitmap) : ATLCanvas::new_raster(1, 1);
	std::swap(atl_canvas->canvas, fresh->canvas);
	std::swap(atl_canvas->bitmap, fresh->bitmap);
	std::swap(atl_canvas->owns_bitmap, fresh->owns_bitmap);
	std::swap(atl_canvas->recorder, fresh->recorder);
	atl_canvas->stubs.swap(fresh->stubs);
	delete fresh; // frees the previous backing
}

static void canvas_finalizer(void *ptr)
{
	delete (ATLCanvas *)ptr;
}

JNIEXPORT jlong JNICALL Java_android_graphics_Canvas_nGetNativeFinalizer(JNIEnv *env, jclass)
{
	return _INTPTR(&canvas_finalizer);
}

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nFreeCaches(JNIEnv *env, jclass) {}
JNIEXPORT void JNICALL Java_android_graphics_Canvas_nFreeTextLayoutCaches(JNIEnv *env, jclass) {}
JNIEXPORT void JNICALL Java_android_graphics_Canvas_nSetCompatibilityVersion(JNIEnv *env, jclass, jint api_level) {}
JNIEXPORT void JNICALL Java_android_graphics_Canvas_nSetDrawFilter(JNIEnv *env, jclass, jlong canvas_ptr, jlong filter_ptr) {}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nPunchHole(JNIEnv *env, jclass, jlong canvas_ptr, jfloat l, jfloat t, jfloat r, jfloat b, jfloat rx, jfloat ry) {}

/* --- queries --- */

JNIEXPORT jboolean JNICALL Java_android_graphics_Canvas_nGetClipBounds(JNIEnv *env, jclass, jlong canvas_ptr, jobject rect)
{
	SkIRect bounds = CANVAS(canvas_ptr)->getDeviceClipBounds();
	/* AOSP reports clip bounds in local coordinates */
	SkRect local = SkRect::Make(bounds);
	SkMatrix inverse;
	if (CANVAS(canvas_ptr)->getTotalMatrix().invert(&inverse))
		local = inverse.mapRect(local);
	SkIRect out = local.roundOut();
	jclass rect_class = env->GetObjectClass(rect);
	env->SetIntField(rect, env->GetFieldID(rect_class, "left", "I"), out.left());
	env->SetIntField(rect, env->GetFieldID(rect_class, "top", "I"), out.top());
	env->SetIntField(rect, env->GetFieldID(rect_class, "right", "I"), out.right());
	env->SetIntField(rect, env->GetFieldID(rect_class, "bottom", "I"), out.bottom());
	return !bounds.isEmpty();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Canvas_nIsOpaque(JNIEnv *env, jclass, jlong canvas_ptr)
{
	return CANVAS(canvas_ptr)->imageInfo().isOpaque();
}

JNIEXPORT jint JNICALL Java_android_graphics_Canvas_nGetWidth(JNIEnv *env, jclass, jlong canvas_ptr)
{
	return CANVAS(canvas_ptr)->getBaseLayerSize().width();
}

JNIEXPORT jint JNICALL Java_android_graphics_Canvas_nGetHeight(JNIEnv *env, jclass, jlong canvas_ptr)
{
	return CANVAS(canvas_ptr)->getBaseLayerSize().height();
}

/* --- save/restore --- */

JNIEXPORT jint JNICALL Java_android_graphics_Canvas_nSave(JNIEnv *env, jclass, jlong canvas_ptr, jint save_flags)
{
	return CANVAS(canvas_ptr)->save();
}

JNIEXPORT jint JNICALL Java_android_graphics_Canvas_nSaveLayer(JNIEnv *env, jclass, jlong canvas_ptr, jfloat l, jfloat t, jfloat r, jfloat b, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	SkRect bounds = SkRect::MakeLTRB(l, t, r, b);
	return CANVAS(canvas_ptr)->saveLayer(&bounds, paint ? &paint->paint : nullptr);
}

JNIEXPORT jint JNICALL Java_android_graphics_Canvas_nSaveLayerAlpha(JNIEnv *env, jclass, jlong canvas_ptr, jfloat l, jfloat t, jfloat r, jfloat b, jint alpha)
{
	SkRect bounds = SkRect::MakeLTRB(l, t, r, b);
	return CANVAS(canvas_ptr)->saveLayerAlpha(&bounds, alpha);
}

JNIEXPORT jint JNICALL Java_android_graphics_Canvas_nSaveUnclippedLayer(JNIEnv *env, jclass, jlong canvas_ptr, jint l, jint t, jint r, jint b)
{
	SkRect bounds = SkRect::MakeLTRB(l, t, r, b);
	SkCanvas::SaveLayerRec rec(&bounds, nullptr, SkCanvas::kInitWithPrevious_SaveLayerFlag);
	return CANVAS(canvas_ptr)->saveLayer(rec);
}

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nRestoreUnclippedLayer(JNIEnv *env, jclass, jlong canvas_ptr, jint save_count, jlong paint_ptr)
{
	CANVAS(canvas_ptr)->restoreToCount(save_count);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Canvas_nRestore(JNIEnv *env, jclass, jlong canvas_ptr)
{
	SkCanvas *canvas = CANVAS(canvas_ptr);
	if (canvas->getSaveCount() <= 1)
		return false;
	canvas->restore();
	return true;
}

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nRestoreToCount(JNIEnv *env, jclass, jlong canvas_ptr, jint save_count)
{
	CANVAS(canvas_ptr)->restoreToCount(save_count);
}

JNIEXPORT jint JNICALL Java_android_graphics_Canvas_nGetSaveCount(JNIEnv *env, jclass, jlong canvas_ptr)
{
	return CANVAS(canvas_ptr)->getSaveCount();
}

/* --- matrix --- */

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nTranslate(JNIEnv *env, jclass, jlong canvas_ptr, jfloat dx, jfloat dy)
{
	CANVAS(canvas_ptr)->translate(dx, dy);
}

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nScale(JNIEnv *env, jclass, jlong canvas_ptr, jfloat sx, jfloat sy)
{
	CANVAS(canvas_ptr)->scale(sx, sy);
}

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nRotate(JNIEnv *env, jclass, jlong canvas_ptr, jfloat degrees)
{
	CANVAS(canvas_ptr)->rotate(degrees);
}

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nSkew(JNIEnv *env, jclass, jlong canvas_ptr, jfloat sx, jfloat sy)
{
	CANVAS(canvas_ptr)->skew(sx, sy);
}

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nConcat(JNIEnv *env, jclass, jlong canvas_ptr, jlong matrix_ptr)
{
	if (matrix_ptr)
		CANVAS(canvas_ptr)->concat(*(SkMatrix *)_PTR(matrix_ptr));
}

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nSetMatrix(JNIEnv *env, jclass, jlong canvas_ptr, jlong matrix_ptr)
{
	SkMatrix *matrix = (SkMatrix *)_PTR(matrix_ptr);
	CANVAS(canvas_ptr)->setMatrix(matrix ? SkM44(*matrix) : SkM44());
}

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nGetMatrix(JNIEnv *env, jclass, jlong canvas_ptr, jlong matrix_ptr)
{
	*(SkMatrix *)_PTR(matrix_ptr) = CANVAS(canvas_ptr)->getTotalMatrix();
}

/* --- clip --- */

/* android.graphics.Region.Op: 0 DIFFERENCE, 1 INTERSECT, others unsupported by Skia */
static SkClipOp region_op_to_skia(jint op)
{
	return op == 0 ? SkClipOp::kDifference : SkClipOp::kIntersect;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Canvas_nClipRect(JNIEnv *env, jclass, jlong canvas_ptr, jfloat l, jfloat t, jfloat r, jfloat b, jint region_op)
{
	SkCanvas *canvas = CANVAS(canvas_ptr);
	canvas->clipRect(SkRect::MakeLTRB(l, t, r, b), region_op_to_skia(region_op));
	return !canvas->isClipEmpty();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Canvas_nClipPath(JNIEnv *env, jclass, jlong canvas_ptr, jlong path_ptr, jint region_op)
{
	SkCanvas *canvas = CANVAS(canvas_ptr);
	canvas->clipPath(*(SkPath *)_PTR(path_ptr), region_op_to_skia(region_op), true /* doAntiAlias */);
	return !canvas->isClipEmpty();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Canvas_nQuickReject__JFFFF(JNIEnv *env, jclass, jlong canvas_ptr, jfloat l, jfloat t, jfloat r, jfloat b)
{
	return CANVAS(canvas_ptr)->quickReject(SkRect::MakeLTRB(l, t, r, b));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Canvas_nQuickReject__JJ(JNIEnv *env, jclass, jlong canvas_ptr, jlong path_ptr)
{
	return CANVAS(canvas_ptr)->quickReject(((SkPath *)_PTR(path_ptr))->getBounds());
}

/* --- simple draws --- */

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawColor__JII(JNIEnv *env, jclass, jlong canvas_ptr, jint color, jint mode)
{
	CANVAS(canvas_ptr)->drawColor((SkColor)color, porterduff_to_skia(mode));
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawColor__JJJI(JNIEnv *env, jclass, jlong canvas_ptr, jlong colorspace_ptr, jlong color_long, jint mode)
{
	/* ColorLong path is rejected in Java; keep the symbol resolvable */
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawPaint(JNIEnv *env, jclass, jlong canvas_ptr, jlong paint_ptr)
{
	CANVAS(canvas_ptr)->drawPaint(((AndroidPaint *)_PTR(paint_ptr))->paint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawPoint(JNIEnv *env, jclass, jlong canvas_ptr, jfloat x, jfloat y, jlong paint_ptr)
{
	CANVAS(canvas_ptr)->drawPoint(x, y, ((AndroidPaint *)_PTR(paint_ptr))->paint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawPoints(JNIEnv *env, jclass, jlong canvas_ptr, jfloatArray pts_arr, jint offset, jint count, jlong paint_ptr)
{
	jfloat *pts = env->GetFloatArrayElements(pts_arr, NULL);
	CANVAS(canvas_ptr)->drawPoints(SkCanvas::kPoints_PointMode, count / 2,
	                               (const SkPoint *)(pts + offset),
	                               ((AndroidPaint *)_PTR(paint_ptr))->paint);
	env->ReleaseFloatArrayElements(pts_arr, pts, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawLine(JNIEnv *env, jclass, jlong canvas_ptr, jfloat x0, jfloat y0, jfloat x1, jfloat y1, jlong paint_ptr)
{
	if (isnan(x0) || isnan(y0) || isnan(x1) || isnan(y1))
		return;
	CANVAS(canvas_ptr)->drawLine(x0, y0, x1, y1, ((AndroidPaint *)_PTR(paint_ptr))->paint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawLines(JNIEnv *env, jclass, jlong canvas_ptr, jfloatArray pts_arr, jint offset, jint count, jlong paint_ptr)
{
	jfloat *pts = env->GetFloatArrayElements(pts_arr, NULL);
	CANVAS(canvas_ptr)->drawPoints(SkCanvas::kLines_PointMode, count / 2,
	                               (const SkPoint *)(pts + offset),
	                               ((AndroidPaint *)_PTR(paint_ptr))->paint);
	env->ReleaseFloatArrayElements(pts_arr, pts, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawRect(JNIEnv *env, jclass, jlong canvas_ptr, jfloat l, jfloat t, jfloat r, jfloat b, jlong paint_ptr)
{
	CANVAS(canvas_ptr)->drawRect(SkRect::MakeLTRB(l, t, r, b), ((AndroidPaint *)_PTR(paint_ptr))->paint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawOval(JNIEnv *env, jclass, jlong canvas_ptr, jfloat l, jfloat t, jfloat r, jfloat b, jlong paint_ptr)
{
	CANVAS(canvas_ptr)->drawOval(SkRect::MakeLTRB(l, t, r, b), ((AndroidPaint *)_PTR(paint_ptr))->paint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawCircle(JNIEnv *env, jclass, jlong canvas_ptr, jfloat cx, jfloat cy, jfloat radius, jlong paint_ptr)
{
	CANVAS(canvas_ptr)->drawCircle(cx, cy, radius, ((AndroidPaint *)_PTR(paint_ptr))->paint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawArc(JNIEnv *env, jclass, jlong canvas_ptr, jfloat l, jfloat t, jfloat r, jfloat b, jfloat start_angle, jfloat sweep_angle, jboolean use_center, jlong paint_ptr)
{
	CANVAS(canvas_ptr)->drawArc(SkRect::MakeLTRB(l, t, r, b), start_angle, sweep_angle,
	                            use_center, ((AndroidPaint *)_PTR(paint_ptr))->paint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawRoundRect(JNIEnv *env, jclass, jlong canvas_ptr, jfloat l, jfloat t, jfloat r, jfloat b, jfloat rx, jfloat ry, jlong paint_ptr)
{
	CANVAS(canvas_ptr)->drawRRect(SkRRect::MakeRectXY(SkRect::MakeLTRB(l, t, r, b), rx, ry),
	                              ((AndroidPaint *)_PTR(paint_ptr))->paint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawDoubleRoundRect__JFFFFFFFFFFFFJ(JNIEnv *env, jclass, jlong canvas_ptr, jfloat ol, jfloat ot, jfloat or_, jfloat ob, jfloat orx, jfloat ory, jfloat il, jfloat it, jfloat ir, jfloat ib, jfloat irx, jfloat iry, jlong paint_ptr)
{
	SkRRect outer = SkRRect::MakeRectXY(SkRect::MakeLTRB(ol, ot, or_, ob), orx, ory);
	SkRRect inner = SkRRect::MakeRectXY(SkRect::MakeLTRB(il, it, ir, ib), irx, iry);
	CANVAS(canvas_ptr)->drawDRRect(outer, inner, ((AndroidPaint *)_PTR(paint_ptr))->paint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawDoubleRoundRect__JFFFF_3FFFFF_3FJ(JNIEnv *env, jclass, jlong canvas_ptr, jfloat ol, jfloat ot, jfloat or_, jfloat ob, jfloatArray outer_radii_arr, jfloat il, jfloat it, jfloat ir, jfloat ib, jfloatArray inner_radii_arr, jlong paint_ptr)
{
	jfloat *outer_radii = env->GetFloatArrayElements(outer_radii_arr, NULL);
	jfloat *inner_radii = env->GetFloatArrayElements(inner_radii_arr, NULL);
	SkRRect outer, inner;
	outer.setRectRadii(SkRect::MakeLTRB(ol, ot, or_, ob), (const SkVector *)outer_radii);
	inner.setRectRadii(SkRect::MakeLTRB(il, it, ir, ib), (const SkVector *)inner_radii);
	CANVAS(canvas_ptr)->drawDRRect(outer, inner, ((AndroidPaint *)_PTR(paint_ptr))->paint);
	env->ReleaseFloatArrayElements(outer_radii_arr, outer_radii, JNI_ABORT);
	env->ReleaseFloatArrayElements(inner_radii_arr, inner_radii, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawPath(JNIEnv *env, jclass, jlong canvas_ptr, jlong path_ptr, jlong paint_ptr)
{
	CANVAS(canvas_ptr)->drawPath(*(SkPath *)_PTR(path_ptr), ((AndroidPaint *)_PTR(paint_ptr))->paint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawRegion(JNIEnv *env, jclass, jlong canvas_ptr, jlong region_ptr, jlong paint_ptr)
{
	/* no callers: the Region fast path was removed from BaseCanvas.drawPath */
}

/* --- bitmaps --- */

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawBitmap__JJFFJIII(JNIEnv *env, jclass, jlong canvas_ptr, jlong bitmap_ptr, jfloat left, jfloat top, jlong paint_ptr, jint canvas_density, jint bitmap_density, jint screen_density)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(canvas_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	sk_sp<SkImage> image = atl_image_for_draw(atl_canvas, (SkBitmap *)_PTR(bitmap_ptr));
	if (!image)
		return;
	/* density scaling deliberately not applied: everything runs at DENSITY_NONE */
	atl_canvas->canvas->drawImage(image, left, top, atl_sampling, paint ? &paint->paint : nullptr);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawBitmap__JJFFFFFFFFJII(JNIEnv *env, jclass, jlong canvas_ptr, jlong bitmap_ptr, jfloat src_l, jfloat src_t, jfloat src_r, jfloat src_b, jfloat dst_l, jfloat dst_t, jfloat dst_r, jfloat dst_b, jlong paint_ptr, jint screen_density, jint bitmap_density)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(canvas_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	sk_sp<SkImage> image = atl_image_for_draw(atl_canvas, (SkBitmap *)_PTR(bitmap_ptr));
	if (!image)
		return;
	atl_canvas->canvas->drawImageRect(image, SkRect::MakeLTRB(src_l, src_t, src_r, src_b),
	                                  SkRect::MakeLTRB(dst_l, dst_t, dst_r, dst_b),
	                                  atl_sampling, paint ? &paint->paint : nullptr,
	                                  SkCanvas::kFast_SrcRectConstraint);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawBitmap__J_3IIIFFIIZJ(JNIEnv *env, jclass, jlong canvas_ptr, jintArray colors_arr, jint offset, jint stride, jfloat x, jfloat y, jint width, jint height, jboolean has_alpha, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	jint *colors = env->GetIntArrayElements(colors_arr, NULL);
	SkBitmap bitmap;
	bitmap.allocPixels(SkImageInfo::MakeN32(width, height,
	                                        has_alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType));
	for (int row = 0; row < height; row++) {
		SkColor *dst = (SkColor *)((char *)bitmap.getPixels() + row * bitmap.rowBytes());
		const jint *src = colors + offset + row * stride;
		for (int col = 0; col < width; col++)
			dst[col] = SkPreMultiplyColor(has_alpha ? (SkColor)src[col] : (SkColor)src[col] | 0xFF000000);
	}
	env->ReleaseIntArrayElements(colors_arr, colors, JNI_ABORT);
	bitmap.setImmutable();
	CANVAS(canvas_ptr)->drawImage(bitmap.asImage(), x, y, atl_sampling, paint ? &paint->paint : nullptr);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawBitmapMatrix(JNIEnv *env, jclass, jlong canvas_ptr, jlong bitmap_ptr, jlong matrix_ptr, jlong paint_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(canvas_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	sk_sp<SkImage> image = atl_image_for_draw(atl_canvas, (SkBitmap *)_PTR(bitmap_ptr));
	if (!image)
		return;
	SkCanvas *canvas = atl_canvas->canvas;
	canvas->save();
	if (matrix_ptr)
		canvas->concat(*(SkMatrix *)_PTR(matrix_ptr));
	canvas->drawImage(image, 0, 0, atl_sampling, paint ? &paint->paint : nullptr);
	canvas->restore();
}

/* --- vertices / mesh --- */

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawVertices(JNIEnv *env, jclass, jlong canvas_ptr, jint mode, jint value_count, jfloatArray verts_arr, jint vert_offset, jfloatArray texs_arr, jint tex_offset, jintArray colors_arr, jint color_offset, jshortArray indices_arr, jint index_offset, jint index_count, jlong paint_ptr)
{
	int vertex_count = value_count / 2;
	jfloat *verts = env->GetFloatArrayElements(verts_arr, NULL);
	jfloat *texs = texs_arr ? env->GetFloatArrayElements(texs_arr, NULL) : nullptr;
	jint *colors = colors_arr ? env->GetIntArrayElements(colors_arr, NULL) : nullptr;
	jshort *indices = indices_arr ? env->GetShortArrayElements(indices_arr, NULL) : nullptr;

	SkVertices::VertexMode vertex_mode =
	    mode == 0 ? SkVertices::kTriangles_VertexMode :
	    mode == 1 ? SkVertices::kTriangleStrip_VertexMode :
	                SkVertices::kTriangleFan_VertexMode;
	sk_sp<SkVertices> vertices = SkVertices::MakeCopy(
	    vertex_mode, vertex_count,
	    (const SkPoint *)(verts + vert_offset),
	    texs ? (const SkPoint *)(texs + tex_offset) : nullptr,
	    colors ? (const SkColor *)(colors + color_offset) : nullptr,
	    indices ? index_count : 0,
	    indices ? (const uint16_t *)(indices + index_offset) : nullptr);

	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	CANVAS(canvas_ptr)->drawVertices(vertices, SkBlendMode::kModulate, paint->paint);

	env->ReleaseFloatArrayElements(verts_arr, verts, JNI_ABORT);
	if (texs)
		env->ReleaseFloatArrayElements(texs_arr, texs, JNI_ABORT);
	if (colors)
		env->ReleaseIntArrayElements(colors_arr, colors, JNI_ABORT);
	if (indices)
		env->ReleaseShortArrayElements(indices_arr, indices, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawBitmapMesh(JNIEnv *env, jclass, jlong canvas_ptr, jlong bitmap_ptr, jint mesh_width, jint mesh_height, jfloatArray verts_arr, jint vert_offset, jintArray colors_arr, jint color_offset, jlong paint_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(canvas_ptr);
	sk_sp<SkImage> image = atl_image_for_draw(atl_canvas, (SkBitmap *)_PTR(bitmap_ptr));
	if (!image)
		return;

	const int cols = mesh_width + 1, rows = mesh_height + 1;
	const int vertex_count = cols * rows;
	const int index_count = mesh_width * mesh_height * 6;
	jfloat *verts = env->GetFloatArrayElements(verts_arr, NULL);
	jint *colors = colors_arr ? env->GetIntArrayElements(colors_arr, NULL) : nullptr;

	std::vector<SkPoint> texs(vertex_count);
	std::vector<uint16_t> indices(index_count);
	const float w = image->width(), h = image->height();
	for (int row = 0; row < rows; row++)
		for (int col = 0; col < cols; col++)
			texs[row * cols + col] = SkPoint::Make(w * col / mesh_width, h * row / mesh_height);
	int i = 0;
	for (int row = 0; row < mesh_height; row++) {
		for (int col = 0; col < mesh_width; col++) {
			uint16_t tl = row * cols + col, tr = tl + 1, bl = tl + cols, br = bl + 1;
			indices[i++] = tl; indices[i++] = bl; indices[i++] = tr;
			indices[i++] = tr; indices[i++] = bl; indices[i++] = br;
		}
	}

	sk_sp<SkVertices> vertices = SkVertices::MakeCopy(
	    SkVertices::kTriangles_VertexMode, vertex_count,
	    (const SkPoint *)(verts + vert_offset), texs.data(),
	    colors ? (const SkColor *)(colors + color_offset) : nullptr,
	    index_count, indices.data());

	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	SkPaint sk_paint = paint ? paint->paint : SkPaint();
	sk_paint.setShader(image->makeShader(SkTileMode::kClamp, SkTileMode::kClamp, atl_sampling));
	CANVAS(canvas_ptr)->drawVertices(vertices, SkBlendMode::kModulate, sk_paint);

	env->ReleaseFloatArrayElements(verts_arr, verts, JNI_ABORT);
	if (colors)
		env->ReleaseIntArrayElements(colors_arr, colors, JNI_ABORT);
}

/* --- text --- */

static void draw_text_utf16(SkCanvas *canvas, const jchar *chars, int count, float x, float y, AndroidPaint *paint)
{
	/* shaping + font fallback via minikin (alignment handled inside) */
	android::minikin_draw_text(canvas, paint, chars, count, minikin::Bidi::LTR, x, y);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawText__J_3CIIFFIJ(JNIEnv *env, jclass, jlong canvas_ptr, jcharArray text_arr, jint index, jint count, jfloat x, jfloat y, jint bidi_flags, jlong paint_ptr)
{
	jchar *chars = env->GetCharArrayElements(text_arr, NULL);
	draw_text_utf16(CANVAS(canvas_ptr), chars + index, count, x, y, (AndroidPaint *)_PTR(paint_ptr));
	env->ReleaseCharArrayElements(text_arr, chars, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawText__JLjava_lang_String_2IIFFIJ(JNIEnv *env, jclass, jlong canvas_ptr, jstring text, jint start, jint end, jfloat x, jfloat y, jint bidi_flags, jlong paint_ptr)
{
	const jchar *chars = env->GetStringChars(text, NULL);
	draw_text_utf16(CANVAS(canvas_ptr), chars + start, end - start, x, y, (AndroidPaint *)_PTR(paint_ptr));
	env->ReleaseStringChars(text, chars);
}

/* no shaping-context support (that needs minikin); draw the run itself */
JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawTextRun__JLjava_lang_String_2IIIIFFZJ(JNIEnv *env, jclass, jlong canvas_ptr, jstring text, jint start, jint end, jint context_start, jint context_end, jfloat x, jfloat y, jboolean is_rtl, jlong paint_ptr)
{
	const jchar *chars = env->GetStringChars(text, NULL);
	draw_text_utf16(CANVAS(canvas_ptr), chars + start, end - start, x, y, (AndroidPaint *)_PTR(paint_ptr));
	env->ReleaseStringChars(text, chars);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawTextRun__J_3CIIIIFFZJJ(JNIEnv *env, jclass, jlong canvas_ptr, jcharArray text_arr, jint start, jint count, jint context_start, jint context_count, jfloat x, jfloat y, jboolean is_rtl, jlong paint_ptr, jlong precomputed_ptr)
{
	jchar *chars = env->GetCharArrayElements(text_arr, NULL);
	draw_text_utf16(CANVAS(canvas_ptr), chars + start, count, x, y, (AndroidPaint *)_PTR(paint_ptr));
	env->ReleaseCharArrayElements(text_arr, chars, JNI_ABORT);
}

static void draw_text_on_path_utf16(SkCanvas *canvas, const jchar *chars, int count, SkPath *path, float h_offset, float v_offset, AndroidPaint *paint)
{
	SkPathMeasure measure(*path, false);
	float distance = h_offset;
	for (int i = 0; i < count; i++) {
		/* skip low surrogates; the high surrogate carries the pair */
		if ((chars[i] & 0xFC00) == 0xDC00)
			continue;
		int glyph_len = ((chars[i] & 0xFC00) == 0xD800 && i + 1 < count) ? 2 : 1;
		SkScalar advance = paint->font.measureText(chars + i, glyph_len * sizeof(jchar),
		                                           SkTextEncoding::kUTF16);
		SkPoint pos;
		SkVector tan;
		if (measure.getPosTan(distance + advance / 2, &pos, &tan)) {
			canvas->save();
			canvas->translate(pos.x(), pos.y());
			canvas->rotate(atan2f(tan.y(), tan.x()) * 180.0f / M_PI);
			canvas->drawSimpleText(chars + i, glyph_len * sizeof(jchar), SkTextEncoding::kUTF16,
			                       -advance / 2, v_offset, paint->font, paint->paint);
			canvas->restore();
		}
		distance += advance;
	}
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawTextOnPath__J_3CIIJFFIJ(JNIEnv *env, jclass, jlong canvas_ptr, jcharArray text_arr, jint index, jint count, jlong path_ptr, jfloat h_offset, jfloat v_offset, jint bidi_flags, jlong paint_ptr)
{
	jchar *chars = env->GetCharArrayElements(text_arr, NULL);
	draw_text_on_path_utf16(CANVAS(canvas_ptr), chars + index, count, (SkPath *)_PTR(path_ptr),
	                        h_offset, v_offset, (AndroidPaint *)_PTR(paint_ptr));
	env->ReleaseCharArrayElements(text_arr, chars, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_graphics_BaseCanvas_nDrawTextOnPath__JLjava_lang_String_2JFFIJ(JNIEnv *env, jclass, jlong canvas_ptr, jstring text, jlong path_ptr, jfloat h_offset, jfloat v_offset, jint bidi_flags, jlong paint_ptr)
{
	const jchar *chars = env->GetStringChars(text, NULL);
	draw_text_on_path_utf16(CANVAS(canvas_ptr), chars, env->GetStringLength(text), (SkPath *)_PTR(path_ptr),
	                        h_offset, v_offset, (AndroidPaint *)_PTR(paint_ptr));
	env->ReleaseStringChars(text, chars);
}

/* --- render nodes --- */

JNIEXPORT void JNICALL Java_android_graphics_Canvas_nDrawRenderNode(JNIEnv *env, jclass, jlong canvas_ptr, jlong node_ptr)
{
	CANVAS(canvas_ptr)->drawDrawable((ATLNode *)_PTR(node_ptr));
}
