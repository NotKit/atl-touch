#include <cmath>

#include "../defines.h"
#include "../graphics/AndroidPaint.h"
#include "../hwui/MinikinGlue.h"

#include <minikin/MeasuredText.h>

extern "C" {
#include "../generated_headers/android_graphics_text_MeasuredText.h"
#include "../generated_headers/android_graphics_text_MeasuredText_Builder.h"
}

/* android.graphics.text.MeasuredText natives, adapted from AOSP
 * libs/hwui/jni/text/MeasuredText.cpp over the vendored minikin. */

static inline minikin::MeasuredTextBuilder *to_builder(jlong ptr)
{
	return reinterpret_cast<minikin::MeasuredTextBuilder *>(ptr);
}

static inline minikin::MeasuredText *to_measured_text(jlong ptr)
{
	return reinterpret_cast<minikin::MeasuredText *>(ptr);
}

JNIEXPORT jlong JNICALL Java_android_graphics_text_MeasuredText_00024Builder_nInitBuilder(JNIEnv *, jclass)
{
	return reinterpret_cast<jlong>(new minikin::MeasuredTextBuilder());
}

JNIEXPORT void JNICALL Java_android_graphics_text_MeasuredText_00024Builder_nAddStyleRun(JNIEnv *, jclass,
	jlong builder_ptr, jlong paint_ptr, jint lb_style, jint lb_word_style, jint start, jint end,
	jboolean is_rtl)
{
	AndroidPaint *paint = reinterpret_cast<AndroidPaint *>(_PTR(paint_ptr));
	to_builder(builder_ptr)->addStyleRun(start, end, android::minikin_paint_for(paint),
	                                     lb_style, lb_word_style, is_rtl);
}

JNIEXPORT void JNICALL Java_android_graphics_text_MeasuredText_00024Builder_nAddReplacementRun(JNIEnv *, jclass,
	jlong builder_ptr, jlong paint_ptr, jint start, jint end, jfloat width)
{
	AndroidPaint *paint = reinterpret_cast<AndroidPaint *>(_PTR(paint_ptr));
	to_builder(builder_ptr)->addReplacementRun(start, end, width, paint->locale_list_id);
}

JNIEXPORT jlong JNICALL Java_android_graphics_text_MeasuredText_00024Builder_nBuildMeasuredText(JNIEnv *env, jclass,
	jlong builder_ptr, jlong hint_ptr, jcharArray java_text, jboolean compute_hyphenation,
	jboolean compute_layout, jboolean fast_hyphenation_mode)
{
	jchar *text = env->GetCharArrayElements(java_text, NULL);
	minikin::U16StringPiece text_buffer(text, env->GetArrayLength(java_text));

	// Pass the ownership to Java.
	jlong result = reinterpret_cast<jlong>(
	    to_builder(builder_ptr)->build(text_buffer, compute_hyphenation, compute_layout,
	                                   fast_hyphenation_mode, to_measured_text(hint_ptr))
	        .release());
	env->ReleaseCharArrayElements(java_text, text, JNI_ABORT);
	return result;
}

JNIEXPORT void JNICALL Java_android_graphics_text_MeasuredText_00024Builder_nFreeBuilder(JNIEnv *, jclass,
	jlong builder_ptr)
{
	delete to_builder(builder_ptr);
}

JNIEXPORT jfloat JNICALL Java_android_graphics_text_MeasuredText_nGetWidth(JNIEnv *, jclass, jlong ptr,
	jint start, jint end)
{
	minikin::MeasuredText *mt = to_measured_text(ptr);
	float r = 0.0f;
	for (int i = start; i < end; i++)
		r += mt->widths[i];
	return r;
}

static void measured_text_finalizer(void *ptr)
{
	delete reinterpret_cast<minikin::MeasuredText *>(ptr);
}

JNIEXPORT jlong JNICALL Java_android_graphics_text_MeasuredText_nGetReleaseFunc(JNIEnv *, jclass)
{
	return reinterpret_cast<jlong>(measured_text_finalizer);
}

JNIEXPORT jint JNICALL Java_android_graphics_text_MeasuredText_nGetMemoryUsage(JNIEnv *, jclass, jlong ptr)
{
	return static_cast<jint>(to_measured_text(ptr)->getMemoryUsage());
}

JNIEXPORT void JNICALL Java_android_graphics_text_MeasuredText_nGetBounds(JNIEnv *env, jclass, jlong ptr,
	jcharArray java_text, jint start, jint end, jobject bounds)
{
	jchar *text = env->GetCharArrayElements(java_text, NULL);
	minikin::U16StringPiece text_buffer(text, env->GetArrayLength(java_text));
	minikin::Range range(start, end);

	minikin::MinikinRect rect = to_measured_text(ptr)->getBounds(text_buffer, range);
	env->ReleaseCharArrayElements(java_text, text, JNI_ABORT);

	jclass bounds_class = env->GetObjectClass(bounds);
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "left", "I"), (jint)floorf(rect.mLeft));
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "top", "I"), (jint)floorf(rect.mTop));
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "right", "I"), (jint)ceilf(rect.mRight));
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "bottom", "I"), (jint)ceilf(rect.mBottom));
}

JNIEXPORT jfloat JNICALL Java_android_graphics_text_MeasuredText_nGetCharWidthAt(JNIEnv *, jclass, jlong ptr,
	jint offset)
{
	return to_measured_text(ptr)->widths[offset];
}

JNIEXPORT jlong JNICALL Java_android_graphics_text_MeasuredText_nGetExtent(JNIEnv *env, jclass, jlong ptr,
	jcharArray java_text, jint start, jint end)
{
	jchar *text = env->GetCharArrayElements(java_text, NULL);
	minikin::U16StringPiece text_buffer(text, env->GetArrayLength(java_text));
	minikin::Range range(start, end);

	minikin::MinikinExtent extent = to_measured_text(ptr)->getExtent(text_buffer, range);
	env->ReleaseCharArrayElements(java_text, text, JNI_ABORT);

	int32_t ascent = (int32_t)roundf(extent.ascent);
	int32_t descent = (int32_t)roundf(extent.descent);

	return (((jlong)ascent) << 32) | (jlong)(uint32_t)descent;
}
