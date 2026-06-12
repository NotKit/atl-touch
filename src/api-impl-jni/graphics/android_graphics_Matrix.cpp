#include <stdint.h>

#include "../defines.h"

#include "include/core/SkMatrix.h"
#include "include/core/SkRect.h"

extern "C" {
#include "../generated_headers/android_graphics_Matrix.h"
}

static SkRect rect_from_java(JNIEnv *env, jobject rect)
{
	jclass rect_class = env->GetObjectClass(rect);
	return SkRect::MakeLTRB(
	    env->GetFloatField(rect, env->GetFieldID(rect_class, "left", "F")),
	    env->GetFloatField(rect, env->GetFieldID(rect_class, "top", "F")),
	    env->GetFloatField(rect, env->GetFieldID(rect_class, "right", "F")),
	    env->GetFloatField(rect, env->GetFieldID(rect_class, "bottom", "F")));
}

static void rect_to_java(JNIEnv *env, jobject rect, const SkRect &skrect)
{
	jclass rect_class = env->GetObjectClass(rect);
	env->SetFloatField(rect, env->GetFieldID(rect_class, "left", "F"), skrect.left());
	env->SetFloatField(rect, env->GetFieldID(rect_class, "top", "F"), skrect.top());
	env->SetFloatField(rect, env->GetFieldID(rect_class, "right", "F"), skrect.right());
	env->SetFloatField(rect, env->GetFieldID(rect_class, "bottom", "F"), skrect.bottom());
}

JNIEXPORT jlong JNICALL Java_android_graphics_Matrix_native_1create(JNIEnv *env, jclass clazz, jlong src)
{
	SkMatrix *matrix = new SkMatrix();
	if (src)
		*matrix = *(SkMatrix *)_PTR(src);
	return _INTPTR(matrix);
}

/* android.graphics.Matrix value indices (MSCALE_X..MPERSP_2) match SkMatrix's flat 9-value layout */
JNIEXPORT void JNICALL Java_android_graphics_Matrix_native_1getValues(JNIEnv *env, jclass clazz, jlong src, jfloatArray values_ref)
{
	SkMatrix *matrix = (SkMatrix *)_PTR(src);
	jfloat *values = env->GetFloatArrayElements(values_ref, NULL);
	matrix->get9(values);
	// add 0.f to all values to avoid failing CTS tests with "expected:<0.0> but was:<-0.0>"
	for (int i = 0; i < 9; i++)
		values[i] += 0.f;
	env->ReleaseFloatArrayElements(values_ref, values, 0);
}

JNIEXPORT void JNICALL Java_android_graphics_Matrix_native_1setValues(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloatArray values_ref)
{
	SkMatrix *matrix = (SkMatrix *)_PTR(matrix_ptr);
	jfloat *values = env->GetFloatArrayElements(values_ref, NULL);
	matrix->set9(values);
	env->ReleaseFloatArrayElements(values_ref, values, 0);
}

JNIEXPORT void JNICALL Java_android_graphics_Matrix_native_1set(JNIEnv *env, jclass clazz, jlong dest_ptr, jlong src_ptr)
{
	*(SkMatrix *)_PTR(dest_ptr) = *(SkMatrix *)_PTR(src_ptr);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1isIdentity(JNIEnv *env, jclass clazz, jlong matrix_ptr)
{
	return ((SkMatrix *)_PTR(matrix_ptr))->isIdentity();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1preConcat(JNIEnv *env, jclass clazz, jlong matrix_ptr, jlong other_ptr)
{
	((SkMatrix *)_PTR(matrix_ptr))->preConcat(*(SkMatrix *)_PTR(other_ptr));
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1postConcat(JNIEnv *env, jclass clazz, jlong matrix_ptr, jlong other_ptr)
{
	((SkMatrix *)_PTR(matrix_ptr))->postConcat(*(SkMatrix *)_PTR(other_ptr));
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1mapRect(JNIEnv *env, jclass clazz, jlong matrix_ptr, jobject dest, jobject src)
{
	SkMatrix *matrix = (SkMatrix *)_PTR(matrix_ptr);
	SkRect dest_rect;
	bool rect_stays_rect = matrix->mapRect(&dest_rect, rect_from_java(env, src));
	rect_to_java(env, dest, dest_rect);
	return rect_stays_rect;
}

JNIEXPORT void JNICALL Java_android_graphics_Matrix_native_1reset(JNIEnv *env, jclass clazz, jlong matrix_ptr)
{
	((SkMatrix *)_PTR(matrix_ptr))->reset();
}

JNIEXPORT void JNICALL Java_android_graphics_Matrix_finalizer(JNIEnv *env, jclass clazz, jlong matrix_ptr)
{
	delete (SkMatrix *)_PTR(matrix_ptr);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1postTranslate(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat x, jfloat y)
{
	((SkMatrix *)_PTR(matrix_ptr))->postTranslate(x, y);
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1postScale__JFF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat x, jfloat y)
{
	((SkMatrix *)_PTR(matrix_ptr))->postScale(x, y);
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1postScale__JFFFF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat x, jfloat y, jfloat px, jfloat py)
{
	((SkMatrix *)_PTR(matrix_ptr))->postScale(x, y, px, py);
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1postRotate__JFFF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat degrees, jfloat px, jfloat py)
{
	((SkMatrix *)_PTR(matrix_ptr))->postRotate(degrees, px, py);
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1postRotate__JF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat degrees)
{
	((SkMatrix *)_PTR(matrix_ptr))->postRotate(degrees);
	return true;
}

JNIEXPORT void Java_android_graphics_Matrix_native_1setScale__JFF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat x, jfloat y)
{
	((SkMatrix *)_PTR(matrix_ptr))->setScale(x, y);
}

JNIEXPORT void JNICALL Java_android_graphics_Matrix_native_1setScale__JFFFF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat x, jfloat y, jfloat px, jfloat py)
{
	((SkMatrix *)_PTR(matrix_ptr))->setScale(x, y, px, py);
}

/* android.graphics.Matrix.ScaleToFit ordinals map directly to SkMatrix::ScaleToFit */
JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1setRectToRect(JNIEnv *env, jclass clazz, jlong matrix_ptr, jobject src, jobject dest, jint stf)
{
	SkMatrix *matrix = (SkMatrix *)_PTR(matrix_ptr);
	return matrix->setRectToRect(rect_from_java(env, src), rect_from_java(env, dest), (SkMatrix::ScaleToFit)stf);
}

JNIEXPORT void JNICALL Java_android_graphics_Matrix_native_1mapPoints(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloatArray dst_ref, jint dst_idx, jfloatArray src_ref, jint src_idx, jint count, jboolean is_pts)
{
	SkMatrix *matrix = (SkMatrix *)_PTR(matrix_ptr);
	jfloat *src = env->GetFloatArrayElements(src_ref, NULL);
	jfloat *dst = env->GetFloatArrayElements(dst_ref, NULL);
	if (is_pts)
		matrix->mapPoints((SkPoint *)(dst + dst_idx), (const SkPoint *)(src + src_idx), count);
	else
		matrix->mapVectors((SkVector *)(dst + dst_idx), (const SkVector *)(src + src_idx), count);
	env->ReleaseFloatArrayElements(src_ref, src, 0);
	env->ReleaseFloatArrayElements(dst_ref, dst, 0);
}

JNIEXPORT void JNICALL Java_android_graphics_Matrix_native_1setTranslate(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat x, jfloat y)
{
	((SkMatrix *)_PTR(matrix_ptr))->setTranslate(x, y);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1preRotate__JF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat degrees)
{
	((SkMatrix *)_PTR(matrix_ptr))->preRotate(degrees);
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1preRotate__JFFF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat degrees, jfloat px, jfloat py)
{
	((SkMatrix *)_PTR(matrix_ptr))->preRotate(degrees, px, py);
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1invert(JNIEnv *env, jclass clazz, jlong matrix_ptr, jlong inverse_ptr)
{
	return ((SkMatrix *)_PTR(matrix_ptr))->invert((SkMatrix *)_PTR(inverse_ptr));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1preScale__JFF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat x, jfloat y)
{
	((SkMatrix *)_PTR(matrix_ptr))->preScale(x, y);
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1preScale__JFFFF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat x, jfloat y, jfloat px, jfloat py)
{
	((SkMatrix *)_PTR(matrix_ptr))->preScale(x, y, px, py);
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1preTranslate(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat x, jfloat y)
{
	((SkMatrix *)_PTR(matrix_ptr))->preTranslate(x, y);
	return true;
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1equals(JNIEnv *env, jclass clazz, jlong matrix1_ptr, jlong matrix2_ptr)
{
	return *(SkMatrix *)_PTR(matrix1_ptr) == *(SkMatrix *)_PTR(matrix2_ptr);
}

JNIEXPORT void JNICALL Java_android_graphics_Matrix_native_1setRotate__JFFF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat degrees, jfloat px, jfloat py)
{
	((SkMatrix *)_PTR(matrix_ptr))->setRotate(degrees, px, py);
}

JNIEXPORT void JNICALL Java_android_graphics_Matrix_native_1setRotate__JF(JNIEnv *env, jclass clazz, jlong matrix_ptr, jfloat degrees)
{
	((SkMatrix *)_PTR(matrix_ptr))->setRotate(degrees);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Matrix_native_1rectStaysRect(JNIEnv *env, jclass clazz, jlong matrix_ptr)
{
	return ((SkMatrix *)_PTR(matrix_ptr))->rectStaysRect();
}
