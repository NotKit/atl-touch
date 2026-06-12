#include <gtk/gtk.h>

#include "../defines.h"
#include "../util.h"

#include "../graphics/ATLCanvas.h"

#include "../generated_headers/android_view_SurfaceView.h"
#include "android_view_SurfaceView.h"

/* The SurfaceViewWidget GTK widget is no longer part of the view hierarchy
 * (SurfaceView draws its front buffer through the Skia scene); the type is
 * kept only because MediaCodec still posts video frames as GdkTextures.
 * TODO: route MediaCodec frames through the Skia scene as well. */

G_DEFINE_TYPE(SurfaceViewWidget, surface_view_widget, GTK_TYPE_WIDGET)

static void surface_view_widget_init(SurfaceViewWidget *surface_view_widget)
{
}

static void surface_view_widget_dispose(GObject *object)
{
	SurfaceViewWidget *surface_view_widget = SURFACE_VIEW_WIDGET(object);
	if (surface_view_widget->texture) {
		g_object_unref(surface_view_widget->texture);
		surface_view_widget->texture = NULL;
	}
	G_OBJECT_CLASS(surface_view_widget_parent_class)->dispose(object);
}

static void surface_view_widget_class_init(SurfaceViewWidgetClass *class)
{
	G_OBJECT_CLASS(class)->dispose = surface_view_widget_dispose;
}

GtkWidget *surface_view_widget_new(void)
{
	return g_object_new(surface_view_widget_get_type(), NULL);
}

void surface_view_widget_set_texture(SurfaceViewWidget *surface_view_widget, GdkTexture *texture, gboolean needs_flip)
{
	if (surface_view_widget->texture)
		g_object_unref(surface_view_widget->texture);
	surface_view_widget->texture = texture;
	surface_view_widget->needs_flip = needs_flip;
}

JNIEXPORT jlong JNICALL Java_android_view_SurfaceView_native_1createSnapshot(JNIEnv *env, jobject this, jint width, jint height)
{
	return _INTPTR(atl_canvas_new_raster(width, height));
}
