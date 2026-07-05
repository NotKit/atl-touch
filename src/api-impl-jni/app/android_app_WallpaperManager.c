#include <gio/gio.h>
#include <libportal/portal.h>

#include "../defines.h"
#include "../generated_headers/android_app_WallpaperManager.h"

static void wallpaper_ready_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	xdp_portal_set_wallpaper_finish(XDP_PORTAL(source), res, NULL);
	GFile *file = user_data;
	g_file_delete(file, NULL, NULL);
	g_object_unref(file);
}

JNIEXPORT void JNICALL Java_android_app_WallpaperManager_set_1wallpaper(JNIEnv *env, jclass clazz, jstring path_jstr)
{
	const char *path = (*env)->GetStringUTFChars(env, path_jstr, NULL);
	GFile *file = g_file_new_for_path(path);
	(*env)->ReleaseStringUTFChars(env, path_jstr, path);
	XdpPortal *portal = xdp_portal_new();
	char *uri = g_file_get_uri(file);
	xdp_portal_set_wallpaper(portal, NULL, uri, XDP_WALLPAPER_FLAG_NONE, NULL, wallpaper_ready_cb, file);
	g_free(uri);
	g_object_unref(portal);
}
