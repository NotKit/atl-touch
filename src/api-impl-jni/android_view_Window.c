#include <gio/gio.h>

#include "defines.h"
#include "util.h"

#include "ATLWindow.h"

#include "generated_headers/android_view_Window.h"

JNIEXPORT void JNICALL Java_android_view_Window_native_1set_1view_1root(JNIEnv *env, jobject this, jlong window, jobject view_root)
{
	atl_window_set_view_root((ATLWindow *)_PTR(window), env, view_root);
}

JNIEXPORT void JNICALL Java_android_view_Window_set_1title(JNIEnv *env, jobject this, jlong window, jstring title_jstr)
{
	const char *title = (*env)->GetStringUTFChars(env, title_jstr, NULL);
	atl_window_set_title((ATLWindow *)_PTR(window), title);
	(*env)->ReleaseStringUTFChars(env, title_jstr, title);
}

JNIEXPORT void JNICALL Java_android_view_Window_take_1input_1queue(JNIEnv *env, jobject this, jlong native_window, jobject callback, jobject queue)
{
	/* NativeActivity input was wired through a GTK event controller; it comes
	 * back together with native EGL surfaces in the GLFW+EGL phase */
	fprintf(stderr, "Window.takeInputQueue: not currently supported\n");
}

JNIEXPORT void JNICALL Java_android_view_Window_set_1layout(JNIEnv *env, jobject this, jlong window, jint width, jint height)
{
	atl_window_set_default_size((ATLWindow *)_PTR(window), width, height);
}

JNIEXPORT void JNICALL Java_android_view_Window_set_1jobject(JNIEnv *env, jclass this, jlong window, jobject window_jobj)
{
	atl_window_set_jobject((ATLWindow *)_PTR(window), env, window_jobj);
}

JNIEXPORT void JNICALL Java_android_view_Window_remove_1gtk_1background(JNIEnv *env, jobject this, jlong window)
{
	/* no GTK background anymore; the scene clears to white before drawing */
}

#define FLOAT_TO_POINTER(f) GINT_TO_POINTER(*(uint32_t *)(&f))
#define POINTER_TO_FLOAT(p) (*((float *)(&p)))

void set_brightness_done(GObject *source_object, GAsyncResult *res, gpointer data)
{
	float brightness = POINTER_TO_FLOAT(data);
	GVariant *result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source_object), res, NULL);
	if (result) {
		g_variant_unref(result);
	} else { // GNOME settings daemon not available. Try fallback to sysfs
		GDir *dir = g_dir_open("/sys/class/backlight", 0, NULL);
		if (!dir)
			return;

		const gchar *name;
		gchar *path;
		FILE *fp;
		while ((name = g_dir_read_name(dir))) {
			path = g_build_filename("/sys/class/backlight/", name, "/max_brightness", NULL);
			fp = fopen(path, "r");
			if (!fp) {
				g_printerr("Failed to read %s: %s\n", path, g_strerror(errno));
				g_free(path);
				continue;
			}
			g_free(path);
			int max = 0;
			fscanf(fp, "%d", &max);
			fclose(fp);

			path = g_build_filename("/sys/class/backlight/", name, "/brightness", NULL);
			fp = fopen(path, "w");
			if (!fp) {
				g_printerr("Failed to write %s: %s\n", path, g_strerror(errno));
				g_free(path);
				continue;
			}
			g_free(path);
			fprintf(fp, "%d", (int)(brightness * max));
			fclose(fp);
		}

		g_dir_close(dir);
	}
}

JNIEXPORT void JNICALL Java_android_view_Window_set_1screen_1brightness(JNIEnv *env, jobject this, jfloat brightness)
{
	GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	if (!connection)
		return;

	g_dbus_connection_call(connection, "org.gnome.SettingsDaemon.Power", "/org/gnome/SettingsDaemon/Power", "org.freedesktop.DBus.Properties", "Set",
	                       g_variant_new("(ssv)", "org.gnome.SettingsDaemon.Power.Screen", "Brightness", g_variant_new_int32(brightness * 100)),
	                       NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, set_brightness_done, FLOAT_TO_POINTER(brightness));

	g_object_unref(connection);
}
