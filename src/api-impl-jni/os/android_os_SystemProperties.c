/*
 * The host Android system properties, on a device that has some.
 *
 * On a Halium port the container's property store is readable from the host
 * through libhybris' libandroid-properties (the same thing /usr/bin/getprop
 * uses), so an app asking what device it runs on can be told the truth:
 * ro.product.device is really "caiman", ro.product.model really "Pixel 9 Pro".
 * Everywhere else the library is absent and every lookup answers NULL, which
 * is what android.os.SystemProperties already did.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include "../generated_headers/android_os_SystemProperties.h"

/* bionic's PROP_VALUE_MAX; property_get writes at most this much */
#define ATL_PROP_VALUE_MAX 92

static int (*property_get_fn)(const char *key, char *value, const char *default_value);

/* one attempt per process: no library means no host properties, not an error */
static int host_properties_available(void)
{
	static int tried;
	static const char *const sonames[] = {"libandroid-properties.so.1",
	                                      "libandroid-properties.so"};

	if (tried)
		return property_get_fn != NULL;
	tried = 1;

	property_get_fn = dlsym(RTLD_DEFAULT, "property_get");
	for (unsigned i = 0; !property_get_fn && i < sizeof(sonames) / sizeof(sonames[0]); i++) {
		void *handle = dlopen(sonames[i], RTLD_LAZY);

		if (!handle)
			continue;
		property_get_fn = dlsym(handle, "property_get");
	}
	if (!property_get_fn)
		fprintf(stderr, "SystemProperties: no host property store (%s)\n", dlerror());
	return property_get_fn != NULL;
}

JNIEXPORT jstring JNICALL Java_android_os_SystemProperties_native_1get(JNIEnv *env, jclass this,
                                                                      jstring name)
{
	char value[ATL_PROP_VALUE_MAX + 1] = {0};
	const char *key;
	jstring result = NULL;

	if (!name || !host_properties_available())
		return NULL;

	key = (*env)->GetStringUTFChars(env, name, NULL);
	if (property_get_fn(key, value, NULL) > 0 && value[0])
		result = (*env)->NewStringUTF(env, value);
	(*env)->ReleaseStringUTFChars(env, name, key);
	return result;
}
