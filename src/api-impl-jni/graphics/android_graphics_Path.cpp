#include "../defines.h"

#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/core/SkRRect.h"

extern "C" {
#include "../generated_headers/android_graphics_Path.h"
}

/*
 * android.graphics.Path keeps the GskPath/GskPathBuilder duality protocol on
 * the Java side; natively both handles are simply SkPath. "Building" a path
 * from a builder snapshots it (copy) and resets the builder, matching the
 * gsk_path_builder_to_path() semantics the Java protocol relies on.
 */

JNIEXPORT jlong JNICALL Java_android_graphics_Path_native_1create_1builder(JNIEnv *env, jclass clazz, jlong path_ptr, jlong builder_ptr)
{
	SkPath *builder = (SkPath *)_PTR(builder_ptr);
	if (!builder)
		builder = new SkPath();
	if (path_ptr) {
		SkPath *path = (SkPath *)_PTR(path_ptr);
		builder->addPath(*path);
		delete path;
	}
	return _INTPTR(builder);
}

JNIEXPORT jlong JNICALL Java_android_graphics_Path_native_1create_1path(JNIEnv *env, jclass clazz, jlong builder_ptr)
{
	SkPath *builder = (SkPath *)_PTR(builder_ptr);
	if (!builder)
		return _INTPTR(new SkPath());
	SkPath *path = new SkPath(*builder);
	builder->reset();
	return _INTPTR(path);
}

JNIEXPORT jlong JNICALL Java_android_graphics_Path_native_1ref_1path(JNIEnv *env, jclass clazz, jlong path_ptr)
{
	return _INTPTR(new SkPath(*(SkPath *)_PTR(path_ptr)));
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1reset(JNIEnv *env, jclass clazz, jlong path_ptr, jlong builder_ptr)
{
	if (path_ptr)
		delete (SkPath *)_PTR(path_ptr);
	if (builder_ptr)
		delete (SkPath *)_PTR(builder_ptr);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1close(JNIEnv *env, jclass clazz, jlong builder_ptr)
{
	((SkPath *)_PTR(builder_ptr))->close();
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1move_1to(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat x, jfloat y)
{
	((SkPath *)_PTR(builder_ptr))->moveTo(x, y);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1line_1to(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat x, jfloat y)
{
	((SkPath *)_PTR(builder_ptr))->lineTo(x, y);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1arc_1to(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom,
                                                                  jfloat start_angle_deg, jfloat sweep_angle_deg, jboolean force_move_to)
{
	((SkPath *)_PTR(builder_ptr))->arcTo(SkRect::MakeLTRB(left, top, right, bottom), start_angle_deg, sweep_angle_deg, force_move_to);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1cubic_1to(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat x1, jfloat y1, jfloat x2, jfloat y2, jfloat x3, jfloat y3)
{
	((SkPath *)_PTR(builder_ptr))->cubicTo(x1, y1, x2, y2, x3, y3);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1quad_1to(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat x1, jfloat y1, jfloat x2, jfloat y2)
{
	((SkPath *)_PTR(builder_ptr))->quadTo(x1, y1, x2, y2);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1rel_1move_1to(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat x, jfloat y)
{
	((SkPath *)_PTR(builder_ptr))->rMoveTo(x, y);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1rel_1line_1to(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat x, jfloat y)
{
	((SkPath *)_PTR(builder_ptr))->rLineTo(x, y);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1rel_1cubic_1to(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat x1, jfloat y1, jfloat x2, jfloat y2, jfloat x3, jfloat y3)
{
	((SkPath *)_PTR(builder_ptr))->rCubicTo(x1, y1, x2, y2, x3, y3);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1rel_1quad_1to(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat x1, jfloat y1, jfloat x2, jfloat y2)
{
	((SkPath *)_PTR(builder_ptr))->rQuadTo(x1, y1, x2, y2);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1add_1arc(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom,
                                                                   jfloat start_angle_deg, jfloat sweep_angle_deg)
{
	((SkPath *)_PTR(builder_ptr))->addArc(SkRect::MakeLTRB(left, top, right, bottom), start_angle_deg, sweep_angle_deg);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1add_1path(JNIEnv *env, jclass clazz, jlong builder_ptr, jlong path_ptr, jlong matrix_ptr)
{
	((SkPath *)_PTR(builder_ptr))->addPath(*(SkPath *)_PTR(path_ptr), *(SkMatrix *)_PTR(matrix_ptr));
}

JNIEXPORT jlong JNICALL Java_android_graphics_Path_native_1transform(JNIEnv *env, jclass clazz, jlong path_ptr, jlong matrix_ptr)
{
	SkPath *path = (SkPath *)_PTR(path_ptr);
	SkPath *transformed = new SkPath();
	path->transform(*(SkMatrix *)_PTR(matrix_ptr), transformed);
	delete path;
	return _INTPTR(transformed);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1add_1rect(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom)
{
	((SkPath *)_PTR(builder_ptr))->addRect(SkRect::MakeLTRB(left, top, right, bottom));
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1add_1round_1rect(JNIEnv *env, jclass clazz, jlong builder_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom, jfloatArray radii_jobj)
{
	jfloat *radii = env->GetFloatArrayElements(radii_jobj, NULL);
	SkRRect rrect;
	rrect.setRectRadii(SkRect::MakeLTRB(left, top, right, bottom), (const SkVector *)radii);
	env->ReleaseFloatArrayElements(radii_jobj, radii, 0);
	((SkPath *)_PTR(builder_ptr))->addRRect(rrect);
}

JNIEXPORT void JNICALL Java_android_graphics_Path_native_1get_1bounds(JNIEnv *env, jclass clazz, jlong path_ptr, jobject bounds)
{
	SkRect rect = ((SkPath *)_PTR(path_ptr))->getBounds();
	jclass bounds_class = env->GetObjectClass(bounds);
	env->SetFloatField(bounds, env->GetFieldID(bounds_class, "left", "F"), rect.left());
	env->SetFloatField(bounds, env->GetFieldID(bounds_class, "top", "F"), rect.top());
	env->SetFloatField(bounds, env->GetFieldID(bounds_class, "right", "F"), rect.right());
	env->SetFloatField(bounds, env->GetFieldID(bounds_class, "bottom", "F"), rect.bottom());
}
