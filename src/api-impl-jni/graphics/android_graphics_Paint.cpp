#include <math.h>

#include "../defines.h"
#include "ATLCanvas.h"
#include "ATLShader.h"
#include "AndroidPaint.h"
#include "../hwui/MinikinGlue.h"

#include <vector>

#include "include/core/SkBlendMode.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPathEffect.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathUtils.h"
#include "include/core/SkRect.h"

#include <minikin/GraphemeBreak.h>
#include <minikin/LocaleList.h>
#include <minikin/Measurement.h>

extern "C" {
#include "../generated_headers/android_graphics_Paint.h"
#include "../generated_headers/android_graphics_PorterDuffColorFilter.h"
}

/*
 * android.graphics.Paint natives (AOSP-13 surface) over AndroidPaint
 * {SkPaint, SkFont, minikin state}; text measurement/queries go through
 * minikin for shaping and font fallback.
 */

#define PAINT(ptr) ((AndroidPaint *)_PTR(ptr))

/* android.graphics.Paint flag bits */
static const int ANTI_ALIAS_FLAG = 1 << 0;
static const int FILTER_BITMAP_FLAG = 1 << 1;
static const int DITHER_FLAG = 1 << 2;
static const int UNDERLINE_TEXT_FLAG = 1 << 3;
static const int STRIKE_THRU_TEXT_FLAG = 1 << 4;
static const int FAKE_BOLD_TEXT_FLAG = 1 << 5;
static const int LINEAR_TEXT_FLAG = 1 << 6;
static const int SUBPIXEL_TEXT_FLAG = 1 << 7;

/* PorterDuff.Mode nativeInt values (see android.graphics.PorterDuff) */
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

/* --- lifecycle --- */

JNIEXPORT jlong JNICALL Java_android_graphics_Paint_nInit(JNIEnv *env, jclass)
{
	AndroidPaint *paint = new AndroidPaint();
	paint->paint.setColor(SK_ColorBLACK);
	paint->font.setTypeface(atl_default_typeface());
	paint->pango_font = pango_font_description_new();
	return _INTPTR(paint);
}

JNIEXPORT jlong JNICALL Java_android_graphics_Paint_nInitWithPaint(JNIEnv *env, jclass, jlong src_ptr)
{
	AndroidPaint *clone = new AndroidPaint(*PAINT(src_ptr));
	clone->pango_font = pango_font_description_copy(PAINT(src_ptr)->pango_font);
	return _INTPTR(clone);
}

static void paint_finalizer(void *ptr)
{
	AndroidPaint *paint = (AndroidPaint *)ptr;
	pango_font_description_free(paint->pango_font);
	delete paint;
}

JNIEXPORT jlong JNICALL Java_android_graphics_Paint_nGetNativeFinalizer(JNIEnv *env, jclass)
{
	return _INTPTR(&paint_finalizer);
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nReset(JNIEnv *env, jclass, jlong paint_ptr)
{
	AndroidPaint *paint = PAINT(paint_ptr);
	PangoFontDescription *pango_font = paint->pango_font;
	*paint = AndroidPaint();
	paint->paint.setColor(SK_ColorBLACK);
	paint->font.setTypeface(atl_default_typeface());
	pango_font_description_free(pango_font);
	paint->pango_font = pango_font_description_new();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSet(JNIEnv *env, jclass, jlong dst_ptr, jlong src_ptr)
{
	AndroidPaint *dst = PAINT(dst_ptr);
	PangoFontDescription *pango_font = dst->pango_font;
	*dst = *PAINT(src_ptr);
	pango_font_description_free(pango_font);
	dst->pango_font = pango_font_description_copy(PAINT(src_ptr)->pango_font);
}

/* --- flags --- */

static void apply_flags(AndroidPaint *paint)
{
	paint->paint.setAntiAlias(paint->flags & ANTI_ALIAS_FLAG);
	paint->paint.setDither(paint->flags & DITHER_FLAG);
	paint->underline = paint->flags & UNDERLINE_TEXT_FLAG;
	paint->strike_thru = paint->flags & STRIKE_THRU_TEXT_FLAG;
	paint->font.setEmbolden(paint->flags & FAKE_BOLD_TEXT_FLAG);
	paint->font.setLinearMetrics(paint->flags & LINEAR_TEXT_FLAG);
	paint->font.setSubpixel(paint->flags & SUBPIXEL_TEXT_FLAG);
}

static void set_flag(AndroidPaint *paint, int flag, bool enable)
{
	paint->flags = enable ? (paint->flags | flag) : (paint->flags & ~flag);
	apply_flags(paint);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetFlags(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->flags;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetFlags(JNIEnv *env, jclass, jlong paint_ptr, jint flags)
{
	PAINT(paint_ptr)->flags = flags;
	apply_flags(PAINT(paint_ptr));
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetAntiAlias(JNIEnv *env, jclass, jlong p, jboolean v) { set_flag(PAINT(p), ANTI_ALIAS_FLAG, v); }
JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetDither(JNIEnv *env, jclass, jlong p, jboolean v) { set_flag(PAINT(p), DITHER_FLAG, v); }
JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetLinearText(JNIEnv *env, jclass, jlong p, jboolean v) { set_flag(PAINT(p), LINEAR_TEXT_FLAG, v); }
JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetSubpixelText(JNIEnv *env, jclass, jlong p, jboolean v) { set_flag(PAINT(p), SUBPIXEL_TEXT_FLAG, v); }
JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetUnderlineText(JNIEnv *env, jclass, jlong p, jboolean v) { set_flag(PAINT(p), UNDERLINE_TEXT_FLAG, v); }
JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetStrikeThruText(JNIEnv *env, jclass, jlong p, jboolean v) { set_flag(PAINT(p), STRIKE_THRU_TEXT_FLAG, v); }
JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetFakeBoldText(JNIEnv *env, jclass, jlong p, jboolean v) { set_flag(PAINT(p), FAKE_BOLD_TEXT_FLAG, v); }
JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetFilterBitmap(JNIEnv *env, jclass, jlong p, jboolean v) { set_flag(PAINT(p), FILTER_BITMAP_FLAG, v); }

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetHinting(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->font.getHinting() == SkFontHinting::kNone ? 0 : 1;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetHinting(JNIEnv *env, jclass, jlong paint_ptr, jint mode)
{
	PAINT(paint_ptr)->font.setHinting(mode == 0 ? SkFontHinting::kNone : SkFontHinting::kNormal);
}

/* --- color / stroke / style --- */

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetColor__JI(JNIEnv *env, jclass, jlong paint_ptr, jint color)
{
	PAINT(paint_ptr)->paint.setColor((SkColor)(uint32_t)color);
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetColor__JJJ(JNIEnv *env, jclass, jlong paint_ptr, jlong colorspace_ptr, jlong color_long)
{
	/* sRGB ColorLong subset: ARGB in the upper 32 bits */
	PAINT(paint_ptr)->paint.setColor((SkColor)(uint32_t)(color_long >> 32));
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetAlpha(JNIEnv *env, jclass, jlong paint_ptr, jint alpha)
{
	PAINT(paint_ptr)->paint.setAlpha(alpha & 0xFF);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetStyle(JNIEnv *env, jclass, jlong paint_ptr)
{
	return (jint)PAINT(paint_ptr)->paint.getStyle();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetStyle(JNIEnv *env, jclass, jlong paint_ptr, jint style)
{
	PAINT(paint_ptr)->paint.setStyle((SkPaint::Style)style);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetStrokeCap(JNIEnv *env, jclass, jlong paint_ptr)
{
	return (jint)PAINT(paint_ptr)->paint.getStrokeCap();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetStrokeCap(JNIEnv *env, jclass, jlong paint_ptr, jint cap)
{
	PAINT(paint_ptr)->paint.setStrokeCap((SkPaint::Cap)cap);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetStrokeJoin(JNIEnv *env, jclass, jlong paint_ptr)
{
	return (jint)PAINT(paint_ptr)->paint.getStrokeJoin();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetStrokeJoin(JNIEnv *env, jclass, jlong paint_ptr, jint join)
{
	PAINT(paint_ptr)->paint.setStrokeJoin((SkPaint::Join)join);
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetStrokeWidth(JNIEnv *env, jclass, jlong paint_ptr, jfloat width)
{
	PAINT(paint_ptr)->paint.setStrokeWidth(width);
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetStrokeWidth(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->paint.getStrokeWidth();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetStrokeMiter(JNIEnv *env, jclass, jlong paint_ptr, jfloat miter)
{
	PAINT(paint_ptr)->paint.setStrokeMiter(miter);
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetStrokeMiter(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->paint.getStrokeMiter();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Paint_nGetFillPath(JNIEnv *env, jclass, jlong paint_ptr, jlong src_ptr, jlong dst_ptr)
{
	return skpathutils::FillPathWithPaint(*(SkPath *)_PTR(src_ptr), PAINT(paint_ptr)->paint,
	                                      (SkPath *)_PTR(dst_ptr));
}

/* --- effects --- */

JNIEXPORT jlong JNICALL Java_android_graphics_Paint_nSetShader(JNIEnv *env, jclass, jlong paint_ptr, jlong shader_ptr)
{
	AndroidPaint *paint = PAINT(paint_ptr);
	if (shader_ptr)
		paint->paint.setShader(((ATLShader *)_PTR(shader_ptr))->effective());
	else
		paint->paint.setShader(nullptr);
	return shader_ptr;
}

JNIEXPORT jlong JNICALL Java_android_graphics_Paint_nSetColorFilter(JNIEnv *env, jclass, jlong paint_ptr, jlong filter_ptr)
{
	/* adopt the reference created by ColorFilter.getNativeInstance() */
	PAINT(paint_ptr)->paint.setColorFilter(sk_sp<SkColorFilter>((SkColorFilter *)_PTR(filter_ptr)));
	return filter_ptr;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetXfermode(JNIEnv *env, jclass, jlong paint_ptr, jint porterduff_mode)
{
	AndroidPaint *paint = PAINT(paint_ptr);
	if (porterduff_mode == 3 /* SRC_OVER, the default */)
		paint->paint.setBlendMode(SkBlendMode::kSrcOver);
	else
		paint->paint.setBlendMode(porter_duff_to_blend_mode(porterduff_mode));
}

JNIEXPORT jlong JNICALL Java_android_graphics_Paint_nSetPathEffect(JNIEnv *env, jclass, jlong paint_ptr, jlong effect_ptr)
{
	/* PathEffect subclasses are not natively backed yet (handle is 0) */
	if (!effect_ptr)
		PAINT(paint_ptr)->paint.setPathEffect(nullptr);
	return effect_ptr;
}

JNIEXPORT jlong JNICALL Java_android_graphics_Paint_nSetMaskFilter(JNIEnv *env, jclass, jlong paint_ptr, jlong filter_ptr)
{
	if (!filter_ptr)
		PAINT(paint_ptr)->paint.setMaskFilter(nullptr);
	return filter_ptr;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetTypeface(JNIEnv *env, jclass, jlong paint_ptr, jlong typeface_ptr)
{
	AndroidPaint *paint = PAINT(paint_ptr);
	paint->typeface = (android::Typeface *)_PTR(typeface_ptr);
	/* keep the SkFont in sync for legacy draw paths */
	if (paint->typeface) {
		const auto &families = paint->typeface->fFontCollection->getFamilies();
		if (!families.empty()) {
			auto closest = families[0]->getClosestMatch(paint->typeface->fStyle);
			paint->font.setTypeface(
			    static_cast<const android::MinikinFontSkia *>(closest.font->typeface().get())
			        ->RefSkTypeface());
		}
	} else {
		paint->font.setTypeface(atl_default_typeface());
	}
}

/* --- shadow layer --- */

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetShadowLayer(JNIEnv *env, jclass, jlong paint_ptr, jfloat radius, jfloat dx, jfloat dy, jlong colorspace_ptr, jlong color_long)
{
	AndroidPaint *paint = PAINT(paint_ptr);
	paint->shadow_radius = radius;
	paint->shadow_dx = dx;
	paint->shadow_dy = dy;
	paint->shadow_color = (SkColor)(uint32_t)(color_long >> 32);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Paint_nHasShadowLayer(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->shadow_radius > 0;
}

/* --- text attributes --- */

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetTextAlign(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->text_align;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetTextAlign(JNIEnv *env, jclass, jlong paint_ptr, jint align)
{
	PAINT(paint_ptr)->text_align = align;
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nSetTextLocales(JNIEnv *env, jclass, jlong paint_ptr, jstring locales)
{
	const char *str = env->GetStringUTFChars(locales, NULL);
	uint32_t id = minikin::registerLocaleList(str);
	env->ReleaseStringUTFChars(locales, str);
	PAINT(paint_ptr)->locale_list_id = id;
	return id;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetTextLocalesByMinikinLocaleListId(JNIEnv *env, jclass, jlong paint_ptr, jint id)
{
	PAINT(paint_ptr)->locale_list_id = id;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetFontFeatureSettings(JNIEnv *env, jclass, jlong paint_ptr, jstring settings)
{
	AndroidPaint *paint = PAINT(paint_ptr);
	if (!settings) {
		paint->font_feature_settings.clear();
		return;
	}
	const char *str = env->GetStringUTFChars(settings, NULL);
	paint->font_feature_settings = str;
	env->ReleaseStringUTFChars(settings, str);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Paint_nIsElegantTextHeight(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->elegant_text_height;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetElegantTextHeight(JNIEnv *env, jclass, jlong paint_ptr, jboolean elegant)
{
	PAINT(paint_ptr)->elegant_text_height = elegant;
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetTextSize(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->font.getSize();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetTextSize(JNIEnv *env, jclass, jlong paint_ptr, jfloat size)
{
	AndroidPaint *paint = PAINT(paint_ptr);
	paint->font.setSize(size);
	pango_font_description_set_absolute_size(paint->pango_font, roundf(size * PANGO_SCALE));
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetTextScaleX(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->font.getScaleX();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetTextScaleX(JNIEnv *env, jclass, jlong paint_ptr, jfloat scale)
{
	PAINT(paint_ptr)->font.setScaleX(scale);
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetTextSkewX(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->font.getSkewX();
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetTextSkewX(JNIEnv *env, jclass, jlong paint_ptr, jfloat skew)
{
	PAINT(paint_ptr)->font.setSkewX(skew);
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetLetterSpacing(JNIEnv *env, jclass, jlong paint_ptr)
{
	AndroidPaint *paint = PAINT(paint_ptr);
	float size = paint->font.getSize();
	return size != 0 ? paint->letter_spacing / size : 0;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetLetterSpacing(JNIEnv *env, jclass, jlong paint_ptr, jfloat spacing)
{
	/* Android's unit is EMs; minikin wants pixels */
	AndroidPaint *paint = PAINT(paint_ptr);
	paint->letter_spacing = spacing * paint->font.getSize();
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetWordSpacing(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->word_spacing;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetWordSpacing(JNIEnv *env, jclass, jlong paint_ptr, jfloat spacing)
{
	PAINT(paint_ptr)->word_spacing = spacing;
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetStartHyphenEdit(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->start_hyphen_edit;
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetEndHyphenEdit(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->end_hyphen_edit;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetStartHyphenEdit(JNIEnv *env, jclass, jlong paint_ptr, jint hyphen)
{
	PAINT(paint_ptr)->start_hyphen_edit = hyphen;
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nSetEndHyphenEdit(JNIEnv *env, jclass, jlong paint_ptr, jint hyphen)
{
	PAINT(paint_ptr)->end_hyphen_edit = hyphen;
}

/* --- metrics --- */

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nAscent(JNIEnv *env, jclass, jlong paint_ptr)
{
	SkFontMetrics metrics;
	PAINT(paint_ptr)->font.getMetrics(&metrics);
	return metrics.fAscent;
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nDescent(JNIEnv *env, jclass, jlong paint_ptr)
{
	SkFontMetrics metrics;
	PAINT(paint_ptr)->font.getMetrics(&metrics);
	return metrics.fDescent;
}

static float fill_font_metrics(JNIEnv *env, jlong paint_ptr, jobject obj, bool as_int)
{
	SkFontMetrics metrics;
	PAINT(paint_ptr)->font.getMetrics(&metrics);
	if (obj) {
		jclass clazz = env->GetObjectClass(obj);
		if (as_int) {
			env->SetIntField(obj, env->GetFieldID(clazz, "top", "I"), (jint)floorf(metrics.fTop));
			env->SetIntField(obj, env->GetFieldID(clazz, "ascent", "I"), (jint)roundf(metrics.fAscent));
			env->SetIntField(obj, env->GetFieldID(clazz, "descent", "I"), (jint)roundf(metrics.fDescent));
			env->SetIntField(obj, env->GetFieldID(clazz, "bottom", "I"), (jint)ceilf(metrics.fBottom));
			env->SetIntField(obj, env->GetFieldID(clazz, "leading", "I"), (jint)roundf(metrics.fLeading));
		} else {
			env->SetFloatField(obj, env->GetFieldID(clazz, "top", "F"), metrics.fTop);
			env->SetFloatField(obj, env->GetFieldID(clazz, "ascent", "F"), metrics.fAscent);
			env->SetFloatField(obj, env->GetFieldID(clazz, "descent", "F"), metrics.fDescent);
			env->SetFloatField(obj, env->GetFieldID(clazz, "bottom", "F"), metrics.fBottom);
			env->SetFloatField(obj, env->GetFieldID(clazz, "leading", "F"), metrics.fLeading);
		}
	}
	return metrics.fDescent - metrics.fAscent + metrics.fLeading;
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetFontMetrics(JNIEnv *env, jclass, jlong paint_ptr, jobject metrics)
{
	return fill_font_metrics(env, paint_ptr, metrics, false);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetFontMetricsInt(JNIEnv *env, jclass, jlong paint_ptr, jobject fmi)
{
	return (jint)ceilf(fill_font_metrics(env, paint_ptr, fmi, true));
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nGetFontMetricsIntForText__J_3CIIIIZLandroid_graphics_Paint_00024FontMetricsInt_2(JNIEnv *env, jclass, jlong paint_ptr, jcharArray text, jint start, jint count, jint ctx_start, jint ctx_count, jboolean is_rtl, jobject fmi)
{
	fill_font_metrics(env, paint_ptr, fmi, true);
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nGetFontMetricsIntForText__JLjava_lang_String_2IIIIZLandroid_graphics_Paint_00024FontMetricsInt_2(JNIEnv *env, jclass, jlong paint_ptr, jstring text, jint start, jint count, jint ctx_start, jint ctx_count, jboolean is_rtl, jobject fmi)
{
	fill_font_metrics(env, paint_ptr, fmi, true);
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetUnderlinePosition(JNIEnv *env, jclass, jlong paint_ptr)
{
	SkFontMetrics metrics;
	PAINT(paint_ptr)->font.getMetrics(&metrics);
	SkScalar position;
	if (metrics.hasUnderlinePosition(&position))
		return position;
	return PAINT(paint_ptr)->font.getSize() * (1.0f / 9.0f);
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetUnderlineThickness(JNIEnv *env, jclass, jlong paint_ptr)
{
	SkFontMetrics metrics;
	PAINT(paint_ptr)->font.getMetrics(&metrics);
	SkScalar thickness;
	if (metrics.hasUnderlineThickness(&thickness))
		return thickness;
	return PAINT(paint_ptr)->font.getSize() * (1.0f / 18.0f);
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetStrikeThruPosition(JNIEnv *env, jclass, jlong paint_ptr)
{
	return -PAINT(paint_ptr)->font.getSize() * (6.0f / 21.0f);
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetStrikeThruThickness(JNIEnv *env, jclass, jlong paint_ptr)
{
	return PAINT(paint_ptr)->font.getSize() * (1.0f / 18.0f);
}

/* --- text measurement (minikin) --- */

static minikin::Bidi to_bidi(jint bidi_flags)
{
	return (minikin::Bidi)bidi_flags;
}

/* measure the context range; write the [start, start+count) advances slice
 * into out (when non-null) and return their sum */
static float advances_slice(AndroidPaint *paint, const jchar *ctx, int ctx_count, int start_in_ctx,
                            int count, jint bidi_flags, jfloat *out)
{
	std::vector<float> advances(ctx_count);
	android::minikin_measure_text(paint, ctx, ctx_count, to_bidi(bidi_flags), advances.data());
	float sum = 0;
	for (int i = 0; i < count; i++) {
		sum += advances[start_in_ctx + i];
		if (out)
			out[i] = advances[start_in_ctx + i];
	}
	return sum;
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetTextAdvances__J_3CIIIII_3FI(JNIEnv *env, jclass, jlong paint_ptr, jcharArray text_arr, jint index, jint count, jint ctx_index, jint ctx_count, jint bidi_flags, jfloatArray advances_arr, jint advances_index)
{
	jchar *chars = env->GetCharArrayElements(text_arr, NULL);
	jfloat *advances = advances_arr ? env->GetFloatArrayElements(advances_arr, NULL) : nullptr;
	float sum = advances_slice(PAINT(paint_ptr), chars + ctx_index, ctx_count, index - ctx_index,
	                           count, bidi_flags, advances ? advances + advances_index : nullptr);
	if (advances)
		env->ReleaseFloatArrayElements(advances_arr, advances, 0);
	env->ReleaseCharArrayElements(text_arr, chars, JNI_ABORT);
	return sum;
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetTextAdvances__JLjava_lang_String_2IIIII_3FI(JNIEnv *env, jclass, jlong paint_ptr, jstring text, jint start, jint end, jint ctx_start, jint ctx_end, jint bidi_flags, jfloatArray advances_arr, jint advances_index)
{
	const jchar *chars = env->GetStringChars(text, NULL);
	jfloat *advances = advances_arr ? env->GetFloatArrayElements(advances_arr, NULL) : nullptr;
	float sum = advances_slice(PAINT(paint_ptr), chars + ctx_start, ctx_end - ctx_start,
	                           start - ctx_start, end - start, bidi_flags,
	                           advances ? advances + advances_index : nullptr);
	if (advances)
		env->ReleaseFloatArrayElements(advances_arr, advances, 0);
	env->ReleaseStringChars(text, chars);
	return sum;
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nBreakText__J_3CIIFI_3F(JNIEnv *env, jclass, jlong paint_ptr, jcharArray text_arr, jint index, jint count, jfloat max_width, jint bidi_flags, jfloatArray measured_arr)
{
	bool backwards = count < 0;
	int n = backwards ? -count : count;
	jchar *chars = env->GetCharArrayElements(text_arr, NULL);
	std::vector<float> advances(n);
	android::minikin_measure_text(PAINT(paint_ptr), chars + index, n, to_bidi(bidi_flags), advances.data());
	env->ReleaseCharArrayElements(text_arr, chars, JNI_ABORT);
	float measured = 0;
	int fitted = 0;
	while (fitted < n) {
		int i = backwards ? n - 1 - fitted : fitted;
		if (measured + advances[i] > max_width && advances[i] > 0)
			break;
		measured += advances[i];
		fitted++;
	}
	if (measured_arr) {
		jfloat measured_f = measured;
		env->SetFloatArrayRegion(measured_arr, 0, 1, &measured_f);
	}
	return fitted;
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nBreakText__JLjava_lang_String_2ZFI_3F(JNIEnv *env, jclass, jlong paint_ptr, jstring text, jboolean measure_forwards, jfloat max_width, jint bidi_flags, jfloatArray measured_arr)
{
	int len = env->GetStringLength(text);
	jcharArray arr = env->NewCharArray(len);
	const jchar *chars = env->GetStringChars(text, NULL);
	env->SetCharArrayRegion(arr, 0, len, chars);
	env->ReleaseStringChars(text, chars);
	jint result = Java_android_graphics_Paint_nBreakText__J_3CIIFI_3F(
	    env, nullptr, paint_ptr, arr, 0, measure_forwards ? len : -len, max_width, bidi_flags,
	    measured_arr);
	env->DeleteLocalRef(arr);
	return result;
}

JNIEXPORT jfloat JNICALL Java_android_graphics_Paint_nGetRunAdvance(JNIEnv *env, jclass, jlong paint_ptr, jcharArray text_arr, jint start, jint end, jint ctx_start, jint ctx_end, jboolean is_rtl, jint offset)
{
	jchar *chars = env->GetCharArrayElements(text_arr, NULL);
	int ctx_count = ctx_end - ctx_start;
	std::vector<float> advances(ctx_count);
	android::minikin_measure_text(PAINT(paint_ptr), chars + ctx_start, ctx_count,
	                              is_rtl ? minikin::Bidi::FORCE_RTL : minikin::Bidi::FORCE_LTR,
	                              advances.data());
	float advance = minikin::getRunAdvance(advances.data(), (const uint16_t *)chars + ctx_start,
	                                       start - ctx_start, end - start, offset - ctx_start);
	env->ReleaseCharArrayElements(text_arr, chars, JNI_ABORT);
	return advance;
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetOffsetForAdvance(JNIEnv *env, jclass, jlong paint_ptr, jcharArray text_arr, jint start, jint end, jint ctx_start, jint ctx_end, jboolean is_rtl, jfloat advance)
{
	jchar *chars = env->GetCharArrayElements(text_arr, NULL);
	int ctx_count = ctx_end - ctx_start;
	std::vector<float> advances(ctx_count);
	android::minikin_measure_text(PAINT(paint_ptr), chars + ctx_start, ctx_count,
	                              is_rtl ? minikin::Bidi::FORCE_RTL : minikin::Bidi::FORCE_LTR,
	                              advances.data());
	size_t offset = minikin::getOffsetForAdvance(advances.data(), (const uint16_t *)chars + ctx_start,
	                                             start - ctx_start, end - start, advance);
	env->ReleaseCharArrayElements(text_arr, chars, JNI_ABORT);
	return offset + ctx_start;
}

static jint text_run_cursor(JNIEnv *env, jlong paint_ptr, const jchar *chars, jint ctx_start, jint ctx_count, jint offset, jint cursor_opt)
{
	std::vector<float> advances(ctx_count);
	android::minikin_measure_text(PAINT(paint_ptr), chars + ctx_start, ctx_count,
	                              minikin::Bidi::DEFAULT_LTR, advances.data());
	return minikin::GraphemeBreak::getTextRunCursor(advances.data(), (const uint16_t *)chars,
	                                                ctx_start, ctx_count, offset,
	                                                (minikin::GraphemeBreak::MoveOpt)cursor_opt);
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetTextRunCursor__J_3CIIIII(JNIEnv *env, jobject, jlong paint_ptr, jcharArray text_arr, jint ctx_start, jint ctx_count, jint dir, jint offset, jint cursor_opt)
{
	jchar *chars = env->GetCharArrayElements(text_arr, NULL);
	jint result = text_run_cursor(env, paint_ptr, chars, ctx_start, ctx_count, offset, cursor_opt);
	env->ReleaseCharArrayElements(text_arr, chars, JNI_ABORT);
	return result;
}

JNIEXPORT jint JNICALL Java_android_graphics_Paint_nGetTextRunCursor__JLjava_lang_String_2IIIII(JNIEnv *env, jobject, jlong paint_ptr, jstring text, jint ctx_start, jint ctx_end, jint dir, jint offset, jint cursor_opt)
{
	const jchar *chars = env->GetStringChars(text, NULL);
	jint result = text_run_cursor(env, paint_ptr, chars, ctx_start, ctx_end - ctx_start, offset, cursor_opt);
	env->ReleaseStringChars(text, chars);
	return result;
}

/* --- text bounds / outlines / glyphs --- */

static void get_text_bounds(JNIEnv *env, jlong paint_ptr, const jchar *chars, int count, jobject bounds)
{
	AndroidPaint *paint = PAINT(paint_ptr);
	SkRect rect;
	paint->font.measureText(chars, count * sizeof(jchar), SkTextEncoding::kUTF16, &rect,
	                        &paint->paint);
	jclass bounds_class = env->GetObjectClass(bounds);
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "left", "I"), (jint)floorf(rect.left()));
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "top", "I"), (jint)floorf(rect.top()));
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "right", "I"), (jint)ceilf(rect.right()));
	env->SetIntField(bounds, env->GetFieldID(bounds_class, "bottom", "I"), (jint)ceilf(rect.bottom()));
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nGetStringBounds(JNIEnv *env, jclass, jlong paint_ptr, jstring text, jint start, jint end, jint bidi_flags, jobject bounds)
{
	const jchar *chars = env->GetStringChars(text, NULL);
	get_text_bounds(env, paint_ptr, chars + start, end - start, bounds);
	env->ReleaseStringChars(text, chars);
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nGetCharArrayBounds(JNIEnv *env, jclass, jlong paint_ptr, jcharArray text_arr, jint index, jint count, jint bidi_flags, jobject bounds)
{
	jchar *chars = env->GetCharArrayElements(text_arr, NULL);
	get_text_bounds(env, paint_ptr, chars + index, count, bounds);
	env->ReleaseCharArrayElements(text_arr, chars, JNI_ABORT);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Paint_nHasGlyph(JNIEnv *env, jclass, jlong paint_ptr, jint bidi_flags, jstring string)
{
	/* shape and check that nothing maps to the missing glyph */
	const jchar *chars = env->GetStringChars(string, NULL);
	int len = env->GetStringLength(string);
	minikin::U16StringPiece str((const uint16_t *)chars, len);
	minikin::Range range(0, len);
	minikin::Layout layout(str, range, to_bidi(bidi_flags),
	                       android::minikin_paint_for(PAINT(paint_ptr)),
	                       minikin::StartHyphenEdit::NO_EDIT, minikin::EndHyphenEdit::NO_EDIT);
	env->ReleaseStringChars(string, chars);
	if (layout.nGlyphs() == 0)
		return false;
	for (size_t i = 0; i < layout.nGlyphs(); i++)
		if (layout.getGlyphId(i) == 0)
			return false;
	return true;
}

static void get_text_path(JNIEnv *env, jlong paint_ptr, const jchar *chars, int count, float x, float y, jlong path_ptr)
{
	AndroidPaint *paint = PAINT(paint_ptr);
	SkPath *dst = (SkPath *)_PTR(path_ptr);
	dst->reset();
	minikin::U16StringPiece str((const uint16_t *)chars, count);
	minikin::Range range(0, count);
	minikin::Layout layout(str, range, minikin::Bidi::DEFAULT_LTR,
	                       android::minikin_paint_for(paint),
	                       minikin::StartHyphenEdit::NO_EDIT, minikin::EndHyphenEdit::NO_EDIT);
	if (paint->text_align == 1)
		x -= layout.getAdvance() / 2;
	else if (paint->text_align == 2)
		x -= layout.getAdvance();
	for (size_t i = 0; i < layout.nGlyphs(); i++) {
		SkFont font = paint->font;
		android::MinikinFontSkia::populateSkFont(&font, layout.getFont(i)->typeface().get(),
		                                         layout.getFakery(i));
		SkPath glyph_path;
		if (font.getPath(layout.getGlyphId(i), &glyph_path)) {
			glyph_path.offset(x + layout.getX(i), y + layout.getY(i));
			dst->addPath(glyph_path);
		}
	}
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nGetTextPath__JI_3CIIFFJ(JNIEnv *env, jclass, jlong paint_ptr, jint bidi_flags, jcharArray text_arr, jint index, jint count, jfloat x, jfloat y, jlong path_ptr)
{
	jchar *chars = env->GetCharArrayElements(text_arr, NULL);
	get_text_path(env, paint_ptr, chars + index, count, x, y, path_ptr);
	env->ReleaseCharArrayElements(text_arr, chars, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_graphics_Paint_nGetTextPath__JILjava_lang_String_2IIFFJ(JNIEnv *env, jclass, jlong paint_ptr, jint bidi_flags, jstring text, jint start, jint end, jfloat x, jfloat y, jlong path_ptr)
{
	const jchar *chars = env->GetStringChars(text, NULL);
	get_text_path(env, paint_ptr, chars + start, end - start, x, y, path_ptr);
	env->ReleaseStringChars(text, chars);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Paint_nEqualsForTextMeasurement(JNIEnv *env, jclass, jlong left_ptr, jlong right_ptr)
{
	AndroidPaint *a = PAINT(left_ptr), *b = PAINT(right_ptr);
	return a->typeface == b->typeface && a->font.getSize() == b->font.getSize() &&
	       a->font.getScaleX() == b->font.getScaleX() && a->font.getSkewX() == b->font.getSkewX() &&
	       a->letter_spacing == b->letter_spacing && a->word_spacing == b->word_spacing &&
	       a->font_feature_settings == b->font_feature_settings &&
	       a->locale_list_id == b->locale_list_id && a->flags == b->flags;
}

/* --- ColorFilter natives (kept here to avoid a separate tiny TU) --- */

JNIEXPORT jlong JNICALL Java_android_graphics_PorterDuffColorFilter_native_1CreateBlendModeFilter(JNIEnv *env, jclass, jint color, jint porterduff_mode)
{
	return _INTPTR(SkColorFilters::Blend((SkColor)(uint32_t)color,
	                                     porter_duff_to_blend_mode(porterduff_mode)).release());
}
