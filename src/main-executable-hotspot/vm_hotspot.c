/*
 * The libjvm.so backend: a stock JVM, linked against, with the framework and
 * the app handed to it as a class path.
 *
 * This is the launcher's original create_vm(), moved out of main.c unchanged
 * when the image backend arrived. Everything JVM-only lives here:
 * -Djava.class.path, --add-opens, -Xcheck:jni and -agentlib:jdwp.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gio/gio.h>

#include "vm.h"

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

void vm_validate_options(struct launcher_options *d)
{
	/* first, so a command meant for the other launcher says so rather than
	 * complaining about a class path the image would not have wanted anyway */
	if (d->vm_library) {
		fprintf(stderr, "error: --vm-library belongs to android-translation-layer-image;"
		                " this launcher links libjvm\n");
		exit(1);
	}

	const char *jar = d->api_impl_jar ?: getenv("ATL_API_IMPL_JAR");
	if (!jar) {
		fprintf(stderr, "error: --api-impl-jar is required (the framework jar, javac output rather than dex)\n");
		exit(1);
	}
	if (access(jar, F_OK) < 0) {
		fprintf(stderr, "error: can't stat %s (%s)\n", jar, strerror(errno));
		exit(1);
	}
	d->api_impl_jar = g_strdup(jar);
}

JNIEnv *create_vm(struct launcher_options *d, const char *apk, const char *app_lib_dir)
{
	JavaVM *vm;
	JNIEnv *env;
	GPtrArray *options = vm_common_properties(d, apk, app_lib_dir);

	GString *class_path = g_string_new(NULL);
	append_path(class_path, d->api_impl_jar);
	append_class_path_list(class_path, d->classpath);
	append_path(class_path, apk);
	append_path(class_path, d->framework_res_apk);

	/* the class path comes first in the echo, the way it always did */
	g_ptr_array_insert(options, 0, g_strdup_printf("-Djava.class.path=%s", class_path->str));

	/* the framework reads java.io.FileDescriptor's private fd where a JVM has no libcore equivalent */
	g_ptr_array_add(options, g_strdup("--add-opens=java.base/java.io=ALL-UNNAMED"));

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
	for (guint i = 0; i < options->len; i++)
		vm_options[i].optionString = g_ptr_array_index(options, i);
	vm_options_dump(options);

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

	vm_verify_properties(env, options);

	g_free(vm_options);
	g_ptr_array_free(options, TRUE);
	g_string_free(class_path, TRUE);

	return env;
}
