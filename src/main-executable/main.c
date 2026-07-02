// for dladdr
#define _GNU_SOURCE

#include <gio/gio.h>
#include <GLFW/glfw3.h>

#include "../api-impl-jni/defines.h"
#include "../api-impl-jni/util.h"
#include "../api-impl-jni/app/android_app_Activity.h"

#include "actions.h"
#include "../api-impl-jni/ATLWindow.h"
#include "libc_bio_path_overrides.h"

#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <locale.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/stat.h>

#ifndef DEFFILEMODE
	#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) /* 0666*/
#endif

#ifdef __x86_64__
	#define NATIVE_ARCH "x86_64"
#elifdef __i386__
	#define NATIVE_ARCH "x86"
#elifdef __aarch64__
	#define NATIVE_ARCH "arm64-v8a"
#elifdef __arm__
	#define NATIVE_ARCH "armeabi-v7a"
#else
	#error unknown native architecture
#endif

void *window = NULL; /* legacy: still referenced by libandroid's egl.c */
ATLWindow *atl_window = NULL;
char *apk_path;

// standard GApplication stuff, more or less



// this is the equivalent of /data/data/com.example.app/
char *app_data_dir = NULL;
char *get_app_data_dir()
{
	return app_data_dir;
}

char *construct_classpath(char *prefix, char **cp_array, size_t len)
{
	size_t result_len = strlen(prefix);
	for (int i = 0; i < len; i++) {
		if (cp_array[i])
			result_len += strlen(cp_array[i]) + 1; // the 1 is for either : or the final \0
	}

	char *result = malloc(result_len);
	strcpy(result, prefix);
	for (int i = 0; i < len; i++) {
		if (cp_array[i]) {
			if (i > 0)
				strcat(result, ":");
			strcat(result, cp_array[i]);
		}
	}

	return result;
}

#define JDWP_ARG    "-XjdwpOptions:transport=dt_socket,server=y,suspend=y,address="
#define SDK_INT_ARG "-DBuild.VERSION.SDK_INT="

JNIEnv *create_vm(char *api_impl_jar, char *apk_classpath, char *framework_res_apk, char *test_runner_jar, char *api_impl_natives_dir, char *app_lib_dir, char *sdk_int, char **extra_jvm_options)
{
	JavaVM *jvm;
	JNIEnv *env;
	JavaVMInitArgs args = {
		.version = JNI_VERSION_1_6,
		.nOptions = 3,
	};
	JavaVMOption *options;

	int option_counter = args.nOptions;

	char jdwp_option_string[sizeof(JDWP_ARG) + 5] = JDWP_ARG;          // 5 chars for port number, NULL byte is counted by sizeof
	char sdk_int_option_string[sizeof(SDK_INT_ARG) + 2] = SDK_INT_ARG; // 2 chars for SDK_INT, NULL byte is counted by sizeof

	const char *jdwp_port = getenv("JDWP_LISTEN");

	if (jdwp_port)
		args.nOptions += 2;
	if (sdk_int)
		args.nOptions += 1;
	if (extra_jvm_options)
		args.nOptions += g_strv_length(extra_jvm_options);
	options = malloc(sizeof(JavaVMOption) * args.nOptions);

	if (getenv("RUN_FROM_BUILDDIR")) {
		options[0].optionString = construct_classpath("-Djava.library.path=", (char *[]){"./", app_lib_dir}, 2);
	} else {
		options[0].optionString = construct_classpath("-Djava.library.path=", (char *[]){api_impl_natives_dir, app_lib_dir}, 2);
	}

	options[1].optionString = construct_classpath("-Djava.class.path=", (char *[]){api_impl_jar, apk_classpath, framework_res_apk, test_runner_jar}, 4);
	options[2].optionString = "-Xcheck:jni";
	if (jdwp_port) {
		strncat(jdwp_option_string, jdwp_port, 5); // 5 chars is enough for a port number, and won't overflow our array
		options[option_counter++].optionString = "-XjdwpProvider:internal";
		options[option_counter++].optionString = jdwp_option_string;
	}

	if (sdk_int) {
		strncat(sdk_int_option_string, sdk_int, 2); // 2 chars should be enough for the foreseeable future, and won't overflow our array
		options[option_counter++].optionString = sdk_int_option_string;
	}

	while (extra_jvm_options && *extra_jvm_options) {
		options[option_counter++].optionString = *(extra_jvm_options++);
	}

	args.options = options;
	args.ignoreUnrecognized = JNI_FALSE;

	int ret = JNI_CreateJavaVM(&jvm, (void **)&env, &args);
	if (ret < 0) {
		fprintf(stderr, "Unable to Launch JVM\n");
	} else {
		fprintf(stderr, "JVM launched successfully\n");
	}

	free(options);
	return env;
}


/*
 * There is no way to get a nice clean callback for when the window is ready to be used for stuff
 * that requires non-zero dimensions, so we just check periodically
 */
gboolean hacky_on_window_focus_changed_callback(JNIEnv *env)
{
	if (atl_window_get_width(atl_window) != 0) {
		activity_window_ready();
		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}

static void install_desktop_entry(const char *desktop_file_id, const char *desktop_entry)
{
	char *applications_dir = g_strdup_printf("%s/applications", g_get_user_data_dir());
	g_mkdir_with_parents(applications_dir, 0755);
	char *desktop_file_path = g_strdup_printf("%s/%s", applications_dir, desktop_file_id);
	GError *err = NULL;
	g_file_set_contents(desktop_file_path, desktop_entry, -1, &err);
	if (err) {
		fprintf(stderr, "failed to install desktop entry %s: %s\n", desktop_file_path, err->message);
		exit(1);
	}
	g_free(desktop_file_path);
	// run update-desktop-database to add the new x-scheme-handler entries to ~/.local/share/applications/mimeinfo.cache
	char *update_desktop_database = g_strdup_printf("update-desktop-database %s", applications_dir);
	printf("running: `%s`\n", update_desktop_database);
	system(update_desktop_database);
	g_free(update_desktop_database);
	g_free(applications_dir);

	exit(0);
}

// this is exported by the shim bionic linker
void dl_parse_library_path(const char *path, char *delim);

#define REL_DEX_INSTALL_PATH              "../java/dex"

#define REL_API_IMPL_JAR_INSTALL_PATH     "android_translation_layer/api-impl.jar"
#define REL_API_IMPL_NATIVES_INSTALL_PATH "android_translation_layer/natives"
#define REL_FRAMEWORK_RES_INSTALL_PATH    "android_translation_layer/framework-res.apk"
#define REL_TEST_RUNNER_JAR_INSTALL_PATH  "android_translation_layer/test_runner.jar"

#define API_IMPL_JAR_PATH_LOCAL           "./api-impl.jar"
#define FRAMEWORK_RES_PATH_LOCAL          "./res/framework-res/framework-res.apk"
#define TEST_RUNNER_JAR_PATH_LOCAL        "./test_runner.jar"

struct jni_callback_data {
	char *apk_main_activity_class;
	char *apk_instrumentation_class;
	uint32_t window_width;
	uint32_t window_height;
	gboolean install;
	gboolean install_internal;
	char *prgname;
	char **extra_jvm_options;
	char **extra_string_keys;
	char *sdk_int;
};

static char *uri_option = NULL;

static void parse_string_extras(JNIEnv *env, char **extra_string_keys, jobject intent)
{
	GError *error = NULL;
	GRegex *regex = g_regex_new("(?<!\\\\)=", 0, 0, &error);
	if (!regex) {
		fprintf(stderr, "g_regex_new error: '%s'\n", error->message);
		exit(1);
	}

	for (char **arg = extra_string_keys; *arg; arg++) {
		gchar **keyval = g_regex_split_full(regex, *arg, -1, 0, 0, 2, NULL);
		if (!keyval || !keyval[0] || !keyval[1]) {
			fprintf(stderr, "extra string arg not in 'key=value' format: '%s'\n", *arg);
			exit(1);
		}
		(*env)->CallObjectMethod(env, intent, handle_cache.intent.putExtraCharSequence, _JSTRING(keyval[0]), _JSTRING(keyval[1]));
		g_strfreev(keyval);
	}
	g_regex_unref(regex);
}

/* Drag and drop callback to simulate ACTION_SEND intents */

char *find_jar_or_die(char *builddir_path, char *installed_path, char *install_prefix)
{
	char *path;

	if (getenv("RUN_FROM_BUILDDIR")) {
		path = strdup(builddir_path); // for running out of builddir; using strdup so we can always safely call free on this
	} else {
		path = g_strdup_printf("%s/%s", install_prefix, installed_path);
	}

	if (access(path, F_OK) < 0) {
		fprintf(stderr, "error: can't stat %s (%s)\n",
		        path, strerror(errno));
		exit(1);
	}

	return path;
}

static void open(GApplication *app, GFile **files, gint nfiles, const gchar *hint, struct jni_callback_data *d)
{
	// TODO: pass all files to classpath
	/*
	printf("nfiles: %d\n", nfiles);
	for(int i = 0; i < nfiles; i++) {
		printf(">- [%s]\n", g_file_get_path(files[i]));
	}
*/
	if (atl_window) { // this is not the first launch, but a DBus request to open an URI in the running app
		fprintf(stderr, "opening uri over DBus %p\n", files[0]);
		char *uri = g_file_get_uri(files[0]);
		JNIEnv *env = get_jni_env();
		fprintf(stderr, "opening uri over DBus: %s\n", uri);
		jobject activity = (*env)->CallStaticObjectMethod(env, handle_cache.activity.class,
		                                                  _STATIC_METHOD(handle_cache.activity.class, "createMainActivity", "(Ljava/lang/String;JLjava/lang/String;)Landroid/app/Activity;"),
		                                                  _JSTRING(d->apk_main_activity_class), _INTPTR(atl_window), _JSTRING(uri));
		if ((*env)->ExceptionCheck(env))
			(*env)->ExceptionDescribe(env);
		activity_start(env, activity);
		return;
	}
	char *dex_install_dir;
	char *api_impl_jar;
	char *test_runner_jar = NULL;
	char *framework_res_apk = NULL;
	const char *package_name;
	int ret;
	jobject activity_object;
	jobject application_object;

	char *apk_classpath = g_file_get_path(files[0]);
	char *apk_name = g_file_get_basename(files[0]);

	if (apk_classpath == NULL) {
		fprintf(stderr, "error: the specified file path doesn't seem to be valid\n");
		exit(1);
	}

	if (access(apk_classpath, F_OK) < 0) {
		fprintf(stderr, "error: the specified file path (%s) doesn't seem to exist (%m)\n", apk_classpath);
		exit(1);
	}

	Dl_info libart_so_dl_info;
	// JNI_CreateJavaVM chosen arbitrarily, what matters is that it's a symbol exported by by libart.so
	// TODO: we shouldn't necessarily count on art being installed in the same prefix as we are
	void *_JNI_CreateJavaVM = dlsym(RTLD_NEXT, "JNI_CreateJavaVM");
	dladdr(_JNI_CreateJavaVM, &libart_so_dl_info);
	// make sure we didn't get NULL
	if (libart_so_dl_info.dli_fname) {
		// it's simpler if we can modify the string, so strdup it
		char *libart_so_full_path = strdup(libart_so_dl_info.dli_fname);
		*strrchr(libart_so_full_path, '/') = '\0'; // now we should have something like /usr/lib64/art
		dex_install_dir = g_strdup_printf("%s/%s", libart_so_full_path, REL_DEX_INSTALL_PATH);
		free(libart_so_full_path);
	} else {
		fprintf(stderr, "error: couldn't find art install path\n");
		exit(1);
	}

	char *app_data_dir_base = getenv("ANDROID_APP_DATA_DIR");
	if (!app_data_dir_base) {
		const char *user_data_dir = g_get_user_data_dir();
		if (user_data_dir) {
			app_data_dir_base = g_strdup_printf("%s/android_translation_layer", user_data_dir);
			ret = mkdir(app_data_dir_base, DEFFILEMODE | S_IXUSR | S_IXGRP | S_IXOTH);
			if (ret) {
				if (errno != EEXIST) {
					fprintf(stderr, "error: ANDROID_APP_DATA_DIR not set, and the default directory (%s) couldn't be created (error: %s)\n", app_data_dir_base, strerror(errno));
					exit(1);
				}
			}
		} else {
			fprintf(stderr, "error: ANDROID_APP_DATA_DIR not set, and HOME is not set either so we can't construct the default path\n");
			exit(1);
		}
	}

	// TODO: we should possibly use the app id instead, but we don't currently have a way to get that soon enough
	// arguably both the app id and the apk name might have an issue with duplicates, but if two apks use the same app id, chances are it's less of an issue than when two apks have the same name
	// !IMPORTANT! Unity can't comprehend that a directory name could end in .apk, so we have to avoid that here by adding `_`
	app_data_dir = g_strdup_printf("%s/%s_/", app_data_dir_base, apk_name);

	ret = mkdir(app_data_dir, DEFFILEMODE | S_IXUSR | S_IXGRP | S_IXOTH);
	if (ret && errno != EEXIST) {
		fprintf(stderr, "can't create app data dir %s (%s)\n", app_data_dir, strerror(errno));
		exit(1);
	}

	// check for jars and apks  in './' (if running from builddir), or in system install path
	api_impl_jar = find_jar_or_die(API_IMPL_JAR_PATH_LOCAL, REL_API_IMPL_JAR_INSTALL_PATH, dex_install_dir);
	framework_res_apk = find_jar_or_die(FRAMEWORK_RES_PATH_LOCAL, REL_FRAMEWORK_RES_INSTALL_PATH, dex_install_dir);
	if (d->apk_instrumentation_class)
		test_runner_jar = find_jar_or_die(TEST_RUNNER_JAR_PATH_LOCAL, REL_TEST_RUNNER_JAR_INSTALL_PATH, dex_install_dir);

	char *api_impl_natives_dir = g_strdup_printf("%s/%s", dex_install_dir, REL_API_IMPL_NATIVES_INSTALL_PATH);

	char *app_lib_dir = malloc(strlen(app_data_dir) + strlen("/lib") + 1); // +1 for NULL
	strcpy(app_lib_dir, app_data_dir);
	strcat(app_lib_dir, "/lib");
	// create lib dir
	mkdir(app_lib_dir, DEFFILEMODE | S_IXUSR | S_IXGRP | S_IXOTH);

	// Apps which extract libraries on their own can place them anywhere in app_data_dir. Therefore, we add app_data_dir/** to the
	// BIONIC_LD_LIBRARY_PATH. While app_data_dir/lib is already matched by the wildcard, it needs to be specified again to allow loading
	// libraries by libname from app_data_dir/lib
	char *ld_path = g_strdup_printf("%s:%s**", app_lib_dir, app_data_dir);
	// calling directly into the shim bionic linker to whitelist the app's lib dir as containing bionic-linked libraries
	dl_parse_library_path(ld_path, ":");
	g_free(ld_path);

	JNIEnv *env = create_vm(api_impl_jar, apk_classpath, framework_res_apk, test_runner_jar, api_impl_natives_dir, app_lib_dir, d->sdk_int, d->extra_jvm_options);

	free(app_lib_dir);

	jclass display_class = (*env)->FindClass(env, "android/view/Display");
	_SET_STATIC_INT_FIELD(display_class, "window_width", d->window_width);
	_SET_STATIC_INT_FIELD(display_class, "window_height", d->window_height);

	/* -- register our JNI library under the appropriate classloader -- */

	/* 'android/view/View' is part of the "hax.dex" package, any other function from that package would serve just as well */
	jmethodID getClassLoader = _METHOD((*env)->FindClass(env, "java/lang/Class"), "getClassLoader", "()Ljava/lang/ClassLoader;");
	jobject class_loader = (*env)->CallObjectMethod(env, (*env)->FindClass(env, "android/view/View"), getClassLoader);

	jclass java_runtime_class = (*env)->FindClass(env, "java/lang/Runtime");

	jmethodID getRuntime = _STATIC_METHOD(java_runtime_class, "getRuntime", "()Ljava/lang/Runtime;");
	jobject java_runtime = (*env)->CallStaticObjectMethod(env, java_runtime_class, getRuntime);

	/* this method is private, but it seems we get away with calling it from C */
	jmethodID loadLibrary_with_classloader = _METHOD(java_runtime_class, "loadLibrary", "(Ljava/lang/String;Ljava/lang/ClassLoader;)V");
	(*env)->CallVoidMethod(env, java_runtime, loadLibrary_with_classloader, _JSTRING("translation_layer_main"), class_loader);

	// some apps need the apk path since they directly read their apk
	apk_path = strdup(apk_classpath);

	(*env)->GetJavaVM(env, &jvm);
	set_up_handle_cache(env);

	/* -- misc -- */

	const char *disable_decoration_env = getenv("ATL_DISABLE_WINDOW_DECORATIONS");
	bool decorated = true;
	if (disable_decoration_env)
		decorated = !strcmp(disable_decoration_env, "0") || !strcmp(disable_decoration_env, "false");

	atl_windows_init();
	atl_window = atl_window_new(d->window_width ? d->window_width : 540,
	                            d->window_height ? d->window_height : 960,
	                            true, decorated);

	/* Our windows are GLFW windows, so the GApplication has no windows of its
	 * own and would auto-quit as soon as `activate`/`open` returns, making
	 * g_application_run() return and the app exit immediately. Hold the
	 * application open for our lifetime; we exit explicitly when the last
	 * activity/window goes away. */
	g_application_hold(app);

	prepare_main_looper(env);

	/* extract native libraries from apk*/
	if (!getenv("ATL_SKIP_NATIVES_EXTRACTION"))
		extract_from_apk("lib/" NATIVE_ARCH "/", "lib/");

	// construct Application
	application_object = (*env)->CallStaticObjectMethod(env, handle_cache.context.class,
	                                                    _STATIC_METHOD(handle_cache.context.class, "createApplication", "(J)Landroid/app/Application;"), _INTPTR(atl_window));
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);

	jclass content_provider = (*env)->FindClass(env, "android/content/ContentProvider");
	(*env)->CallStaticVoidMethod(env, content_provider, _STATIC_METHOD(content_provider, "createContentProviders", "()V"));
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);

	jmethodID on_create_method = _METHOD(handle_cache.application.class, "onCreate", "()V");
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	(*env)->CallVoidMethod(env, application_object, on_create_method);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);

	if (d->apk_instrumentation_class) {
		if (d->apk_main_activity_class) {
			fprintf(stderr, "error: both --instrument and --launch-activity supplied, exiting\n");
			exit(1);
		}

		jobject intent = NULL;

		if (d->extra_string_keys) {
			intent = (*env)->NewObject(env, handle_cache.intent.class, handle_cache.intent.constructor);
			parse_string_extras(env, d->extra_string_keys, intent);
		}

		(*env)->CallStaticObjectMethod(env, handle_cache.instrumentation.class,
		                               _STATIC_METHOD(handle_cache.instrumentation.class, "create", "(Ljava/lang/String;Landroid/content/Intent;)Landroid/app/Instrumentation;"),
		                               _JSTRING(d->apk_instrumentation_class), intent);

		if ((*env)->ExceptionCheck(env))
			(*env)->ExceptionDescribe(env);
	}

	// construct main Activity
	if (!d->apk_instrumentation_class && !d->install_internal) {
		activity_object = (*env)->CallStaticObjectMethod(env, handle_cache.activity.class,
		                                                 _STATIC_METHOD(handle_cache.activity.class, "createMainActivity", "(Ljava/lang/String;JLjava/lang/String;)Landroid/app/Activity;"),
		                                                 _JSTRING(d->apk_main_activity_class), _INTPTR(atl_window), (uri_option && *uri_option) ? _JSTRING(uri_option) : NULL);
		if ((*env)->ExceptionCheck(env))
			(*env)->ExceptionDescribe(env);
		if (uri_option)
			g_free(uri_option);

		if (d->extra_string_keys) {
			jobject intent = _GET_OBJ_FIELD(activity_object, "intent", "Landroid/content/Intent;");
			parse_string_extras(env, d->extra_string_keys, intent);
			g_strfreev(d->extra_string_keys);
		}
	}
	/* -- set the window title and app icon -- */

	if (!d->apk_instrumentation_class) {
		jstring package_name_jstr = (*env)->CallObjectMethod(env, application_object, handle_cache.context.get_package_name);
		package_name = package_name_jstr ? _CSTRING(package_name_jstr) : NULL;
		if ((*env)->ExceptionCheck(env))
			(*env)->ExceptionDescribe(env);
	}

	jstring app_icon_path_jstr = (*env)->CallObjectMethod(env, application_object, handle_cache.application.get_app_icon_path);
	const char *app_icon_path = app_icon_path_jstr ? _CSTRING(app_icon_path_jstr) : NULL;
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);

	if (d->install || d->install_internal) {
		if (d->apk_instrumentation_class) {
			fprintf(stderr, "error: --instrument supplied together with --install, exiting\n");
			exit(1);
		}

		const char *app_label = _CSTRING((*env)->CallObjectMethod(env, application_object, _METHOD(handle_cache.application.class, "get_app_label", "()Ljava/lang/String;")));
		if ((*env)->ExceptionCheck(env))
			(*env)->ExceptionDescribe(env);

		char *icon_path_installed = NULL;
		if (!d->install_internal) {
			if (app_icon_path) {
			/* we can reference the extracted icon file directly */
				extract_from_apk(app_icon_path, app_icon_path);
				icon_path_installed = g_strdup_printf("%s%s", app_data_dir, app_icon_path);
			}
			/* TODO: icons that only exist as a generalized Drawable (e.g. VectorDrawable)
			 * used to be rendered to SVG through GTK/GSK; that path was dropped together
			 * with libportal. Render through Skia instead. */
		}

		gchar *dest_name = g_strdup_printf("%s.apk", package_name);
		GFile *dest = g_file_new_build_filename(app_data_dir_base, "_installed_apks_", d->install_internal ? dest_name : apk_name, NULL);
		free(dest_name);
		printf("installing %s to %s\n", apk_name, g_file_get_path(dest));
		g_file_make_directory(g_file_get_parent(dest), NULL, NULL);
		GError *err = NULL;
		g_file_copy(files[0], dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err);
		if (err)
			fprintf(stderr, "error copying apk: %s\n", err->message);

		if (d->install_internal)
			exit(0);

		jmethodID get_supported_mime_types = _METHOD(handle_cache.application.class, "get_supported_mime_types", "()Ljava/lang/String;");
		jstring supported_mime_types_jstr = (*env)->CallObjectMethod(env, application_object, get_supported_mime_types);
		const char *supported_mime_types = supported_mime_types_jstr ? _CSTRING(supported_mime_types_jstr) : NULL;
		if ((*env)->ExceptionCheck(env))
			(*env)->ExceptionDescribe(env);

		GString *desktop_entry = g_string_new("[Desktop Entry]\n"
		                                      "Type=Application\n"
		                                      "DBusActivatable=true\n");
		g_string_append_printf(desktop_entry, "Name=%s\n", app_label);
		if (icon_path_installed) {
			g_string_append_printf(desktop_entry, "Icon=%s\n", icon_path_installed);
			g_free(icon_path_installed);
		}
		g_string_append(desktop_entry, "Exec=env ");
		if (getenv("RUN_FROM_BUILDDIR")) {
			printf("WARNING: RUN_FROM_BUILDDIR set and --install given: using current directory in desktop entry\n");
			g_string_append_printf(desktop_entry, "-C %s ", g_get_current_dir());
		}
		char *envs[] = {"RUN_FROM_BUILDDIR", "LD_LIBRARY_PATH", "ANDROID_APP_DATA_DIR", "ATL_UGLY_ENABLE_LOCATION", "ATL_UGLY_ENABLE_MICROPHONE", "ATL_UGLY_ENABLE_WEBVIEW", "ATL_DISABLE_WINDOW_DECORATIONS", "ATL_FORCE_FULLSCREEN", "ATL_IS_AUTOMOTIVE", "ATL_IS_TELEVISION", "ATL_IS_WATCH"};
		for (int i = 0; i < ARRAY_SIZE(envs); i++) {
			if (getenv(envs[i])) {
				g_string_append_printf(desktop_entry, "%s=%s ", envs[i], getenv(envs[i]));
			}
		}
		g_string_append_printf(desktop_entry, "%s ", d->prgname);
		g_string_append_printf(desktop_entry, "--gapplication-app-id %s ", package_name);
		if (d->apk_main_activity_class)
			g_string_append_printf(desktop_entry, "-l %s ", d->apk_main_activity_class);
		if (d->window_width)
			g_string_append_printf(desktop_entry, "-w %d ", d->window_width);
		if (d->window_height)
			g_string_append_printf(desktop_entry, "-h %d ", d->window_height);
		g_string_append_printf(desktop_entry, "%s --uri %%u\n", g_file_get_path(dest));
		if (supported_mime_types)
			g_string_append_printf(desktop_entry, "MimeType=%s\n", supported_mime_types);
		char *desktop_file_id = g_strdup_printf("%s.desktop", package_name);
		char *desktop_entry_str = g_string_free(desktop_entry, FALSE);
		printf("installing %s\n\n%s\n", desktop_file_id, desktop_entry_str);
		install_desktop_entry(desktop_file_id, desktop_entry_str); // exits
		return;
	}

	if (!d->apk_instrumentation_class)
		atl_window_set_title(atl_window, package_name);

	const GLFWvidmode *monitor_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	jobject resources = _GET_STATIC_OBJ_FIELD(handle_cache.context.class, "r", "Landroid/content/res/Resources;");
	jobject configuration = _GET_OBJ_FIELD(resources, "mConfiguration", "Landroid/content/res/Configuration;");
	if (monitor_mode && monitor_mode->width >= 800 && monitor_mode->height >= 800)
		_SET_INT_FIELD(configuration, "screenLayout", /*SCREENLAYOUT_SIZE_LARGE*/ 0x03);
	else
		_SET_INT_FIELD(configuration, "screenLayout", /*SCREENLAYOUT_SIZE_NORMAL*/ 0x02);

	// TODO: window icon (GLFW only supports this on X11; Wayland needs an app-id + .desktop file)

	if (!d->apk_instrumentation_class) {
		activity_start(env, activity_object);

		g_timeout_add(10, G_SOURCE_FUNC(hacky_on_window_focus_changed_callback), env);
	}


	const char *app_id = g_application_get_application_id(app);
	if (strcmp(app_id, "com.example.demo_application")) {
		// This would normally happen automatically, if the GApplication is not contructed with G_APPLICATION_NON_UNIQUE
		g_dbus_connection_call(g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
		                       "org.freedesktop.DBus",
		                       "/org/freedesktop/DBus",
		                       "org.freedesktop.DBus",
		                       "RequestName",
		                       g_variant_new("(su)", app_id, G_BUS_NAME_OWNER_FLAGS_NONE),
		                       G_VARIANT_TYPE("(u)"),
		                       0, -1, NULL, NULL, NULL);
	}
}

static void activate(GApplication *app, struct jni_callback_data *d)
{
	if (atl_window) { // this is not the first launch, but a DBus activate request to the running app
		atl_window_focus(atl_window);
		return;
	}
	fprintf(stderr, "error: usage: ./android-translation-layer [app.apk] [-l path/to/activity]\n"
	                "you can specify --help to see the list of options\n");
	exit(1);
}

static gboolean option_uri_cb(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
	fprintf(stderr, "option_uri_cb: %s %s, %p, %p\n", option_name, value, data, error);
	uri_option = g_strdup(value);
	return TRUE;
}

void init_cmd_parameters(GApplication *app, struct jni_callback_data *d)
{
	const GOptionEntry cmd_params[] = {
		/* clang-format off */
		/* long_name | short_name | flags | arg                 | arg_data                     | description                                                                                   | arg_desc */
		{ "launch-activity",  'l', 0, G_OPTION_ARG_STRING,       &d->apk_main_activity_class,   "the fully qualifed name of the activity you wish to launch (usually the apk's main activity)", "ACTIVITY_NAME" },
		{ "instrument",        0,  0, G_OPTION_ARG_STRING,       &d->apk_instrumentation_class, "the fully qualifed name of the instrumentation you wish to launch",                            "CLASS_NAME"    },
		{ "window-width",     'w', 0, G_OPTION_ARG_INT,          &d->window_width,              "window width to launch with (some apps react poorly to runtime window size adjustments)",      "WIDTH"         },
		{ "window-height",    'h', 0, G_OPTION_ARG_INT,          &d->window_height,             "window height to launch with (some apps react poorly to runtime window size adjustments)",     "HEIGHT"        },
		{ "install",          'i', 0, G_OPTION_ARG_NONE,         &d->install,                   "install .desktop file for the given apk",                                                      NULL            },
		{ "install-internal",  0 , 0, G_OPTION_ARG_NONE,         &d->install_internal,          "copy an apk to _installed_apks_ but don't create a desktop entry",                             NULL            },
		{ "extra-jvm-option", 'X', 0, G_OPTION_ARG_STRING_ARRAY, &d->extra_jvm_options,         "pass an additional option directly to art (e.g -X \"-verbose:jni\")",                          "\"OPTION\""    },
		{ "extra-string-key", 'e', 0, G_OPTION_ARG_STRING_ARRAY, &d->extra_string_keys,         "pass a string extra (-e key=value)",                                                           "\"KEY=VALUE\"" },
		{ "sdk-int",           0 , 0, G_OPTION_ARG_STRING,       &d->sdk_int,                   "shorthand for -X \"-DBuild.VERSION.SDK_INT=<version>\"",                                                           "SDK_INT" },
		/* long_name | short_name | flags                     | arg                  | arg_data     | description                                                                              | arg_desc */
		{ "uri",              'u', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, option_uri_cb, "open the given URI inside the application",                                               "URI"           },
		{NULL}
		/* clang-format on */
	};

	g_application_add_main_option_entries(G_APPLICATION(app), cmd_params);
}

void init__r_debug();
void remove_ongoing_notifications();

typedef bool(apply_path_overrides_func_type)(char **);
void libc_bio_set_apply_path_overrides_func(apply_path_overrides_func_type *func);

/*
 * The main thread's stack is initialized to 128KiB by Linux.
 * When the stack grows below the allocated mapping, the kernel
 * automagically maps in more memory (as long as there is no existing
 * mapping in the way).
 * However, ART puts in a guard page of it's own in order to catch stack
 * overflows (and if they happen in Java code, it handles them gracefully).
 * When ART uses pthread_getattr_np to get the main thread's stack size,
 * glibc reports RLIMIT_STACK (minus TLS and stuff), while musl reports
 * the current stack size. Therefore on musl we have to pre-grow the stack
 * to make the kernel actually grow the mapping before ART puts in it's guard
 * page and makes further growth impossible.
 */
static void pregrow_stack()
{
	/* set RLIMIT_STACK to 8 MiB, which should fit both our 6 MiB stack and whatever
	 * the libc stores above the top of stack */
	setrlimit(RLIMIT_STACK, &(struct rlimit){8 * MiB, 8 * MiB});
	/* accessing the first element of this array will grow the stack down 6MiB */
	volatile uint8_t dummy[6 * MiB];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
	/* noop, but should generate an access because volatile */
	dummy[0] = dummy[0];
#pragma GCC diagnostic pop
}

int main(int argc, char **argv)
{
	GApplication *app;
	int status;

	pregrow_stack();

	/* this has to be done in the main executable, so might as well do it here */
	init__r_debug();

	// locale on android is always either C or C.UTF-8, and some apps might unbeknownst to them depend on that
	// for correct functionality
	setenv("LC_ALL", "C.UTF-8", 1);

	libc_bio_set_apply_path_overrides_func(apply_path_overrides);

	struct jni_callback_data *callback_data = malloc(sizeof(struct jni_callback_data));
	callback_data->apk_main_activity_class = NULL;
	callback_data->apk_instrumentation_class = NULL;
	callback_data->window_width = 960;
	callback_data->window_height = 540;
	callback_data->install = FALSE;
	callback_data->install_internal = FALSE;
	callback_data->prgname = argv[0];
	callback_data->extra_jvm_options = NULL;
	callback_data->extra_string_keys = NULL;
	callback_data->sdk_int = NULL;

	app = g_application_new("com.example.demo_application", G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_OPEN | G_APPLICATION_CAN_OVERRIDE_APP_ID);

	// cmdline related setup
	init_cmd_parameters(G_APPLICATION(app), callback_data);
	g_application_set_option_context_summary(G_APPLICATION(app), "a translation layer for running android applications natively on Linux");

	g_signal_connect(app, "activate", G_CALLBACK(activate), callback_data);
	g_signal_connect(app, "open", G_CALLBACK(open), callback_data);
	g_action_map_add_action_entries(G_ACTION_MAP(app), action_entries, action_entries_count, NULL);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	remove_ongoing_notifications();
	g_object_unref(app);

	if (jvm) {
		JNIEnv *env = get_jni_env();
		jobject system = (*env)->FindClass(env, "java/lang/System");
		jmethodID exit = (*env)->GetStaticMethodID(env, system, "exit", "(I)V");
		(*env)->CallStaticVoidMethod(env, system, exit, status);
	}

	return status;
}

/* TODO: recall what this is doing here */
const char dl_loader[] __attribute__((section(".interp"))) = "/lib/ld-linux.so.2";
