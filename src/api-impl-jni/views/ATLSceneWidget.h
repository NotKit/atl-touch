#pragma once

#include <gtk/gtk.h>
#include <jni.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(ATLSceneWidget, atl_scene_widget, ATL, SCENE_WIDGET, GtkWidget)

/* One scene widget hosts an entire Android view hierarchy: it renders the
 * tree through a Skia canvas each frame and feeds all input events to the
 * given android.view.ViewRootImpl. */
GtkWidget *atl_scene_widget_new(JNIEnv *env, jobject view_root);

G_END_DECLS
