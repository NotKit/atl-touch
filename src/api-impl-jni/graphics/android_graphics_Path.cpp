#include <vector>

#include "../defines.h"

#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathMeasure.h"
#include "include/core/SkPathTypes.h"
#include "include/core/SkRect.h"
#include "include/pathops/SkPathOps.h"

extern "C" {
#include "../generated_headers/android_graphics_Path.h"
}

/*
 * android.graphics.Path natives (AOSP surface) over a plain SkPath*.
 * Direction, FillType and Op nativeInt values match the Skia enums, as they
 * do in AOSP's hwui.
 */

#define PATH(ptr) ((SkPath *)_PTR(ptr))

JNIEXPORT jlong JNICALL Java_android_graphics_Path_nInit__(JNIEnv *env, jclass)
{
	return _INTPTR(new SkPath());
}

JNIEXPORT jlong JNICALL Java_android_graphics_Path_nInit__J(JNIEnv *env, jclass, jlong src_ptr)
{
	return _INTPTR(new SkPath(*PATH(src_ptr)));
}

static void path_finalizer(void *ptr)
{
	delete (SkPath *)ptr;
}

JNIEXPORT jlong JNICALL Java_android_graphics_Path_nGetFinalizer(JNIEnv *env, jclass)
{
	return _INTPTR(&path_finalizer);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nSet(JNIEnv *env, jclass, jlong dst_ptr, jlong src_ptr)
{
	*PATH(dst_ptr) = *PATH(src_ptr);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nComputeBounds(JNIEnv *env, jclass, jlong path_ptr, jobject bounds)
{
	const SkRect &rect = PATH(path_ptr)->getBounds();
	jclass rectf_class = env->GetObjectClass(bounds);
	env->SetFloatField(bounds, env->GetFieldID(rectf_class, "left", "F"), rect.left());
	env->SetFloatField(bounds, env->GetFieldID(rectf_class, "top", "F"), rect.top());
	env->SetFloatField(bounds, env->GetFieldID(rectf_class, "right", "F"), rect.right());
	env->SetFloatField(bounds, env->GetFieldID(rectf_class, "bottom", "F"), rect.bottom());
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nIncReserve(JNIEnv *env, jclass, jlong path_ptr, jint extra)
{
	PATH(path_ptr)->incReserve(extra);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nMoveTo(JNIEnv *env, jclass, jlong path_ptr, jfloat x, jfloat y)
{
	PATH(path_ptr)->moveTo(x, y);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nRMoveTo(JNIEnv *env, jclass, jlong path_ptr, jfloat dx, jfloat dy)
{
	PATH(path_ptr)->rMoveTo(dx, dy);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nLineTo(JNIEnv *env, jclass, jlong path_ptr, jfloat x, jfloat y)
{
	PATH(path_ptr)->lineTo(x, y);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nRLineTo(JNIEnv *env, jclass, jlong path_ptr, jfloat dx, jfloat dy)
{
	PATH(path_ptr)->rLineTo(dx, dy);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nQuadTo(JNIEnv *env, jclass, jlong path_ptr, jfloat x1, jfloat y1, jfloat x2, jfloat y2)
{
	PATH(path_ptr)->quadTo(x1, y1, x2, y2);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nRQuadTo(JNIEnv *env, jclass, jlong path_ptr, jfloat dx1, jfloat dy1, jfloat dx2, jfloat dy2)
{
	PATH(path_ptr)->rQuadTo(dx1, dy1, dx2, dy2);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nCubicTo(JNIEnv *env, jclass, jlong path_ptr, jfloat x1, jfloat y1, jfloat x2, jfloat y2, jfloat x3, jfloat y3)
{
	PATH(path_ptr)->cubicTo(x1, y1, x2, y2, x3, y3);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nRCubicTo(JNIEnv *env, jclass, jlong path_ptr, jfloat dx1, jfloat dy1, jfloat dx2, jfloat dy2, jfloat dx3, jfloat dy3)
{
	PATH(path_ptr)->rCubicTo(dx1, dy1, dx2, dy2, dx3, dy3);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nArcTo(JNIEnv *env, jclass, jlong path_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom, jfloat start_angle, jfloat sweep_angle, jboolean force_move_to)
{
	PATH(path_ptr)->arcTo(SkRect::MakeLTRB(left, top, right, bottom), start_angle, sweep_angle, force_move_to);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nClose(JNIEnv *env, jclass, jlong path_ptr)
{
	PATH(path_ptr)->close();
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nAddRect(JNIEnv *env, jclass, jlong path_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom, jint dir)
{
	PATH(path_ptr)->addRect(SkRect::MakeLTRB(left, top, right, bottom), (SkPathDirection)dir);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nAddOval(JNIEnv *env, jclass, jlong path_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom, jint dir)
{
	PATH(path_ptr)->addOval(SkRect::MakeLTRB(left, top, right, bottom), (SkPathDirection)dir);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nAddCircle(JNIEnv *env, jclass, jlong path_ptr, jfloat x, jfloat y, jfloat radius, jint dir)
{
	PATH(path_ptr)->addCircle(x, y, radius, (SkPathDirection)dir);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nAddArc(JNIEnv *env, jclass, jlong path_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom, jfloat start_angle, jfloat sweep_angle)
{
	PATH(path_ptr)->addArc(SkRect::MakeLTRB(left, top, right, bottom), start_angle, sweep_angle);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nAddRoundRect__JFFFFFFI(JNIEnv *env, jclass, jlong path_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom, jfloat rx, jfloat ry, jint dir)
{
	PATH(path_ptr)->addRoundRect(SkRect::MakeLTRB(left, top, right, bottom), rx, ry, (SkPathDirection)dir);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nAddRoundRect__JFFFF_3FI(JNIEnv *env, jclass, jlong path_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom, jfloatArray radii_arr, jint dir)
{
	jfloat *radii = env->GetFloatArrayElements(radii_arr, NULL);
	PATH(path_ptr)->addRoundRect(SkRect::MakeLTRB(left, top, right, bottom),
	                             (const SkScalar *)radii, (SkPathDirection)dir);
	env->ReleaseFloatArrayElements(radii_arr, radii, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nAddPath__JJFF(JNIEnv *env, jclass, jlong path_ptr, jlong src_ptr, jfloat dx, jfloat dy)
{
	PATH(path_ptr)->addPath(*PATH(src_ptr), dx, dy);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nAddPath__JJ(JNIEnv *env, jclass, jlong path_ptr, jlong src_ptr)
{
	PATH(path_ptr)->addPath(*PATH(src_ptr));
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nAddPath__JJJ(JNIEnv *env, jclass, jlong path_ptr, jlong src_ptr, jlong matrix_ptr)
{
	PATH(path_ptr)->addPath(*PATH(src_ptr), matrix_ptr ? *(SkMatrix *)_PTR(matrix_ptr) : SkMatrix::I());
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nOffset(JNIEnv *env, jclass, jlong path_ptr, jfloat dx, jfloat dy)
{
	PATH(path_ptr)->offset(dx, dy);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nSetLastPoint(JNIEnv *env, jclass, jlong path_ptr, jfloat x, jfloat y)
{
	PATH(path_ptr)->setLastPt(x, y);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nTransform__JJJ(JNIEnv *env, jclass, jlong path_ptr, jlong matrix_ptr, jlong dst_ptr)
{
	PATH(path_ptr)->transform(*(SkMatrix *)_PTR(matrix_ptr), dst_ptr ? PATH(dst_ptr) : PATH(path_ptr));
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nTransform__JJ(JNIEnv *env, jclass, jlong path_ptr, jlong matrix_ptr)
{
	PATH(path_ptr)->transform(*(SkMatrix *)_PTR(matrix_ptr));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Path_nOp(JNIEnv *env, jclass, jlong path1_ptr, jlong path2_ptr, jint op, jlong result_ptr)
{
	return Op(*PATH(path1_ptr), *PATH(path2_ptr), (SkPathOp)op, PATH(result_ptr));
}

/* AOSP semantics: sample the path into (fraction, x, y) triples no further
 * apart than acceptableError allows; used by path-based animation */
JNIEXPORT jfloatArray JNICALL Java_android_graphics_Path_nApproximate(JNIEnv *env, jclass, jlong path_ptr, jfloat acceptable_error)
{
	SkPathMeasure measure(*PATH(path_ptr), false);
	SkScalar total_length = 0;
	std::vector<SkScalar> contour_lengths;
	do {
		contour_lengths.push_back(measure.getLength());
		total_length += contour_lengths.back();
	} while (measure.nextContour());

	std::vector<jfloat> out;
	if (total_length == 0) {
		SkPoint pt = PATH(path_ptr)->countPoints() ? PATH(path_ptr)->getPoint(0) : SkPoint::Make(0, 0);
		out = {0, pt.x(), pt.y(), 1, pt.x(), pt.y()};
	} else {
		SkPathMeasure walk(*PATH(path_ptr), false);
		SkScalar step = std::max(acceptable_error, total_length / 128);
		SkScalar walked = 0;
		for (size_t contour = 0; contour < contour_lengths.size(); contour++) {
			SkScalar length = contour_lengths[contour];
			for (SkScalar d = 0;; d += step) {
				if (d > length)
					d = length;
				SkPoint pos;
				if (walk.getPosTan(d, &pos, nullptr)) {
					out.push_back((walked + d) / total_length);
					out.push_back(pos.x());
					out.push_back(pos.y());
				}
				if (d >= length)
					break;
			}
			walked += length;
			walk.nextContour();
		}
	}

	jfloatArray result = env->NewFloatArray(out.size());
	env->SetFloatArrayRegion(result, 0, out.size(), out.data());
	return result;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Path_nIsRect(JNIEnv *env, jclass, jlong path_ptr, jobject rect)
{
	SkRect sk_rect;
	if (!PATH(path_ptr)->isRect(&sk_rect))
		return false;
	if (rect) {
		jclass rectf_class = env->GetObjectClass(rect);
		env->SetFloatField(rect, env->GetFieldID(rectf_class, "left", "F"), sk_rect.left());
		env->SetFloatField(rect, env->GetFieldID(rectf_class, "top", "F"), sk_rect.top());
		env->SetFloatField(rect, env->GetFieldID(rectf_class, "right", "F"), sk_rect.right());
		env->SetFloatField(rect, env->GetFieldID(rectf_class, "bottom", "F"), sk_rect.bottom());
	}
	return true;
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nReset(JNIEnv *env, jclass, jlong path_ptr)
{
	PATH(path_ptr)->reset();
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nRewind(JNIEnv *env, jclass, jlong path_ptr)
{
	PATH(path_ptr)->rewind();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Path_nIsEmpty(JNIEnv *env, jclass, jlong path_ptr)
{
	return PATH(path_ptr)->isEmpty();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Path_nIsConvex(JNIEnv *env, jclass, jlong path_ptr)
{
	return PATH(path_ptr)->isConvex();
}

JNIEXPORT jint JNICALL Java_android_graphics_Path_nGetFillType(JNIEnv *env, jclass, jlong path_ptr)
{
	return (jint)PATH(path_ptr)->getFillType();
}

JNIEXPORT void JNICALL Java_android_graphics_Path_nSetFillType(JNIEnv *env, jclass, jlong path_ptr, jint fill_type)
{
	PATH(path_ptr)->setFillType((SkPathFillType)fill_type);
}
