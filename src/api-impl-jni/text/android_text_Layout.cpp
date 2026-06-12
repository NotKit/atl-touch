#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "../defines.h"
#include "../jni_cpp.h"
#include "../graphics/ATLCanvas.h"
#include "../graphics/AndroidPaint.h"

extern "C" {
#include "../generated_headers/android_text_Layout.h"
}

static PangoContext *atl_pango_context(void)
{
	static PangoContext *context = pango_font_map_create_context(pango_cairo_font_map_get_default());
	return context;
}

JNIEXPORT jlong JNICALL Java_android_text_Layout_native_1constructor(JNIEnv *env, jclass clazz, jstring text, jlong paint, jint width)
{
	AndroidPaint *android_paint = (AndroidPaint *)_PTR(paint);
	PangoLayout *layout = pango_layout_new(atl_pango_context());
	pango_layout_set_font_description(layout, android_paint->pango_font);
	const char *str = env->GetStringUTFChars(text, NULL);
	pango_layout_set_text(layout, str, -1);
	env->ReleaseStringUTFChars(text, str);
	pango_layout_set_width(layout, width == -1 ? -1 : width * PANGO_SCALE);
	return _INTPTR(layout);
}

JNIEXPORT void JNICALL Java_android_text_Layout_native_1set_1width(JNIEnv *env, jobject object, jlong layout, jint width)
{
	pango_layout_set_width(PANGO_LAYOUT(_PTR(layout)), width * PANGO_SCALE);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1width(JNIEnv *env, jobject object, jlong layout)
{
	return pango_layout_get_width(PANGO_LAYOUT(_PTR(layout))) / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1height(JNIEnv *env, jobject object, jlong layout)
{
	PangoRectangle ink_rect;
	PangoRectangle logical_rect;
	pango_layout_get_extents(PANGO_LAYOUT(_PTR(layout)), &ink_rect, &logical_rect);
	return logical_rect.height / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1count(JNIEnv *env, jobject object, jlong layout)
{
	return pango_layout_get_line_count(PANGO_LAYOUT(_PTR(layout)));
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1start(JNIEnv *env, jobject object, jlong layout, jint line)
{
	PangoLayout *pango_layout = PANGO_LAYOUT(_PTR(layout));
	PangoLayoutLine *pango_line = pango_layout_get_line_readonly(pango_layout, line);
	int byte_index = pango_layout_line_get_start_index(pango_line);
	return g_utf8_strlen(pango_layout_get_text(pango_layout), byte_index);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1end(JNIEnv *env, jobject object, jlong layout, jint line)
{
	PangoLayout *pango_layout = PANGO_LAYOUT(_PTR(layout));
	PangoLayoutLine *pango_line = pango_layout_get_line_readonly(pango_layout, line);
	int byte_index = pango_layout_line_get_start_index(pango_line) + pango_layout_line_get_length(pango_line);
	return g_utf8_strlen(pango_layout_get_text(pango_layout), byte_index);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1top(JNIEnv *env, jobject object, jlong layout, jint line)
{
	PangoLayoutLine *pango_line = pango_layout_get_line_readonly(PANGO_LAYOUT(_PTR(layout)), line);
	PangoRectangle logical_rect;
	pango_layout_line_get_extents(pango_line, &logical_rect, NULL);
	return (logical_rect.y) / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1bottom(JNIEnv *env, jobject object, jlong layout, jint line)
{
	PangoLayoutLine *pango_line = pango_layout_get_line_readonly(PANGO_LAYOUT(_PTR(layout)), line);
	PangoRectangle logical_rect;
	pango_layout_line_get_extents(pango_line, &logical_rect, NULL);
	return (logical_rect.y + logical_rect.height) / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1left(JNIEnv *env, jobject object, jlong layout, jint line)
{
	PangoLayoutLine *pango_line = pango_layout_get_line_readonly(PANGO_LAYOUT(_PTR(layout)), line);
	PangoRectangle logical_rect;
	pango_layout_line_get_extents(pango_line, &logical_rect, NULL);
	return logical_rect.x / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1right(JNIEnv *env, jobject object, jlong layout, jint line)
{
	PangoLayoutLine *pango_line = pango_layout_get_line_readonly(PANGO_LAYOUT(_PTR(layout)), line);
	PangoRectangle logical_rect;
	pango_layout_line_get_extents(pango_line, &logical_rect, NULL);
	return (logical_rect.x + logical_rect.width) / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1width(JNIEnv *env, jobject object, jlong layout, jint line)
{
	PangoLayoutLine *pango_line = pango_layout_get_line_readonly(PANGO_LAYOUT(_PTR(layout)), line);
	PangoRectangle logical_rect;
	pango_layout_line_get_extents(pango_line, &logical_rect, NULL);
	return logical_rect.width / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1baseline(JNIEnv *env, jobject object, jlong layout, jint line)
{
	PangoLayoutIter *pango_iter = pango_layout_get_iter(PANGO_LAYOUT(_PTR(layout)));
	while (line--)
		pango_layout_iter_next_line(pango_iter);

	return pango_layout_iter_get_baseline(pango_iter) / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1ascent(JNIEnv *env, jobject object, jlong layout, jint line)
{
	PangoLayoutLine *pango_line = pango_layout_get_line_readonly(PANGO_LAYOUT(_PTR(layout)), line);
	PangoRectangle logical_rect, ink_rect;
	pango_layout_line_get_extents(pango_line, &logical_rect, &ink_rect);
	return -PANGO_ASCENT(ink_rect) / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1descent(JNIEnv *env, jobject object, jlong layout, jint line)
{
	PangoLayoutLine *pango_line = pango_layout_get_line_readonly(PANGO_LAYOUT(_PTR(layout)), line);
	PangoRectangle logical_rect, ink_rect;
	pango_layout_line_get_extents(pango_line, &logical_rect, &ink_rect);
	return PANGO_DESCENT(ink_rect) / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1for_1vertical(JNIEnv *env, jobject object, jlong layout, jint y)
{
	PangoLayout *pango_layout = PANGO_LAYOUT(_PTR(layout));
	int index_, trailing;
	pango_layout_xy_to_index(pango_layout, 0, y * PANGO_SCALE, &index_, &trailing);
	int line, x_pos;
	pango_layout_index_to_line_x(pango_layout, index_, trailing, &line, &x_pos);
	return line;
}

JNIEXPORT void JNICALL Java_android_text_Layout_native_1set_1ellipsize(JNIEnv *env, jobject object, jlong layout, jint ellipsize_mode, jfloat ellipsize_width)
{
	PangoLayout *pango_layout = PANGO_LAYOUT(_PTR(layout));
	pango_layout_set_ellipsize(pango_layout, (PangoEllipsizeMode)ellipsize_mode);
	pango_layout_set_width(pango_layout, ellipsize_width * PANGO_SCALE);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1ellipsis_1count(JNIEnv *env, jobject object, jlong layout, jint line)
{
	return pango_layout_is_ellipsized(PANGO_LAYOUT(_PTR(layout)));
}

JNIEXPORT void JNICALL Java_android_text_Layout_native_1draw(JNIEnv *env, jobject object, jlong layout, jlong snapshot_ptr, jlong paint_ptr)
{
	PangoLayout *pango_layout = PANGO_LAYOUT(_PTR(layout));
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	AndroidPaint *android_paint = (AndroidPaint *)_PTR(paint_ptr);

	/* line breaking and positioning come from Pango; glyph rendering is Skia */
	const char *text = pango_layout_get_text(pango_layout);
	PangoLayoutIter *pango_iter = pango_layout_get_iter(pango_layout);
	do {
		PangoLayoutLine *pango_line = pango_layout_iter_get_line_readonly(pango_iter);
		if (pango_line->length == 0)
			continue;
		PangoRectangle line_rect;
		pango_layout_iter_get_line_extents(pango_iter, NULL, &line_rect);
		float x = (float)line_rect.x / PANGO_SCALE;
		float y = (float)pango_layout_iter_get_baseline(pango_iter) / PANGO_SCALE;
		atl_canvas->canvas->drawSimpleText(text + pango_line->start_index, pango_line->length,
		                                   SkTextEncoding::kUTF8, x, y,
		                                   android_paint->font, android_paint->paint);
	} while (pango_layout_iter_next_line(pango_iter));
	pango_layout_iter_free(pango_iter);
}

JNIEXPORT void JNICALL Java_android_text_Layout_native_1draw_1custom_1canvas(JNIEnv *env, jobject object, jlong layout, jobject canvas, jobject paint)
{
	PangoLayout *pango_layout = PANGO_LAYOUT(_PTR(layout));

	const gchar *text = pango_layout_get_text(pango_layout);
	PangoLayoutIter *pango_iter = pango_layout_get_iter(pango_layout);
	do {
		PangoLayoutLine *pango_line = pango_layout_iter_get_line_readonly(pango_iter);

		jstring text_jstr = env->NewStringUTF(text + pango_line->start_index);
		jint end = env->GetStringLength(text_jstr);
		if (pango_line->length < end)
			end = pango_line->length;
		jfloat y = (float)pango_layout_iter_get_baseline(pango_iter) / PANGO_SCALE;
		env->CallVoidMethod(canvas, handle_cache.canvas.drawText, text_jstr, (jint)0, end, (jfloat)0, y, paint);
		env->DeleteLocalRef(text_jstr);
	} while (pango_layout_iter_next_line(pango_iter));
	pango_layout_iter_free(pango_iter);
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1line_1for_1offset(JNIEnv *env, jobject object, jlong layout, jint offset)
{
	int line;
	pango_layout_index_to_line_x(PANGO_LAYOUT(_PTR(layout)), offset, FALSE, &line, NULL);
	return line;
}

JNIEXPORT jfloat JNICALL Java_android_text_Layout_native_1get_1primary_1horizontal(JNIEnv *env, jobject object, jlong layout, jint offset)
{
	PangoRectangle cursor_rect;
	pango_layout_get_cursor_pos(PANGO_LAYOUT(_PTR(layout)), offset, &cursor_rect, NULL);
	return (jfloat)cursor_rect.x / PANGO_SCALE;
}

JNIEXPORT jfloat JNICALL Java_android_text_Layout_native_1get_1secondary_1horizontal(JNIEnv *env, jobject object, jlong layout, jint offset)
{
	PangoRectangle cursor_rect;
	pango_layout_get_cursor_pos(PANGO_LAYOUT(_PTR(layout)), offset, NULL, &cursor_rect);
	return (jfloat)cursor_rect.x / PANGO_SCALE;
}

JNIEXPORT jint JNICALL Java_android_text_Layout_native_1get_1offset_1for_1horizontal(JNIEnv *env, jobject object, jlong layout, jint line, jfloat x)
{
	PangoLayoutLine *pango_line = pango_layout_get_line_readonly(PANGO_LAYOUT(_PTR(layout)), line);
	int index;
	pango_layout_line_x_to_index(pango_line, x * PANGO_SCALE, &index, NULL);
	return index;
}

JNIEXPORT jfloat JNICALL Java_android_text_Layout_native_1get_1desired_1width(JNIEnv *env, jclass clazz, jlong layout)
{
	int width;
	pango_layout_get_size(PANGO_LAYOUT(_PTR(layout)), &width, NULL);
	return (jfloat)width / PANGO_SCALE;
}

JNIEXPORT void JNICALL Java_android_text_Layout_native_1free(JNIEnv *env, jclass clazz, jlong layout)
{
	g_object_unref(PANGO_LAYOUT(_PTR(layout)));
}
