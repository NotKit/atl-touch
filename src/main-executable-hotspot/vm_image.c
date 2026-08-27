/*
 * The GraalVM native-image backend: the VM comes from a shared library that was
 * ahead-of-time compiled from the same jars the HotSpot launcher would put on
 * its class path, and exports the JNI Invocation API.
 *
 * The image is dlopened rather than linked against, for four reasons: the image
 * is built *from* atlas's own api-impl.jar, so a link would make atlas depend on
 * one of its own products; the image is rebuilt far more often than the
 * launcher; the launcher has to be testable against a stub image before a real
 * one exists; and it keeps libjvm.so and every image out of the executable's
 * ldd output, which is what "no VM is baked into the launcher" means in
 * practice.
 *
 * The image has no class path, so none is assembled here: no
 * -Djava.class.path, no -agentlib:*, no -XX:*, no -Xcheck:jni and no
 * --add-opens (on an image that is a native-image *build* option -- passing it
 * at VM creation is accepted and does nothing). What is left is exactly the
 * -D properties the framework reads, which do reach the image and do so before
 * any class initialiser runs.
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include "vm.h"

#define DEFAULT_IMAGE_LIBRARY "libmercurygram.so"

typedef jint (*create_java_vm_fn)(JavaVM **vm, void **env, void *args);

void vm_validate_options(struct launcher_options *d)
{
	/* --api-impl-jar and --classpath describe a class path the image does not
	 * have. They are warned about rather than refused, so one argv can drive
	 * both launchers. */
	if (d->api_impl_jar || getenv("ATL_API_IMPL_JAR"))
		fprintf(stderr, "image: ignoring --api-impl-jar; the image already contains the framework\n");
	if (d->classpath)
		fprintf(stderr, "image: ignoring --classpath; the image has no class path\n");
	if (getenv("JDWP_LISTEN"))
		fprintf(stderr, "image: ignoring $JDWP_LISTEN; there is no JDWP agent in an image\n");
	if (getenv("ATL_CHECK_JNI"))
		fprintf(stderr, "image: ignoring $ATL_CHECK_JNI; -Xcheck:jni is a HotSpot option\n");
}

static create_java_vm_fn open_image(struct launcher_options *d)
{
	const char *path = d->vm_library ?: getenv("ATL_IMAGE_LIB") ?: DEFAULT_IMAGE_LIBRARY;

	void *handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
	if (!handle) {
		fprintf(stderr, "error: cannot open the image library %s: %s\n", path, dlerror());
		fprintf(stderr, "       pass --vm-library PATH, or set $ATL_IMAGE_LIB\n");
		exit(1);
	}

	/* the resolved path, not the one asked for: with a bare soname the loader
	 * decides, and which .so provided the VM is the whole question here */
	struct link_map *map = NULL;
	if (!dlinfo(handle, RTLD_DI_LINKMAP, &map) && map && map->l_name && *map->l_name)
		fprintf(stderr, "image: opened %s\n", map->l_name);
	else
		fprintf(stderr, "image: opened %s\n", path);

	/* dlsym ignores symbol versioning: HotSpot's export is
	 * JNI_CreateJavaVM@SUNWprivate_1.1, an image's is unversioned */
	create_java_vm_fn create = (create_java_vm_fn)dlsym(handle, "JNI_CreateJavaVM");
	if (!create) {
		fprintf(stderr, "error: %s is not a native-image shared library (no JNI_CreateJavaVM)\n", path);
		exit(1);
	}

	return create;
}

JNIEnv *create_vm(struct launcher_options *d, const char *apk, const char *app_lib_dir)
{
	JavaVM *vm;
	JNIEnv *env;

	create_java_vm_fn create_java_vm = open_image(d);
	GPtrArray *options = vm_common_properties(d, apk, app_lib_dir);

	/* -X is the caller's escape hatch and stays one: run.sh delivers
	 * -Dtmessages.native.path and -Dtmessages.tldump=0 through it */
	for (char **extra = d->extra_jvm_options; extra && *extra; extra++)
		g_ptr_array_add(options, g_strdup(*extra));

	JavaVMOption *vm_options = g_new0(JavaVMOption, options->len);
	for (guint i = 0; i < options->len; i++)
		vm_options[i].optionString = g_ptr_array_index(options, i);
	vm_options_dump(options);

	/*
	 * ignoreUnrecognized is JNI_TRUE here where the HotSpot backend has
	 * JNI_FALSE, and both halves of that are measured rather than assumed.
	 *
	 * On an image the flag governs the `-XX:` namespace and nothing else: every
	 * other string is accepted regardless, and an unknown or unparsable `-XX:`
	 * name is otherwise fatal. Fatal *and silent* -- JNI_CreateJavaVM returns
	 * JNI_ERR having printed nothing at all, naming neither the option nor a
	 * reason -- and there is no second chance: calling JNI_CreateJavaVM again in
	 * the same process after a failed call hangs (GraalVM CE 21.0.2), so a
	 * retry-and-degrade ladder is not available. One stray -XX: in
	 * $PORT_EXTRA_JVM_ARGS would therefore be an unexplained hang or exit.
	 *
	 * Nothing is lost by dropping those: the image path assembles no `-XX:`
	 * option itself, and a `-D` cannot be in that namespace. What the flag
	 * cannot cover -- a property accepted but not delivered -- is what
	 * vm_verify_properties() below is for.
	 */
	JavaVMInitArgs args = {
		.version = JNI_VERSION_1_8,
		.nOptions = (jint)options->len,
		.options = vm_options,
		.ignoreUnrecognized = JNI_TRUE,
	};

	jint ret = create_java_vm(&vm, (void **)&env, &args);
	if (ret != JNI_OK) {
		fprintf(stderr, "error: unable to launch the JVM (JNI_CreateJavaVM returned %d)\n", (int)ret);
		fprintf(stderr, "       the image names neither the option nor a reason;"
		                " the whole option set is echoed above\n");
		exit(1);
	}

	/* not "none rejected": with ignoreUnrecognized JNI_TRUE a bad -XX: is
	 * dropped silently, and the image reports neither. What each -D actually
	 * delivered is the next block's business */
	fprintf(stderr, "image: VM created with %u options\n", options->len);
	/* the same line the HotSpot backend prints, because run.sh and
	 * launcher/smoke-test.sh grep for it */
	fprintf(stderr, "JVM launched successfully\n");

	/* every property is in place before main.c looks up its first class, which
	 * is what android.os.Build$VERSION, android.os.SystemProperties and
	 * AssetManager.findApkPaths need -- they read these in a static
	 * initialiser or on first use */
	vm_verify_properties(env, options);

	g_free(vm_options);
	g_ptr_array_free(options, TRUE);

	return env;
}
