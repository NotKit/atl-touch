/*
 * Launcher for running an app on a stock JVM instead of ART.
 *
 * Same boot sequence as src/main-executable/main.c — create the VM, register
 * libtranslation_layer_main under the app's class loader, then
 * Context.createApplication -> ContentProvider.createContentProviders ->
 * Activity.createMainActivity, then the GLib main loop — but the framework and
 * the app come in as plain jars on the class path rather than dex, so there is
 * no dex install dir to find and no bionic linker to feed a library path to.
 *
 * Because a JVM has no libcore, the caller supplies the class path explicitly:
 * the pre-dex framework jar (--api-impl-jar), whatever compat jars the runtime
 * needs (--classpath) and the apk itself.
 *
 * This directory now holds two VM backends behind vm.h — the name is kept for
 * the paths and docs that point at it. This file is the boot sequence and is
 * shared by both; vm_hotspot.c creates the VM from libjvm.so, vm_image.c from a
 * GraalVM native-image shared library that has no class path at all. Exactly
 * one of them is linked into each executable.
 */

// for RTLD_NEXT/dladdr in the compat shims, and for asprintf
#define _GNU_SOURCE

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <GLFW/glfw3.h>
#include <gio/gio.h>
#include <jni.h>

#include "../api-impl-jni/ATLWindow.h"
#include "../api-impl-jni/app/android_app_Activity.h"
#include "../api-impl-jni/defines.h"
#include "../api-impl-jni/util.h"
#include "../main-executable/actions.h"
#include "vm.h"

#ifndef DEFFILEMODE
	#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) /* 0666 */
#endif

#define DIRMODE (DEFFILEMODE | S_IXUSR | S_IXGRP | S_IXOTH)

void remove_ongoing_notifications(void); // app/android_app_NotificationManager.c

/* referenced by libtranslation_layer_main.so, which expects the launcher to own them */
ATLWindow *atl_window = NULL;
char *apk_path = NULL;

/* the equivalent of /data/data/com.example.app/ */
static char *app_data_dir = NULL;
char *get_app_data_dir()
{
	return app_data_dir;
}

static bool exception_check(JNIEnv *env, const char *what)
{
	if (!(*env)->ExceptionCheck(env))
		return false;

	fprintf(stderr, "error: exception during %s:\n", what);
	(*env)->ExceptionDescribe(env);
	return true;
}

/*
 * Every step of the boot sequence hands its result to the next one, so carrying
 * on after one throws just turns a readable stack trace into a JVM crash a few
 * calls later.
 */
static void fatal_exception_check(JNIEnv *env, const char *what)
{
	if (exception_check(env, what))
		exit(1);
}

/*
 * System.load() registers a library with the class loader of its *calling*
 * class, and JNI method lookup only searches the loader of the class declaring
 * the method. Called straight from here there is no Java caller at all, so the
 * library would land on the boot loader and android.* natives would never
 * resolve. ART's two-argument Runtime.loadLibrary(String, ClassLoader) has no
 * JVM equivalent, so the framework provides a loader of its own.
 */
static void load_translation_layer(JNIEnv *env, const char *natives_dir)
{
	char *path = g_build_filename(natives_dir, "libtranslation_layer_main.so", NULL);

	jclass runtime_class = (*env)->FindClass(env, "android/atl/ATLRuntime");
	if (exception_check(env, "looking up android.atl.ATLRuntime")) {
		/* both backends land here, and each has its own way of not having the
		 * framework, so name neither one's option as if it were the only cause */
		fprintf(stderr, "error: the framework is not in this VM"
		                " (--api-impl-jar for the libjvm launcher; built in, for the image one)\n");
		exit(1);
	}

	(*env)->CallStaticVoidMethod(env, runtime_class,
	                             _STATIC_METHOD(runtime_class, "loadNativeLibrary", "(Ljava/lang/String;)V"),
	                             _JSTRING(path));
	if (exception_check(env, "loading libtranslation_layer_main.so"))
		exit(1);

	fprintf(stderr, "loaded %s\n", path);
	g_free(path);
}

/*
 * There is no clean callback for "the window has usable dimensions", so poll.
 */
static gboolean hacky_on_window_focus_changed_callback(JNIEnv *env)
{
	if (atl_window_get_width(atl_window) != 0) {
		activity_window_ready();
		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}

/* --smoke-test: prove the runtime is alive without running any app code */
static gboolean smoke_test_done(gpointer user_data)
{
	JNIEnv *env = get_jni_env();

	jclass system_clock = (*env)->FindClass(env, "android/os/SystemClock");
	jlong uptime = (*env)->CallStaticLongMethod(env, system_clock, _STATIC_METHOD(system_clock, "uptimeMillis", "()J"));
	if (exception_check(env, "SystemClock.uptimeMillis()"))
		exit(1);

	fprintf(stderr, "smoke test: android.os.SystemClock.uptimeMillis() resolved and returned %ld\n", (long)uptime);
	fprintf(stderr, "smoke test: window is %dpx wide\n", atl_window_get_width(atl_window));
	fprintf(stderr, "smoke test: passed\n");

	activity_close_all();
	exit(0);
}

/*
 * main(String[]) needs an empty String[], so java.lang.String has to be looked
 * up like any other class. Unchecked it is worse than useless on an image whose
 * jni-config omits it: FindClass returns NULL with an exception pending, and
 * NewObjectArray with a NULL element class then reports the miss as a failure of
 * whatever ran next.
 */
static jobjectArray empty_string_array(JNIEnv *env)
{
	jclass string_class = (*env)->FindClass(env, "java/lang/String");
	if (exception_check(env, "looking up java.lang.String") || !string_class)
		exit(1);
	return (*env)->NewObjectArray(env, 0, string_class, NULL);
}

/*
 * --run-class: run one class in the app's runtime instead of the application.
 * The framework, the class path and the native libraries are exactly what a
 * real run gets, which is what makes it useful for testing a single piece of an
 * app (or of the framework) without a UI.
 */
static gboolean run_class_main(gpointer user_data)
{
	const char *class_name = user_data;
	JNIEnv *env = get_jni_env();

	char *binary_name = g_strdelimit(g_strdup(class_name), ".", '/');
	jclass class = (*env)->FindClass(env, binary_name);
	g_free(binary_name);
	if (exception_check(env, "looking up the --run-class class"))
		exit(1);

	jmethodID main_method = (*env)->GetStaticMethodID(env, class, "main", "([Ljava/lang/String;)V");
	if (exception_check(env, "looking up its main(String[])"))
		exit(1);

	jobjectArray no_args = empty_string_array(env);
	(*env)->CallStaticVoidMethod(env, class, main_method, no_args);
	if (exception_check(env, "running the --run-class class"))
		exit(1);

	fprintf(stderr, "run-class: %s.main() returned\n", class_name);
	exit(0);
}

/*
 * --vm-check: run one class's main(String[]) as soon as the VM exists and exit.
 *
 * Unlike --run-class this happens before any framework setup at all — no
 * ATLRuntime, no handle cache, no window, no looper — so it works against a VM
 * that contains nothing but the checked class. That is what makes it the test
 * for a VM backend itself, rather than for the runtime the app needs.
 */
static void vm_check_main(JNIEnv *env, const char *class_name)
{
	char *binary_name = g_strdelimit(g_strdup(class_name), ".", '/');
	jclass class = (*env)->FindClass(env, binary_name);
	g_free(binary_name);
	if (exception_check(env, "looking up the --vm-check class"))
		exit(1);

	jmethodID main_method = (*env)->GetStaticMethodID(env, class, "main", "([Ljava/lang/String;)V");
	if (exception_check(env, "looking up its main(String[])"))
		exit(1);

	jobjectArray no_args = empty_string_array(env);
	(*env)->CallStaticVoidMethod(env, class, main_method, no_args);
	if (exception_check(env, "running the --vm-check class"))
		exit(1);

	fprintf(stderr, "vm check: %s.main() returned\n", class_name);
	exit(0);
}

static char *required_path(const char *path, const char *option, const char *what)
{
	if (!path) {
		fprintf(stderr, "error: %s is required (%s)\n", option, what);
		exit(1);
	}
	if (access(path, F_OK) < 0) {
		fprintf(stderr, "error: can't stat %s (%s)\n", path, strerror(errno));
		exit(1);
	}
	return g_strdup(path);
}

static void make_app_data_dir(const char *apk_name)
{
	char *base = getenv("ANDROID_APP_DATA_DIR");
	if (!base) {
		base = g_strdup_printf("%s/android_translation_layer", g_get_user_data_dir());
		if (mkdir(base, DIRMODE) && errno != EEXIST) {
			fprintf(stderr, "error: can't create %s (%s)\n", base, strerror(errno));
			exit(1);
		}
	}

	/* Unity can't comprehend a directory name ending in .apk, hence the trailing _ */
	app_data_dir = g_strdup_printf("%s/%s_/", base, apk_name);
	if (mkdir(app_data_dir, DIRMODE) && errno != EEXIST) {
		fprintf(stderr, "error: can't create app data dir %s (%s)\n", app_data_dir, strerror(errno));
		exit(1);
	}
}

static void open(GApplication *app, GFile **files, gint nfiles, const gchar *hint, struct launcher_options *d)
{
	if (atl_window) { /* a DBus request to the already running app */
		atl_window_focus(atl_window);
		return;
	}

	char *apk = g_file_get_path(files[0]);
	char *apk_name = g_file_get_basename(files[0]);
	if (!apk || access(apk, F_OK) < 0) {
		fprintf(stderr, "error: can't stat the given apk (%s)\n", strerror(errno));
		exit(1);
	}

	/* what a class path means differs per backend, so each one checks its own */
	vm_validate_options(d);
	d->natives_dir = required_path(d->natives_dir ?: getenv("ATL_NATIVES_DIR"),
	                               "--natives-dir", "the directory holding libtranslation_layer_main.so");
	if (d->framework_res_apk || getenv("ATL_FRAMEWORK_RES_APK"))
		d->framework_res_apk = required_path(d->framework_res_apk ?: getenv("ATL_FRAMEWORK_RES_APK"),
		                                     "--framework-res", "framework-res.apk");

	make_app_data_dir(apk_name);

	char *app_lib_dir = g_strdup_printf("%s/lib", app_data_dir);
	mkdir(app_lib_dir, DIRMODE);

	JNIEnv *env = create_vm(d, apk, app_lib_dir);
	g_free(app_lib_dir);

	if (d->vm_check) {
		vm_check_main(env, d->vm_check);
		return;
	}

	/* the first framework class either backend touches: unchecked, a VM without
	 * it reports the *next* miss instead, which sends the reader after the wrong
	 * one (it is how a stub image looks like a broken --api-impl-jar) */
	jclass display_class = (*env)->FindClass(env, "android/view/Display");
	fatal_exception_check(env, "looking up android.view.Display");
	_SET_STATIC_INT_FIELD(display_class, "window_width", d->window_width);
	_SET_STATIC_INT_FIELD(display_class, "window_height", d->window_height);

	load_translation_layer(env, d->natives_dir);

	/* some apps read their own apk */
	apk_path = g_strdup(apk);

	(*env)->GetJavaVM(env, &jvm);
	set_up_handle_cache(env);

	const char *disable_decoration_env = getenv("ATL_DISABLE_WINDOW_DECORATIONS");
	bool decorated = true;
	if (disable_decoration_env)
		decorated = !strcmp(disable_decoration_env, "0") || !strcmp(disable_decoration_env, "false");

	atl_windows_init();
	atl_window = atl_window_new(d->window_width, d->window_height, true, decorated);

	/* our windows are GLFW windows, so the GApplication has none of its own and
	 * would auto-quit as soon as this returns */
	g_application_hold(app);

	prepare_main_looper(env);

	if (d->smoke_test) {
		fprintf(stderr, "smoke test: no application will be created, quitting in %ds\n", d->smoke_test);
		g_timeout_add_seconds(d->smoke_test, smoke_test_done, NULL);
		return;
	}

	if (d->run_class) {
		fprintf(stderr, "run-class: running %s.main() instead of creating the application\n", d->run_class);
		g_idle_add(run_class_main, d->run_class);
		return;
	}

	jobject application_object = (*env)->CallStaticObjectMethod(env, handle_cache.context.class,
	                                                            _STATIC_METHOD(handle_cache.context.class, "createApplication", "(J)Landroid/app/Application;"),
	                                                            _INTPTR(atl_window));
	fatal_exception_check(env, "Context.createApplication");
	fprintf(stderr, "boot: application created\n");

	jclass content_provider = (*env)->FindClass(env, "android/content/ContentProvider");
	(*env)->CallStaticVoidMethod(env, content_provider, _STATIC_METHOD(content_provider, "createContentProviders", "()V"));
	fatal_exception_check(env, "ContentProvider.createContentProviders");
	fprintf(stderr, "boot: content providers created\n");

	(*env)->CallVoidMethod(env, application_object, _METHOD(handle_cache.application.class, "onCreate", "()V"));
	fatal_exception_check(env, "Application.onCreate");
	fprintf(stderr, "boot: Application.onCreate returned\n");

	jobject activity_object = (*env)->CallStaticObjectMethod(env, handle_cache.activity.class,
	                                                         _STATIC_METHOD(handle_cache.activity.class, "createMainActivity", "(Ljava/lang/String;JLjava/lang/String;)Landroid/app/Activity;"),
	                                                         _JSTRING(d->main_activity_class), _INTPTR(atl_window), NULL);
	fatal_exception_check(env, "Activity.createMainActivity");

	jstring package_name_jstr = (*env)->CallObjectMethod(env, application_object, handle_cache.context.get_package_name);
	if (!exception_check(env, "Context.getPackageName") && package_name_jstr)
		atl_window_set_title(atl_window, _CSTRING(package_name_jstr));

	const GLFWvidmode *monitor_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	jobject resources = _GET_STATIC_OBJ_FIELD(handle_cache.context.class, "r", "Landroid/content/res/Resources;");
	jobject configuration = _GET_OBJ_FIELD(resources, "mConfiguration", "Landroid/content/res/Configuration;");
	if (monitor_mode && monitor_mode->width >= 800 && monitor_mode->height >= 800)
		_SET_INT_FIELD(configuration, "screenLayout", /*SCREENLAYOUT_SIZE_LARGE*/ 0x03);
	else
		_SET_INT_FIELD(configuration, "screenLayout", /*SCREENLAYOUT_SIZE_NORMAL*/ 0x02);

	activity_start(env, activity_object);
	fatal_exception_check(env, "Activity.onStart");
	fprintf(stderr, "boot: main activity started\n");

	g_timeout_add(10, G_SOURCE_FUNC(hacky_on_window_focus_changed_callback), env);
}

static void activate(GApplication *app, struct launcher_options *d)
{
	if (atl_window) { /* a DBus activate request to the running app */
		atl_window_focus(atl_window);
		return;
	}

	fprintf(stderr, "error: usage: ./android-translation-layer-hotspot [app.apk] --api-impl-jar JAR --natives-dir DIR [-l ACTIVITY]\n"
	                "you can specify --help to see the list of options\n");
	exit(1);
}

static void init_cmd_parameters(GApplication *app, struct launcher_options *d)
{
	const GOptionEntry cmd_params[] = {
		/* clang-format off */
		/* long_name | short_name | flags | arg | arg_data | description | arg_desc */
		{ "launch-activity", 'l', 0, G_OPTION_ARG_STRING,       &d->main_activity_class, "the fully qualified name of the activity to launch (usually the apk's main activity)", "ACTIVITY_NAME" },
		{ "window-width",    'w', 0, G_OPTION_ARG_INT,          &d->window_width,        "window width to launch with",                                                        "WIDTH"         },
		{ "window-height",   'h', 0, G_OPTION_ARG_INT,          &d->window_height,       "window height to launch with",                                                       "HEIGHT"        },
		{ "api-impl-jar",     0,  0, G_OPTION_ARG_FILENAME,     &d->api_impl_jar,        "the framework jar (javac output, not dex); $ATL_API_IMPL_JAR",                       "JAR"           },
		{ "framework-res",    0,  0, G_OPTION_ARG_FILENAME,     &d->framework_res_apk,   "framework-res.apk; $ATL_FRAMEWORK_RES_APK",                                          "APK"           },
		{ "natives-dir",      0,  0, G_OPTION_ARG_FILENAME,     &d->natives_dir,         "directory holding libtranslation_layer_main.so; $ATL_NATIVES_DIR",                   "DIR"           },
		{ "classpath",       'c', 0, G_OPTION_ARG_FILENAME_ARRAY, &d->classpath,         "extra class path entry, after the framework jar and before the apk (repeatable)",     "JAR"           },
		{ "library-path",     0,  0, G_OPTION_ARG_FILENAME_ARRAY, &d->library_path,      "extra java.library.path entry (repeatable)",                                         "DIR"           },
		{ "extra-jvm-option", 'X', 0, G_OPTION_ARG_STRING_ARRAY, &d->extra_jvm_options,  "pass an additional option directly to the JVM (e.g -X \"-Xmx512m\")",                 "\"OPTION\""    },
		{ "sdk-int",          0,  0, G_OPTION_ARG_INT,          &d->sdk_int,             "the SDK level to report as Build.VERSION.SDK_INT",                                    "SDK_INT"       },
		{ "smoke-test",       0,  0, G_OPTION_ARG_INT,          &d->smoke_test,          "boot the runtime and a window but no application, then exit 0 after N seconds",       "SECONDS"       },
		{ "run-class",        0,  0, G_OPTION_ARG_STRING,      &d->run_class,           "run this class's main(String[]) in the app's runtime instead of the application",     "CLASS"         },
		{ "vm-library",       0,  0, G_OPTION_ARG_FILENAME,   &d->vm_library,          "the native-image shared library to create the VM from; $ATL_IMAGE_LIB",               "SO"            },
		{ "vm-check",         0,  0, G_OPTION_ARG_STRING,     &d->vm_check,            "run CLASS.main(String[]) as soon as the VM exists and exit, before any framework setup", "CLASS"       },
		{NULL}
		/* clang-format on */
	};

	g_application_add_main_option_entries(G_APPLICATION(app), cmd_params);
}

int main(int argc, char **argv)
{
	struct launcher_options options = {
		.window_width = 960,
		.window_height = 540,
		.sdk_int = 28,
	};

	GApplication *app = g_application_new("com.example.demo_application",
	                                      G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_OPEN | G_APPLICATION_CAN_OVERRIDE_APP_ID);

	init_cmd_parameters(G_APPLICATION(app), &options);
	g_application_set_option_context_summary(G_APPLICATION(app), "run an android application on a stock JVM, with the framework and the app as jars");

	g_signal_connect(app, "activate", G_CALLBACK(activate), &options);
	g_signal_connect(app, "open", G_CALLBACK(open), &options);
	g_action_map_add_action_entries(G_ACTION_MAP(app), action_entries, action_entries_count, NULL);

	int status = g_application_run(G_APPLICATION(app), argc, argv);
	remove_ongoing_notifications();
	g_object_unref(app);

	if (jvm) {
		JNIEnv *env = get_jni_env();
		jclass system = (*env)->FindClass(env, "java/lang/System");
		(*env)->CallStaticVoidMethod(env, system, _STATIC_METHOD(system, "exit", "(I)V"), status);
	}

	return status;
}
