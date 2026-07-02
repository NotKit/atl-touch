#include <algorithm>
#include <cmath>
#include <vector>

#include "../defines.h"
#include "../graphics/ATLCanvas.h"
#include "../graphics/AndroidPaint.h"
#include "../hwui/MinikinGlue.h"
#include "../jni_cpp.h"

#include "include/core/SkFontMetrics.h"

#include <minikin/LineBreaker.h>
#include <minikin/MeasuredText.h>

extern "C" {
#include "../generated_headers/android_text_Layout.h"
}

/*
 * android.text.Layout backend over minikin's line breaker (the same greedy
 * breaker Android's StaticLayout uses), replacing the previous Pango one.
 * All offsets are UTF-16 code units, matching the Java side directly.
 */

namespace {

class ConstantLineWidth : public minikin::LineWidth {
public:
	explicit ConstantLineWidth(float width) : width_(width) {}
	float getAt(size_t line_no) const override { return width_; }
	float getMin() const override { return width_; }

private:
	float width_;
};

struct ATLTextLayout {
	std::vector<uint16_t> text;
	AndroidPaint *paint; // owned by the Java Paint the Java Layout references
	float width = -1;    // wrap width; -1 = unlimited
	int ellipsize_mode = 0;
	float ellipsize_width = 0;
	bool ellipsized = false;

	struct Line {
		int start, end;
		float advance;
		bool ellipsis = false; // draw "…" after this (truncated) line
	};
	std::vector<Line> lines;
	float ascent = 0, descent = 0, line_height = 1;

	void relayout();
	float measure(int start, int end, float *advances) const;
	int line_for_offset(int offset) const;
};

float ATLTextLayout::measure(int start, int end, float *advances) const
{
	if (end <= start)
		return 0;
	return android::minikin_measure_text(paint, text.data() + start, end - start,
	                                     minikin::Bidi::DEFAULT_LTR, advances);
}

void ATLTextLayout::relayout()
{
	SkFontMetrics metrics;
	paint->font.getMetrics(&metrics);
	ascent = metrics.fAscent;
	descent = metrics.fDescent;
	line_height = descent - ascent + metrics.fLeading;
	if (line_height <= 0)
		line_height = 1;

	lines.clear();
	ellipsized = false;

	int len = text.size();
	int para_start = 0;
	while (para_start <= len) {
		int para_end = para_start;
		while (para_end < len && text[para_end] != '\n')
			para_end++;

		int para_len = para_end - para_start;
		if (para_len == 0) {
			lines.push_back({para_start, para_start, 0, false});
		} else if (width < 0) {
			lines.push_back({para_start, para_end, measure(para_start, para_end, nullptr), false});
		} else {
			minikin::U16StringPiece para(text.data() + para_start, para_len);
			minikin::MeasuredTextBuilder builder;
			builder.addStyleRun(0, para_len, android::minikin_paint_for(paint),
			                    0 /* LINE_BREAK_STYLE_NONE */, 0 /* WORD_STYLE_NONE */,
			                    false /* isRtl */);
			std::unique_ptr<minikin::MeasuredText> measured = builder.build(
			    para, false /* hyphenation */, false /* computeLayout */,
			    true /* ignoreHyphenKerning */, nullptr);
			ConstantLineWidth line_width(width);
			minikin::TabStops tab_stops(nullptr, 0, paint->font.getSize() * 2);
			minikin::LineBreakResult result = minikin::breakIntoLines(
			    para, minikin::BreakStrategy::Greedy, minikin::HyphenationFrequency::None,
			    false /* justified */, *measured, line_width, tab_stops);
			int line_start = 0;
			for (size_t i = 0; i < result.breakPoints.size(); i++) {
				lines.push_back({para_start + line_start, para_start + result.breakPoints[i],
				                 result.widths[i], false});
				line_start = result.breakPoints[i];
			}
		}
		para_start = para_end + 1;
	}

	/* single-line END ellipsis, the common TextView case */
	if (ellipsize_mode != 0 && (lines.size() > 1 || (lines.size() == 1 && width > 0 && lines[0].advance > width))) {
		float max_width = ellipsize_width > 0 ? ellipsize_width : width;
		if (max_width > 0) {
			const uint16_t ellipsis_char = 0x2026; // …
			float ellipsis_advance = android::minikin_measure_text(
			    paint, &ellipsis_char, 1, minikin::Bidi::DEFAULT_LTR, nullptr);
			Line line = {0, 0, 0, true};
			std::vector<float> advances(text.size());
			measure(0, text.size(), advances.data());
			float budget = max_width - ellipsis_advance;
			int end = 0;
			float advance = 0;
			while (end < (int)text.size() && text[end] != '\n' &&
			       (advance + advances[end] <= budget || advances[end] == 0)) {
				advance += advances[end];
				end++;
			}
			line.end = end;
			line.advance = advance + ellipsis_advance;
			lines.assign(1, line);
			ellipsized = true;
		}
	}
}

int ATLTextLayout::line_for_offset(int offset) const
{
	for (size_t i = 0; i < lines.size(); i++)
		if (offset <= lines[i].end)
			return i;
	return lines.size() - 1;
}

#define LAYOUT(ptr) ((ATLTextLayout *)_PTR(ptr))

} // namespace

JNIEXPORT jlong JNICALL Java_android_text_Layout_native_1constructor(JNIEnv *env, jclass, jstring text, jlong paint_ptr, jint width)
{
	ATLTextLayout *layout = new ATLTextLayout();
	const jchar *chars = env->GetStringChars(text, NULL);
	layout->text.assign(chars, chars + env->GetStringLength(text));
	env->ReleaseStringChars(text, chars);
	layout->paint = (AndroidPaint *)_PTR(paint_ptr);
	layout->width = width;
	layout->relayout();
	return _INTPTR(layout);
}

JNIEXPORT void JNICALL Java_android_text_Layout_native_1free(JNIEnv *env, jclass, jlong layout)
{
	delete LAYOUT(layout);
}

JNIEXPORT void JNICALL Java_android_text_Layout_native_1set_1width(JNIEnv *env, jobject, jlong layout, jint width)
{
	LAYOUT(layout)->width = width;
	LAYOUT(layout)->relayout();
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1width(JNIEnv *env, jobject, jlong layout)
{
	return (jint)LAYOUT(layout)->width;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1height(JNIEnv *env, jobject, jlong layout)
{
	return (jint)ceilf(LAYOUT(layout)->lines.size() * LAYOUT(layout)->line_height);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1count(JNIEnv *env, jobject, jlong layout)
{
	return LAYOUT(layout)->lines.size();
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1start(JNIEnv *env, jobject, jlong layout, jint line)
{
	return LAYOUT(layout)->lines[line].start;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1end(JNIEnv *env, jobject, jlong layout, jint line)
{
	return LAYOUT(layout)->lines[line].end;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1top(JNIEnv *env, jobject, jlong layout, jint line)
{
	return (jint)(line * LAYOUT(layout)->line_height);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1bottom(JNIEnv *env, jobject, jlong layout, jint line)
{
	return (jint)ceilf((line + 1) * LAYOUT(layout)->line_height);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1left(JNIEnv *env, jobject, jlong layout, jint line)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1right(JNIEnv *env, jobject, jlong layout, jint line)
{
	return (jint)ceilf(LAYOUT(layout)->lines[line].advance);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1width(JNIEnv *env, jobject, jlong layout, jint line)
{
	return (jint)ceilf(LAYOUT(layout)->lines[line].advance);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1baseline(JNIEnv *env, jobject, jlong layout, jint line)
{
	ATLTextLayout *l = LAYOUT(layout);
	return (jint)roundf(line * l->line_height - l->ascent);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1ascent(JNIEnv *env, jobject, jlong layout, jint line)
{
	return (jint)LAYOUT(layout)->ascent;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1descent(JNIEnv *env, jobject, jlong layout, jint line)
{
	return (jint)ceilf(LAYOUT(layout)->descent);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1for_1vertical(JNIEnv *env, jobject, jlong layout, jint y)
{
	ATLTextLayout *l = LAYOUT(layout);
	int line = (int)(y / l->line_height);
	return std::clamp(line, 0, (int)l->lines.size() - 1);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1for_1offset(JNIEnv *env, jobject, jlong layout, jint offset)
{
	return LAYOUT(layout)->line_for_offset(offset);
}

JNIEXPORT void JNICALL Java_android_text_Layout_native_1set_1ellipsize(JNIEnv *env, jobject, jlong layout, jint ellipsize_mode, jfloat ellipsize_width)
{
	ATLTextLayout *l = LAYOUT(layout);
	l->ellipsize_mode = ellipsize_mode;
	l->ellipsize_width = ellipsize_width;
	l->relayout();
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1ellipsis_1count(JNIEnv *env, jobject, jlong layout, jint line)
{
	return LAYOUT(layout)->ellipsized;
}

JNIEXPORT jfloat JNICALL Java_android_text_Layout_native_1get_1primary_1horizontal(JNIEnv *env, jobject, jlong layout, jint offset)
{
	ATLTextLayout *l = LAYOUT(layout);
	if (l->lines.empty())
		return 0;
	const auto &line = l->lines[l->line_for_offset(offset)];
	return l->measure(line.start, std::min(offset, line.end), nullptr);
}

JNIEXPORT jfloat JNICALL Java_android_text_Layout_native_1get_1secondary_1horizontal(JNIEnv *env, jobject object, jlong layout, jint offset)
{
	return Java_android_text_Layout_native_1get_1primary_1horizontal(env, object, layout, offset);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1offset_1for_1horizontal(JNIEnv *env, jobject, jlong layout, jint line_no, jfloat x)
{
	ATLTextLayout *l = LAYOUT(layout);
	if (l->lines.empty())
		return 0;
	const auto &line = l->lines[line_no];
	int count = line.end - line.start;
	if (count <= 0)
		return line.start;
	std::vector<float> advances(count);
	l->measure(line.start, line.end, advances.data());
	float pos = 0;
	for (int i = 0; i < count; i++) {
		if (pos + advances[i] / 2 > x)
			return line.start + i;
		pos += advances[i];
	}
	return line.end;
}

JNIEXPORT jfloat JNICALL Java_android_text_Layout_native_1get_1desired_1width(JNIEnv *env, jclass, jlong layout)
{
	float max_advance = 0;
	for (const auto &line : LAYOUT(layout)->lines)
		max_advance = std::max(max_advance, line.advance);
	return max_advance;
}

JNIEXPORT void JNICALL Java_android_text_Layout_native_1draw(JNIEnv *env, jobject, jlong layout_ptr, jlong canvas_ptr, jlong paint_ptr)
{
	ATLTextLayout *layout = LAYOUT(layout_ptr);
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(canvas_ptr);
	AndroidPaint *paint = (AndroidPaint *)_PTR(paint_ptr);
	const uint16_t ellipsis_char = 0x2026;

	for (size_t i = 0; i < layout->lines.size(); i++) {
		const auto &line = layout->lines[i];
		float baseline = i * layout->line_height - layout->ascent;
		if (line.end > line.start)
			android::minikin_draw_text(atl_canvas->canvas, paint,
			                           layout->text.data() + line.start, line.end - line.start,
			                           minikin::Bidi::DEFAULT_LTR, 0, baseline);
		if (line.ellipsis) {
			float x = layout->measure(line.start, line.end, nullptr);
			android::minikin_draw_text(atl_canvas->canvas, paint, &ellipsis_char, 1,
			                           minikin::Bidi::DEFAULT_LTR, x, baseline);
		}
	}
}

JNIEXPORT void JNICALL Java_android_text_Layout_native_1draw_1custom_1canvas(JNIEnv *env, jobject, jlong layout_ptr, jobject canvas, jobject paint)
{
	ATLTextLayout *layout = LAYOUT(layout_ptr);
	for (size_t i = 0; i < layout->lines.size(); i++) {
		const auto &line = layout->lines[i];
		if (line.end <= line.start)
			continue;
		float baseline = i * layout->line_height - layout->ascent;
		jstring text_jstr = env->NewString(layout->text.data() + line.start, line.end - line.start);
		env->CallVoidMethod(canvas, handle_cache.canvas.drawText, text_jstr, (jint)0,
		                    (jint)(line.end - line.start), (jfloat)0, (jfloat)baseline, paint);
		env->DeleteLocalRef(text_jstr);
	}
}
