/*
 * The VM backend interface of the jar-based launcher.
 *
 * main.c is the boot sequence and knows nothing about where the VM comes from;
 * exactly one backend is linked into each executable:
 *
 *   vm_hotspot.c  android-translation-layer-hotspot -- libjvm.so, class path
 *   vm_image.c    android-translation-layer-image   -- a GraalVM native-image
 *                                                      shared library, dlopened
 *
 * Both define create_vm() and vm_validate_options(), so they must never end up
 * in the same target; the two source lists in meson.build are the only guard.
 */

#ifndef ATL_HOTSPOT_VM_H
#define ATL_HOTSPOT_VM_H

#include <glib.h>
#include <jni.h>

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
	char *vm_library; /* image backend: the native-image .so to create the VM from */
	char *vm_check;   /* run CLASS.main(String[]) right after the VM is up, then exit */
};

/* defined by main.c; the backends need it for -Datl.data.dir */
char *get_app_data_dir(void);

/* per-backend check of the options the backend cannot work without */
void vm_validate_options(struct launcher_options *d);

/* creates the VM and returns the env of the calling thread; never returns NULL */
JNIEnv *create_vm(struct launcher_options *d, const char *apk, const char *app_lib_dir);

/* --- shared between the backends (vm_common.c) ---------------------------- */

void add_option(GPtrArray *options, const char *format, ...) G_GNUC_PRINTF(2, 3);
void append_path(GString *joined, const char *entry);
void append_path_list(GString *joined, char **list);
void locale_options(GPtrArray *options);

/*
 * The -D properties that must reach Java on both paths: what the ART launcher
 * keeps in C globals, plus java.library.path and the locale.
 */
GPtrArray *vm_common_properties(struct launcher_options *d, const char *apk, const char *app_lib_dir);

/* echoes every option, in the format linux-port/run.sh greps for */
void vm_options_dump(GPtrArray *options);

/* reads every -Dkey=value back from Java and reports which value arrived; never
 * writes one back. Exits 1 when a property the launcher itself assembled did
 * not arrive, and reports (without failing) a caller -X -D the VM normalised */
void vm_verify_properties(JNIEnv *env, GPtrArray *options);

#endif /* ATL_HOTSPOT_VM_H */
