#include <math.h>

#include "../defines.h"
#include "ATLCanvas.h"
#include "AndroidPaint.h"

#include "include/core/SkBlendMode.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkRect.h"

extern "C" {
#include "../generated_headers/android_graphics_Paint.h"
}

JNIEXPORT jlong JNICALL Java_android_graphics_Paint_native_1create(JNIEnv *env, jclass clazz)
{
	AndroidPaint *paint = new AndroidPaint();
	paint->paint.setColor(SK_ColorBLACK);
	paint->paint.setAntiAlias(true);
	paint->font.setTypeface(atl_default_typeface());
	paint->pango_font = pango_font_description_new();
	return _INTPTR(paint);
}

JNIEXPORT jlong JNICALL Java_android_graphics_Paint_native_1clone(JNIEnv *env, jclass clazz, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	AndroidPaint *clone = new AndroidPaint(*paint);
	clone->pango_font = pango_font_description_copy(paint->pango_font);
	return _INTPTR(clone);
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1recycle(JNIEnv *env, jclass clazz, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	pango_font_description_free(paint->pango_font);
	delete paint;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1set_1color(JNIEnv *env, jclass clazz, jlong paint_ptr, jint color)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	paint->paint.setColor((SkColor)(uint32_t)color);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_native_1get_1color(JNIEnv *env, jclass clazz, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	return (jint)paint->paint.getColor();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1set_1alpha(JNIEnv *env, jclass clazz, jlong paint_ptr, jint alpha)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	paint->paint.setAlpha(alpha & 0xFF);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_native_1get_1alpha(JNIEnv *env, jclass clazz, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	return paint->paint.getAlpha();
}

/* android.graphics.Paint.Style ordinals map directly to SkPaint::Style */
JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1set_1style(JNIEnv *env, jclass clazz, jlong paint_ptr, jint style)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	paint->paint.setStyle((SkPaint::Style)style);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_native_1get_1style(JNIEnv *env, jclass clazz, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	return (jint)paint->paint.getStyle();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1set_1stroke_1width(JNIEnv *env, jclass clazz, jlong paint_ptr, jfloat width)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	/* width 0 means hairline in both Android and Skia */
	paint->paint.setStrokeWidth(width);
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_native_1get_1stroke_1width(JNIEnv *env, jclass clazz, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	return paint->paint.getStrokeWidth();
}

/* android.graphics.Paint.Cap ordinals map directly to SkPaint::Cap */
JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1set_1stroke_1cap(JNIEnv *env, jclass clazz, jlong paint_ptr, jint cap)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	paint->paint.setStrokeCap((SkPaint::Cap)cap);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_native_1get_1stroke_1cap(JNIEnv *env, jclass clazz, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	return (jint)paint->paint.getStrokeCap();
}

/* android.graphics.Paint.Join ordinals map directly to SkPaint::Join */
JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1set_1stroke_1join(JNIEnv *env, jclass clazz, jlong paint_ptr, jint join)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	paint->paint.setStrokeJoin((SkPaint::Join)join);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_native_1get_1stroke_1join(JNIEnv *env, jclass clazz, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	return (jint)paint->paint.getStrokeJoin();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1set_1text_1size(JNIEnv *env, jclass clazz, jlong paint_ptr, jfloat size)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	paint->font.setSize(size);
	pango_font_description_set_absolute_size(paint->pango_font, roundf(size * PANGO_SCALE));
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_native_1get_1text_1size(JNIEnv *env, jclass clazz, jlong paint_ptr)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	return paint->font.getSize();
}

/* PorterDuff.Mode ordinals (see android.graphics.PorterDuff) */
static SkBlendMode porter_duff_to_blend_mode(int mode)
{
	switch (mode) {
		case 0: return SkBlendMode::kClear;
		case 1: return SkBlendMode::kSrc;
		case 2: return SkBlendMode::kDst;
		case 3: return SkBlendMode::kSrcOver;
		case 4: return SkBlendMode::kDstOver;
		case 5: return SkBlendMode::kSrcIn;
		case 6: return SkBlendMode::kDstIn;
		case 7: return SkBlendMode::kSrcOut;
		case 8: return SkBlendMode::kDstOut;
		case 9: return SkBlendMode::kSrcATop;
		case 10: return SkBlendMode::kDstATop;
		case 11: return SkBlendMode::kXor;
		case 12: return SkBlendMode::kDarken;
		case 13: return SkBlendMode::kLighten;
		case 14: return SkBlendMode::kModulate; // PorterDuff MULTIPLY
		case 15: return SkBlendMode::kScreen;
		case 16: return SkBlendMode::kPlus; // PorterDuff ADD
		case 17: return SkBlendMode::kOverlay;
		default: return SkBlendMode::kSrcOver;
	}
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1set_1color_1filter(JNIEnv *env, jclass clazz, jlong paint_ptr, jint mode, jint color)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	if (mode == -1)
		paint->paint.setColorFilter(nullptr);
	else
		paint->paint.setColorFilter(SkColorFilters::Blend((SkColor)(uint32_t)color, porter_duff_to_blend_mode(mode)));
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1get_1text_1bounds(JNIEnv *env, jclass clazz, jlong paint_ptr, jstring text_ptr, jobject bounds)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	const char *str = env->GetStringUTFChars(text_ptr, NULL);
	SkRect rect;
	paint->font.measureText(str, strlen(str), SkTextEncoding::kUTF8, &rect, &paint->paint);
	env->ReleaseStringUTFChars(text_ptr, str);
	if (paint->text_align == 1)
		rect.offset(-rect.width() / 2.f, 0);
	else if (paint->text_align == 2)
		rect.offset(-rect.width(), 0);
	jclass bounds_class = env->GetObjectClass(bounds);
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "left", "I"), (jint)floorf(rect.left()));
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "top", "I"), (jint)floorf(rect.top()));
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "right", "I"), (jint)ceilf(rect.right()));
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "bottom", "I"), (jint)ceilf(rect.bottom()));
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_native_1set_1text_1align(JNIEnv *env, jclass clazz, jlong paint_ptr, jint align)
{
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	paint->text_align = align;
}
