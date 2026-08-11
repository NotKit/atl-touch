/*
 * Everything both VM backends need: option assembly that is not specific to
 * HotSpot or to a native image, and the read-back that proves the properties
 * actually arrived in Java.
 */

#define _GNU_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include "vm.h"

void add_option(GPtrArray *options, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	g_ptr_array_add(options, g_strdup_vprintf(format, args));
	va_end(args);
}

void append_path(GString *joined, const char *entry)
{
	if (!entry)
		return;
	if (joined->len)
		g_string_append_c(joined, ':');
	g_string_append(joined, entry);
}

void append_path_list(GString *joined, char **list)
{
	for (char **entry = list; list && *entry; entry++)
		append_path(joined, *entry);
}

/*
 * The JVM's default locale comes from the environment, but Android's C locale
 * is always C/C.UTF-8 and native code may depend on that (decimal points in
 * strtod, and so on). Keep both: force the C locale, but hand the user's
 * language and region to the JVM as properties.
 */
void locale_options(GPtrArray *options)
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

/* the -D keys the launcher assembles itself; a caller's -X -D is not one of
 * them, and the VM is free to normalise or refuse those (HotSpot does) */
static GPtrArray *own_property_keys;

static char *property_key(const char *option) /* option starts with -D */
{
	const char *assign = strchr(option + 2, '=');
	return assign ? g_strndup(option + 2, assign - (option + 2)) : g_strdup(option + 2);
}

static gboolean is_own_property(const char *key)
{
	for (guint i = 0; own_property_keys && i < own_property_keys->len; i++)
		if (!strcmp(key, (const char *)g_ptr_array_index(own_property_keys, i)))
			return TRUE;
	return FALSE;
}

GPtrArray *vm_common_properties(struct launcher_options *d, const char *apk, const char *app_lib_dir)
{
	GPtrArray *options = g_ptr_array_new_with_free_func(g_free);

	GString *library_path = g_string_new(NULL);
	append_path_list(library_path, d->library_path);
	append_path(library_path, d->natives_dir);
	append_path(library_path, app_lib_dir);

	add_option(options, "-Djava.library.path=%s", library_path->str);
	add_option(options, "-DBuild.VERSION.SDK_INT=%d", d->sdk_int);
	/* what the ART launcher keeps in C globals; a jar-based runtime wants them from Java too */
	add_option(options, "-Datl.apk.path=%s", apk);
	if (d->framework_res_apk)
		add_option(options, "-Datl.framework.res.path=%s", d->framework_res_apk);
	add_option(options, "-Datl.data.dir=%s", get_app_data_dir());
	add_option(options, "-Datl.app.id=%s", g_application_get_application_id(g_application_get_default()));

	locale_options(options);

	/* recorded from the array itself, so a new property added above cannot be
	 * left out of the check below */
	g_clear_pointer(&own_property_keys, g_ptr_array_unref);
	own_property_keys = g_ptr_array_new_with_free_func(g_free);
	for (guint i = 0; i < options->len; i++) {
		const char *option = g_ptr_array_index(options, i);
		if (!strncmp(option, "-D", 2))
			g_ptr_array_add(own_property_keys, property_key(option));
	}

	g_string_free(library_path, TRUE);
	return options;
}

void vm_options_dump(GPtrArray *options)
{
	for (guint i = 0; i < options->len; i++)
		fprintf(stderr, "jvm option: %s\n", (const char *)g_ptr_array_index(options, i));
}

/* the caller reports its own miss, so nothing here may print the
 * "error: exception during" prefix a driving script treats as a failed run */
static void clear_pending_exception(JNIEnv *env)
{
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionClear(env);
}

static char *get_property(JNIEnv *env, jclass system, jmethodID get, const char *key)
{
	jstring key_str = (*env)->NewStringUTF(env, key);
	jstring value_str = (*env)->CallStaticObjectMethod(env, system, get, key_str);
	if ((*env)->ExceptionCheck(env)) {
		clear_pending_exception(env);
		(*env)->DeleteLocalRef(env, key_str);
		return NULL;
	}
	(*env)->DeleteLocalRef(env, key_str);
	if (!value_str)
		return NULL;

	const char *chars = (*env)->GetStringUTFChars(env, value_str, NULL);
	char *value = g_strdup(chars ?: "");
	(*env)->ReleaseStringUTFChars(env, value_str, chars);
	(*env)->DeleteLocalRef(env, value_str);
	return value;
}

/*
 * Reads back every -Dkey=value the VM was created with, and only reads: a VM
 * that normalised or refused a value is reporting something, and writing the
 * command line back through System.setProperty would hide it. It matters on the
 * image backend, where "JNI_CreateJavaVM accepted the option" and "Java can read
 * the property" are two different questions, and it puts java.lang.System into
 * jni-config.json when traced on the HotSpot path.
 *
 * A miss is fatal only for the properties the launcher assembles itself: the
 * framework cannot boot without them. The caller's -X -Dkey=value is the
 * caller's business -- HotSpot rewrites sun.jnu.encoding, for one -- so that one
 * is reported and left alone.
 */
void vm_verify_properties(JNIEnv *env, GPtrArray *options)
{
	jclass system = (*env)->FindClass(env, "java/lang/System");
	if (!system || (*env)->ExceptionCheck(env)) {
		clear_pending_exception(env);
		fprintf(stderr, "error: cannot look up java.lang.System to verify the VM properties\n");
		exit(1);
	}
	jmethodID get = (*env)->GetStaticMethodID(env, system, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
	if (!get || (*env)->ExceptionCheck(env)) {
		clear_pending_exception(env);
		fprintf(stderr, "error: java.lang.System has no getProperty(String) here\n");
		exit(1);
	}

	for (guint i = 0; i < options->len; i++) {
		const char *option = g_ptr_array_index(options, i);
		if (strncmp(option, "-D", 2))
			continue;

		const char *assign = strchr(option + 2, '=');
		char *key = property_key(option);
		const char *want = assign ? assign + 1 : "";

		char *have = get_property(env, system, get, key);

		if (have && !strcmp(have, want)) {
			fprintf(stderr, "vm property: %s=%s (from -D)\n", key, have);
		} else if (!is_own_property(key)) {
			fprintf(stderr, "vm property: %s=%s (the VM's own value, -D asked for %s)\n",
			        key, have ?: "null", want);
		} else {
			fprintf(stderr, "error: the VM will not carry %s: it reads back as %s\n",
			        option, have ?: "null");
			g_free(have);
			g_free(key);
			exit(1);
		}

		g_free(have);
		g_free(key);
	}
}
