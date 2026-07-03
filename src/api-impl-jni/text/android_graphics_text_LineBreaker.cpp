#include <vector>

#include "../defines.h"
#include "../graphics/AndroidPaint.h"

#include <minikin/AndroidLineBreakerHelper.h>
#include <minikin/LineBreaker.h>
#include <minikin/MeasuredText.h>

extern "C" {
#include "../generated_headers/android_graphics_text_LineBreaker.h"
}

/* android.graphics.text.LineBreaker natives, adapted from AOSP
 * libs/hwui/jni/text/LineBreaker.cpp over the vendored minikin. */

static inline minikin::android::StaticLayoutNative *to_native(jlong ptr)
{
	return reinterpret_cast<minikin::android::StaticLayoutNative *>(ptr);
}

static inline minikin::LineBreakResult *to_result(jlong ptr)
{
	return reinterpret_cast<minikin::LineBreakResult *>(ptr);
}

static std::vector<float> int_array_to_float_vector(JNIEnv *env, jintArray java_array)
{
	if (!java_array)
		return std::vector<float>();
	jsize length = env->GetArrayLength(java_array);
	std::vector<float> result(length);
	jint *ints = env->GetIntArrayElements(java_array, NULL);
	for (jsize i = 0; i < length; i++)
		result[i] = ints[i];
	env->ReleaseIntArrayElements(java_array, ints, JNI_ABORT);
	return result;
}

JNIEXPORT jlong JNICALL Java_android_graphics_text_LineBreaker_nInit(JNIEnv *env, jclass,
	jint break_strategy, jint hyphenation_frequency, jboolean is_justified, jintArray indents)
{
	return reinterpret_cast<jlong>(new minikin::android::StaticLayoutNative(
	    static_cast<minikin::BreakStrategy>(break_strategy),
	    static_cast<minikin::HyphenationFrequency>(hyphenation_frequency),
	    is_justified,
	    int_array_to_float_vector(env, indents)));
}

static void line_breaker_finalizer(void *ptr)
{
	delete reinterpret_cast<minikin::android::StaticLayoutNative *>(ptr);
}

JNIEXPORT jlong JNICALL Java_android_graphics_text_LineBreaker_nGetReleaseFunc(JNIEnv *, jclass)
{
	return reinterpret_cast<jlong>(line_breaker_finalizer);
}

JNIEXPORT jlong JNICALL Java_android_graphics_text_LineBreaker_nComputeLineBreaks(JNIEnv *env, jclass,
	jlong native_ptr, jcharArray java_text, jlong measured_text_ptr, jint length,
	jfloat first_width, jint first_width_line_count, jfloat rest_width,
	jfloatArray variable_tab_stops, jfloat default_tab_stop, jint indents_offset)
{
	jchar *text = env->GetCharArrayElements(java_text, NULL);
	minikin::U16StringPiece u16_text(text, length);
	minikin::MeasuredText *measured_text =
	    reinterpret_cast<minikin::MeasuredText *>(measured_text_ptr);

	jfloat *tab_stops = variable_tab_stops
	    ? env->GetFloatArrayElements(variable_tab_stops, NULL) : NULL;
	jsize tab_stop_size = variable_tab_stops ? env->GetArrayLength(variable_tab_stops) : 0;

	minikin::LineBreakResult *result = new minikin::LineBreakResult(
	    to_native(native_ptr)->computeBreaks(u16_text, *measured_text, first_width,
	                                         first_width_line_count, rest_width, indents_offset,
	                                         tab_stops, tab_stop_size, default_tab_stop));

	if (tab_stops)
		env->ReleaseFloatArrayElements(variable_tab_stops, tab_stops, JNI_ABORT);
	env->ReleaseCharArrayElements(java_text, text, JNI_ABORT);
	return reinterpret_cast<jlong>(result);
}

JNIEXPORT jint JNICALL Java_android_graphics_text_LineBreaker_nGetLineCount(JNIEnv *, jclass, jlong ptr)
{
	return to_result(ptr)->breakPoints.size();
}

JNIEXPORT jint JNICALL Java_android_graphics_text_LineBreaker_nGetLineBreakOffset(JNIEnv *, jclass, jlong ptr, jint i)
{
	return to_result(ptr)->breakPoints[i];
}

JNIEXPORT jfloat JNICALL Java_android_graphics_text_LineBreaker_nGetLineWidth(JNIEnv *, jclass, jlong ptr, jint i)
{
	return to_result(ptr)->widths[i];
}

JNIEXPORT jfloat JNICALL Java_android_graphics_text_LineBreaker_nGetLineAscent(JNIEnv *, jclass, jlong ptr, jint i)
{
	return to_result(ptr)->ascents[i];
}

JNIEXPORT jfloat JNICALL Java_android_graphics_text_LineBreaker_nGetLineDescent(JNIEnv *, jclass, jlong ptr, jint i)
{
	return to_result(ptr)->descents[i];
}

JNIEXPORT jint JNICALL Java_android_graphics_text_LineBreaker_nGetLineFlag(JNIEnv *, jclass, jlong ptr, jint i)
{
	return to_result(ptr)->flags[i];
}

static void line_break_result_finalizer(void *ptr)
{
	delete reinterpret_cast<minikin::LineBreakResult *>(ptr);
}

JNIEXPORT jlong JNICALL Java_android_graphics_text_LineBreaker_nGetReleaseResultFunc(JNIEnv *, jclass)
{
	return reinterpret_cast<jlong>(line_break_result_finalizer);
}
