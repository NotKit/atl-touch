#include <math.h>

#include "../defines.h"
#include "ATLCanvas.h"
#include "AndroidPaint.h"

#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/core/SkRRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/utils/SkTextUtils.h"

extern "C" {
#include "../generated_headers/android_atl_GskCanvas.h"
}

/* the jlong named snapshot_ptr throughout holds an ATLCanvas* (the Java field
 * is still called `snapshot`; renaming it is deferred to a follow-up) */

static const SkSamplingOptions atl_sampling(SkFilterMode::kLinear);

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1drawBitmap__JJIIIIJ(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jlong texture_ptr, jint x, jint y, jint width, jint height, jlong paint_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	SkBitmap *bitmap = (SkBitmap *)_PTR(texture_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	sk_sp<SkImage> image = atl_image_for_draw(atl_canvas, bitmap);
	if (!image)
		return;
	atl_canvas->canvas->drawImageRect(image, SkRect::MakeXYWH(x, y, width, height),
	                                  atl_sampling, &paint->paint);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1drawBitmap__JJIIIIIIIIJ(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jlong texture_ptr, jint x, jint y, jint width, jint height, jint src_x, jint src_y, jint src_width, jint src_height, jlong paint_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	SkBitmap *bitmap = (SkBitmap *)_PTR(texture_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	sk_sp<SkImage> image = atl_image_for_draw(atl_canvas, bitmap);
	if (!image)
		return;
	atl_canvas->canvas->drawImageRect(image, SkRect::MakeXYWH(src_x, src_y, src_width, src_height),
	                                  SkRect::MakeXYWH(x, y, width, height),
	                                  atl_sampling, &paint->paint, SkCanvas::kFast_SrcRectConstraint);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1drawRect(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom, jlong paint_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	atl_canvas->canvas->drawRect(SkRect::MakeLTRB(left, top, right, bottom), paint->paint);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1drawPath(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jlong path_ptr, jlong paint_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	SkPath *path = (SkPath *)_PTR(path_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	atl_canvas->canvas->drawPath(*path, paint->paint);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1translate(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jfloat dx, jfloat dy)
{
	((ATLCanvas *)_PTR(snapshot_ptr))->canvas->translate(dx, dy);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1rotate(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jfloat angle)
{
	((ATLCanvas *)_PTR(snapshot_ptr))->canvas->rotate(angle);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1save(JNIEnv *env, jobject this_class, jlong snapshot_ptr)
{
	((ATLCanvas *)_PTR(snapshot_ptr))->canvas->save();
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1restore(JNIEnv *env, jobject this_class, jlong snapshot_ptr)
{
	((ATLCanvas *)_PTR(snapshot_ptr))->canvas->restore();
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1drawLine(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jfloat x0, jfloat y0, jfloat x1, jfloat y1, jlong paint_ptr)
{
	if (isnan(x0) || isnan(y0) || isnan(x1) || isnan(y1))
		return;
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	atl_canvas->canvas->drawLine(x0, y0, x1, y1, paint->paint);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1drawLines(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jfloatArray points_arr, jint offset, jint count, jlong paint_ptr)
{
	jfloat *points = env->GetFloatArrayElements(points_arr, NULL);
	for (int i = 0; i + 3 < count; i += 4)
		Java_android_atl_GskCanvas_native_1drawLine(env, this_class, snapshot_ptr,
		                                            points[offset + i], points[offset + i + 1],
		                                            points[offset + i + 2], points[offset + i + 3], paint_ptr);
	env->ReleaseFloatArrayElements(points_arr, points, 0);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1drawText(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jstring text, jfloat x, jfloat y, jlong paint_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	const char *str = env->GetStringUTFChars(text, NULL);
	SkTextUtils::DrawString(atl_canvas->canvas, str, x, y, paint->font, paint->paint,
	                        (SkTextUtils::Align)paint->text_align);
	env->ReleaseStringUTFChars(text, str);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1drawRoundRect(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom, jfloat rx, jfloat ry, jlong paint_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	atl_canvas->canvas->drawRRect(SkRRect::MakeRectXY(SkRect::MakeLTRB(left, top, right, bottom), rx, ry), paint->paint);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1scale(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jfloat x, jfloat y)
{
	((ATLCanvas *)_PTR(snapshot_ptr))->canvas->scale(x, y);
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1concat(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jlong matrix_ptr)
{
	((ATLCanvas *)_PTR(snapshot_ptr))->canvas->concat(*(SkMatrix *)_PTR(matrix_ptr));
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1clipRect(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom)
{
	((ATLCanvas *)_PTR(snapshot_ptr))->canvas->clipRect(SkRect::MakeLTRB(left, top, right, bottom));
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1clipPath(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jlong path_ptr)
{
	SkPath *path = (SkPath *)_PTR(path_ptr);
	((ATLCanvas *)_PTR(snapshot_ptr))->canvas->clipPath(*path, true /* doAntiAlias */);
}

/* clips are part of the save/restore stack in Skia (unlike GSK's push/pop),
 * so there is nothing to pop here; the Java side's push counting is harmless */
JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1pop(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jint count)
{
}

JNIEXPORT void JNICALL Java_android_atl_GskCanvas_native_1drawRenderNode(JNIEnv *env, jobject this_class, jlong snapshot_ptr, jlong node_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	ATLNode *node = (ATLNode *)_PTR(node_ptr);
	atl_canvas->canvas->drawDrawable(node);
}
