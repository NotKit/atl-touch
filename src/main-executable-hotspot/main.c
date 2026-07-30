/*
 * Launcher for running an app on a stock JVM (HotSpot) instead of ART.
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

struct launcher_options {
	char *main_activity_class;
	char *api_impl_jar;
	char *framework_res_apk;
	char *natives_dir;
	char **classpath;
	char **library_path;
	char **extra_jvm_options;
	int window_width;
	int window_height;
	int sdk_int;
	int smoke_test;
	char *run_class;
};

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

static void add_option(GPtrArray *options, const char *format, ...) G_GNUC_PRINTF(2, 3);
static void add_option(GPtrArray *options, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	g_ptr_array_add(options, g_strdup_vprintf(format, args));
	va_end(args);
}

static void append_path(GString *joined, const char *entry)
{
	if (!entry)
		return;
	if (joined->len)
		g_string_append_c(joined, ':');
	g_string_append(joined, entry);
}

static void append_path_list(GString *joined, char **list)
{
	for (char **entry = list; list && *entry; entry++)
		append_path(joined, *entry);
}

/*
 * The `java` command expands a "dir/*" class path entry into the jars in that
 * directory before the VM ever sees it; JNI_CreateJavaVM does not, and would
 * silently put a directory named "*" on the class path instead. An app's
 * runtime dependencies are a directory full of jars, so do it here.
 */
static void append_class_path(GString *joined, const char *entry)
{
	size_t len = strlen(entry);
	if (len < 2 || strcmp(entry + len - 2, "/*")) {
		append_path(joined, entry);
		return;
	}

	char *dir_path = g_strndup(entry, len - 2);
	GDir *dir = g_dir_open(dir_path, 0, NULL);
	if (!dir) {
		fprintf(stderr, "warning: class path entry %s: %s is not a directory\n", entry, dir_path);
		g_free(dir_path);
		return;
	}

	/* the order jars come back in is unspecified for `java` too */
	const char *name;
	while ((name = g_dir_read_name(dir))) {
		if (!g_str_has_suffix(name, ".jar") && !g_str_has_suffix(name, ".JAR"))
			continue;
		char *jar = g_build_filename(dir_path, name, NULL);
		append_path(joined, jar);
		g_free(jar);
	}

	g_dir_close(dir);
	g_free(dir_path);
}

/* class path entries are colon-separated like everywhere else in Java */
static void append_class_path_list(GString *joined, char **list)
{
	for (char **entry = list; list && *entry; entry++) {
		char **split = g_strsplit(*entry, ":", -1);
		for (char **part = split; *part; part++)
			if (**part)
				append_class_path(joined, *part);
		g_strfreev(split);
	}
}

/*
 * The JVM's default locale comes from the environment, but Android's C locale
 * is always C/C.UTF-8 and native code may depend on that (decimal points in
 * strtod, and so on). Keep both: force the C locale, but hand the user's
 * language and region to the JVM as properties.
 */
static void locale_options(GPtrArray *options)
{
	const char *locale = getenv("LC_ALL");
	if (!locale || !*locale)
		locale = getenv("LC_MESSAGES");
	if (!locale || !*locale)
		locale = getenv("LANG");

	if (locale && *locale && strcmp(locale, "C") && strncmp(locale, "C.", 2) && strcmp(locale, "POSIX")) {
		char language[3] = {0};
		char country[3] = {0};
		if (sscanf(locale, "%2[a-z]_%2[A-Z]", language, country) >= 1) {
			add_option(options, "-Duser.language=%s", language);
			if (*country)
				add_option(options, "-Duser.country=%s", country);
		}
	}

	setenv("LC_ALL", "C.UTF-8", 1);
}

static JNIEnv *create_vm(struct launcher_options *d, const char *apk, const char *app_lib_dir)
{
	JavaVM *vm;
	JNIEnv *env;
	GPtrArray *options = g_ptr_array_new_with_free_func(g_free);

	GString *class_path = g_string_new(NULL);
	append_path(class_path, d->api_impl_jar);
	append_class_path_list(class_path, d->classpath);
	append_path(class_path, apk);
	append_path(class_path, d->framework_res_apk);

	GString *library_path = g_string_new(NULL);
	append_path_list(library_path, d->library_path);
	append_path(library_path, d->natives_dir);
	append_path(library_path, app_lib_dir);

	add_option(options, "-Djava.class.path=%s", class_path->str);
	add_option(options, "-Djava.library.path=%s", library_path->str);
	add_option(options, "-DBuild.VERSION.SDK_INT=%d", d->sdk_int);
	/* what the ART launcher keeps in C globals; a jar-based runtime wants them from Java too */
	add_option(options, "-Datl.apk.path=%s", apk);
	add_option(options, "-Datl.data.dir=%s", app_data_dir);
	add_option(options, "-Datl.app.id=%s", g_application_get_application_id(g_application_get_default()));
	/* the framework reads java.io.FileDescriptor's private fd where a JVM has no libcore equivalent */
	g_ptr_array_add(options, g_strdup("--add-opens=java.base/java.io=ALL-UNNAMED"));

	locale_options(options);

	/* CheckJNI validates every JNI transition; with the AOSP graphics stack a
	 * single frame makes tens of thousands of them, so keep it opt-in */
	if (getenv("ATL_CHECK_JNI"))
		g_ptr_array_add(options, g_strdup("-Xcheck:jni"));

	const char *jdwp_port = getenv("JDWP_LISTEN");
	if (jdwp_port)
		add_option(options, "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=%s", jdwp_port);

	for (char **extra = d->extra_jvm_options; extra && *extra; extra++)
		g_ptr_array_add(options, g_strdup(*extra));

	JavaVMOption *vm_options = g_new0(JavaVMOption, options->len);
	for (guint i = 0; i < options->len; i++) {
		vm_options[i].optionString = g_ptr_array_index(options, i);
		fprintf(stderr, "jvm option: %s\n", vm_options[i].optionString);
	}

	JavaVMInitArgs args = {
		.version = JNI_VERSION_1_8,
		.nOptions = options->len,
		.options = vm_options,
		.ignoreUnrecognized = JNI_FALSE,
	};

	int ret = JNI_CreateJavaVM(&vm, (void **)&env, &args);
	if (ret < 0) {
		fprintf(stderr, "error: unable to launch the JVM (JNI_CreateJavaVM returned %d)\n", ret);
		exit(1);
	}
	fprintf(stderr, "JVM launched successfully\n");

	g_free(vm_options);
	g_ptr_array_free(options, TRUE);
	g_string_free(class_path, TRUE);
	g_string_free(library_path, TRUE);

	return env;
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
		fprintf(stderr, "error: the framework jar (--api-impl-jar) does not look like a jar this launcher can use\n");
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

	jobjectArray no_args = (*env)->NewObjectArray(env, 0, (*env)->FindClass(env, "java/lang/String"), NULL);
	(*env)->CallStaticVoidMethod(env, class, main_method, no_args);
	if (exception_check(env, "running the --run-class class"))
		exit(1);

	fprintf(stderr, "run-class: %s.main() returned\n", class_name);
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

	d->api_impl_jar = required_path(d->api_impl_jar ?: getenv("ATL_API_IMPL_JAR"),
	                                "--api-impl-jar", "the framework jar, javac output rather than dex");
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

	jclass display_class = (*env)->FindClass(env, "android/view/Display");
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
