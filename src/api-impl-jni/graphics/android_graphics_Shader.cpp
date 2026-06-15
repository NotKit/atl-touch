#include <vector>

#include "../defines.h"
#include "ATLShader.h"

#include "include/core/SkColor.h"
#include "include/core/SkPoint.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradientShader.h"

#include <jni.h>

/* JNI entry points are declared with C linkage and resolved by name at runtime;
 * we don't include the javac-generated headers here so this file builds cleanly
 * on a fresh tree (the headers don't exist until the Java side is compiled). */

/* matches android.graphics.Shader.TileMode ordinals */
static SkTileMode tile_mode(jint mode)
{
	switch (mode) {
	case 1: return SkTileMode::kMirror;
	case 2: return SkTileMode::kRepeat;
	case 0:
	default: return SkTileMode::kClamp;
	}
}

static void read_stops(JNIEnv *env, jintArray colors_arr, jfloatArray pos_arr,
                       std::vector<SkColor> &colors, std::vector<SkScalar> &pos)
{
	jsize count = env->GetArrayLength(colors_arr);
	jint *c = env->GetIntArrayElements(colors_arr, nullptr);
	colors.resize(count);
	for (jsize i = 0; i < count; i++)
		colors[i] = (SkColor)(uint32_t)c[i];
	env->ReleaseIntArrayElements(colors_arr, c, JNI_ABORT);

	if (pos_arr != nullptr) {
		jfloat *p = env->GetFloatArrayElements(pos_arr, nullptr);
		pos.assign(p, p + count);
		env->ReleaseFloatArrayElements(pos_arr, p, JNI_ABORT);
	}
}

extern "C" {

JNIEXPORT void JNICALL Java_android_graphics_Shader_native_1setLocalMatrix(JNIEnv *env, jclass clazz, jlong shader_ptr, jlong matrix_ptr)
{
	ATLShader *shader = (ATLShader *)_PTR(shader_ptr);
	if (matrix_ptr)
		shader->localMatrix = *(SkMatrix *)_PTR(matrix_ptr);
	else
		shader->localMatrix = SkMatrix::I();
}

JNIEXPORT void JNICALL Java_android_graphics_Shader_native_1destroy(JNIEnv *env, jclass clazz, jlong shader_ptr)
{
	delete (ATLShader *)_PTR(shader_ptr);
}

JNIEXPORT jlong JNICALL Java_android_graphics_LinearGradient_native_1create(JNIEnv *env, jclass clazz, jfloat x0, jfloat y0, jfloat x1, jfloat y1, jintArray colors_arr, jfloatArray pos_arr, jint tile)
{
	std::vector<SkColor> colors;
	std::vector<SkScalar> pos;
	read_stops(env, colors_arr, pos_arr, colors, pos);

	SkPoint pts[2] = {{x0, y0}, {x1, y1}};
	ATLShader *shader = new ATLShader();
	shader->base = SkGradientShader::MakeLinear(pts, colors.data(),
	                                            pos.empty() ? nullptr : pos.data(),
	                                            (int)colors.size(), tile_mode(tile));
	return _INTPTR(shader);
}

JNIEXPORT jlong JNICALL Java_android_graphics_RadialGradient_native_1create(JNIEnv *env, jclass clazz, jfloat cx, jfloat cy, jfloat radius, jintArray colors_arr, jfloatArray pos_arr, jint tile)
{
	std::vector<SkColor> colors;
	std::vector<SkScalar> pos;
	read_stops(env, colors_arr, pos_arr, colors, pos);

	ATLShader *shader = new ATLShader();
	shader->base = SkGradientShader::MakeRadial(SkPoint::Make(cx, cy), radius, colors.data(),
	                                            pos.empty() ? nullptr : pos.data(),
	                                            (int)colors.size(), tile_mode(tile));
	return _INTPTR(shader);
}

} // extern "C"
