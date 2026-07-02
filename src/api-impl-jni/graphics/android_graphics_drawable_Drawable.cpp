#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "../defines.h"
#include "../jni_cpp.h"
#include "ATLCanvas.h"
#include "NinePatchPaintable.h"

#include "include/core/SkSamplingOptions.h"

extern "C" {
#include "../generated_headers/android_graphics_drawable_Drawable.h"
}

JNIEXPORT jlong JNICALL Java_android_graphics_drawable_Drawable_native_1paintable_1from_1path(JNIEnv *env, jclass clazz, jstring pathStr)
{
	const char *path = env->GetStringUTFChars(pathStr, NULL);
	GdkTexture *texture = gdk_texture_new_from_filename(path, NULL);
	env->ReleaseStringUTFChars(pathStr, path);
	return _INTPTR(texture);
}

struct _JavaPaintable {
	GObject parent_instance;
	jobject drawable;
};
G_DECLARE_FINAL_TYPE(JavaPaintable, java_paintable, JAVA, PAINTABLE, GObject)

/* GTK asks a java Drawable to paint itself: render through a raster ATLCanvas
 * and hand the pixels to the snapshot */
static void java_paintable_snapshot(GdkPaintable *gdk_paintable, GdkSnapshot *snapshot, double width, double height)
{
	JNIEnv *env = get_jni_env();
	JavaPaintable *paintable = JAVA_PAINTABLE(gdk_paintable);
	if (width < 1 || height < 1)
		return;
	void *atl_canvas = atl_canvas_new_raster((int)width, (int)height);
	jclass canvas_class = env->FindClass("android/view/DisplayListCanvas");
	jmethodID canvas_constructor = env->GetMethodID(canvas_class, "<init>", "(J)V");
	jobject canvas = env->NewObject(canvas_class, canvas_constructor, _INTPTR(atl_canvas));
	env->CallVoidMethod(paintable->drawable, handle_cache.drawable.setBounds, 0, 0, (int)width, (int)height);
	env->CallVoidMethod(paintable->drawable, handle_cache.drawable.draw, canvas);
	if (env->ExceptionCheck())
		env->ExceptionDescribe();
	env->DeleteLocalRef(canvas);
	env->DeleteLocalRef(canvas_class);
	GdkTexture *texture = atl_canvas_to_gdk_texture(atl_canvas);
	graphene_rect_t bounds = GRAPHENE_RECT_INIT(0.f, 0.f, (float)width, (float)height);
	gtk_snapshot_append_texture(GTK_SNAPSHOT(snapshot), texture, &bounds);
	g_object_unref(texture);
	atl_canvas_free(atl_canvas);
}

static int java_paintable_get_intrinsic_width(GdkPaintable *gdk_paintable)
{
	JNIEnv *env = get_jni_env();
	JavaPaintable *paintable = JAVA_PAINTABLE(gdk_paintable);
	jmethodID getIntrinsicWidth = env->GetMethodID(handle_cache.drawable.klass, "getIntrinsicWidth", "()I");
	int width = env->CallIntMethod(paintable->drawable, getIntrinsicWidth);
	return width > 0 ? width : 0;
}

static int java_paintable_get_intrinsic_height(GdkPaintable *gdk_paintable)
{
	JNIEnv *env = get_jni_env();
	JavaPaintable *paintable = JAVA_PAINTABLE(gdk_paintable);
	jmethodID getIntrinsicHeight = env->GetMethodID(handle_cache.drawable.klass, "getIntrinsicHeight", "()I");
	int height = env->CallIntMethod(paintable->drawable, getIntrinsicHeight);
	return height > 0 ? height : 0;
}

static void java_paintable_init(JavaPaintable *java_paintable)
{
}

static void java_paintable_paintable_init(GdkPaintableInterface *iface)
{
	iface->snapshot = java_paintable_snapshot;
	iface->get_intrinsic_height = java_paintable_get_intrinsic_height;
	iface->get_intrinsic_width = java_paintable_get_intrinsic_width;
}

static void java_paintable_dispose(GObject *object)
{
	JavaPaintable *java_paintable = JAVA_PAINTABLE(object);
	JNIEnv *env = get_jni_env();
	env->DeleteWeakGlobalRef(java_paintable->drawable);
}

static void java_paintable_class_init(JavaPaintableClass *clazz)
{
	G_OBJECT_CLASS(clazz)->dispose = java_paintable_dispose;
}

G_DEFINE_TYPE_WITH_CODE(JavaPaintable, java_paintable, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(GDK_TYPE_PAINTABLE, java_paintable_paintable_init))

JNIEXPORT jlong JNICALL Java_android_graphics_drawable_Drawable_native_1constructor(JNIEnv *env, jobject thiz)
{
	JavaPaintable *paintable = NULL;
	jclass this_class = env->GetObjectClass(thiz);
	if (handle_cache.drawable.draw != env->GetMethodID(this_class, "draw", "(Landroid/graphics/Canvas;)V")) {
		paintable = (JavaPaintable *)g_object_new(java_paintable_get_type(), NULL);
		paintable->drawable = env->NewWeakGlobalRef(thiz);
	}
	return _INTPTR(paintable);
}

static guint queue_invalidate_contents(GdkPaintable *paintable)
{
	gdk_paintable_invalidate_contents(paintable);
	g_object_unref(paintable);
	return G_SOURCE_REMOVE;
}

JNIEXPORT void JNICALL Java_android_graphics_drawable_Drawable_native_1invalidate(JNIEnv *env, jobject thiz, jlong paintable_ptr)
{
	// GTK doesn't allow invalidating a paintable while it's being drawn, so we need to queue it up
	g_idle_add_full(G_PRIORITY_HIGH_IDLE + 20, G_SOURCE_FUNC(queue_invalidate_contents), g_object_ref(GDK_PAINTABLE(_PTR(paintable_ptr))), NULL);
}

/* a java Drawable paints its native paintable onto an (ATL) canvas */
JNIEXPORT void JNICALL Java_android_graphics_drawable_Drawable_native_1draw(JNIEnv *env, jobject thiz, jlong paintable_ptr, jlong snapshot_ptr, jint width, jint height)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	GdkPaintable *paintable = GDK_PAINTABLE(_PTR(paintable_ptr));
	if (JAVA_IS_PAINTABLE(paintable))
		return; // the java side draws itself
	if (GDK_IS_TEXTURE(paintable)) {
		SkBitmap *bitmap = (SkBitmap *)atl_skbitmap_from_gdk_texture(GDK_TEXTURE(paintable));
		sk_sp<SkImage> image = atl_image_for_draw(atl_canvas, bitmap);
		if (image)
			atl_canvas->canvas->drawImageRect(image, SkRect::MakeWH(width, height),
			                                  SkSamplingOptions(SkFilterMode::kLinear), nullptr);
	} else if (NINEPATCH_IS_PAINTABLE(paintable)) {
		ninepatch_paintable_draw_skia(NINEPATCH_PAINTABLE(paintable), atl_canvas, width, height);
	} else {
		atl_canvas_draw_gdk_paintable(atl_canvas, paintable, width, height);
	}
}

JNIEXPORT void JNICALL Java_android_graphics_drawable_Drawable_native_1ref(JNIEnv *env, jobject thiz, jlong paintable_ptr)
{
	g_object_ref(G_OBJECT(_PTR(paintable_ptr)));
}

JNIEXPORT void JNICALL Java_android_graphics_drawable_Drawable_native_1unref(JNIEnv *env, jobject thiz, jlong paintable_ptr)
{
	g_object_unref(G_OBJECT(_PTR(paintable_ptr)));
}
