#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <gio/gio.h>

#include "src/api-impl-jni/defines.h"
#include "util.h"

const char *attribute_set_get_string(JNIEnv *env, jobject attrs, char *attribute, char *schema)
{
	if (!attrs)
		return NULL;

	if (!schema)
		schema = "http://schemas.android.com/apk/res/android";

	jstring string = (jstring)(*env)->CallObjectMethod(env, attrs, handle_cache.attribute_set.getAttributeValue_string, _JSTRING(schema), _JSTRING(attribute));
	return string ? _CSTRING(string) : NULL;
}

int attribute_set_get_int(JNIEnv *env, jobject attrs, char *attribute, char *schema, int default_value)
{
	if (!attrs)
		return default_value;

	if (!schema)
		schema = "http://schemas.android.com/apk/res/android";

	return (*env)->CallIntMethod(env, attrs, handle_cache.attribute_set.getAttributeValue_int, _JSTRING(schema), _JSTRING(attribute), default_value);
}

JavaVM *jvm;

// TODO: use this everywhere, not just for gdb helper functions
JNIEnv *get_jni_env(void)
{
	JNIEnv *env;
	(*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6);
	return env;
}

/* JNI's "UTF-8" is not UTF-8: a character outside the BMP comes out as the
 * 6-byte CESU-8 encoding of its surrogate pair, and U+0000 as 0xC0 0x80. GLib
 * validates strictly, so GetStringUTFChars into g_variant_new_string() fails on
 * the first emoji. Go through UTF-16, which both sides agree on. */
char *jstring_to_utf8(JNIEnv *env, jstring str)
{
	if (!str)
		return NULL;
	const jchar *chars = (*env)->GetStringChars(env, str, NULL);
	if (!chars)
		return NULL;
	char *utf8 = g_utf16_to_utf8((const gunichar2 *)chars, (*env)->GetStringLength(env, str),
	                             NULL, NULL, NULL);
	(*env)->ReleaseStringChars(env, str, chars);
	return utf8 ? utf8 : g_strdup("");
}

/* the same hazard in reverse: NewStringUTF takes modified UTF-8, so real UTF-8
 * from outside (an emoji committed by the keyboard) has to be converted */
jstring utf8_to_jstring(JNIEnv *env, const char *utf8)
{
	if (!utf8)
		return NULL;
	glong len = 0;
	gunichar2 *utf16 = g_utf8_to_utf16(utf8, -1, NULL, &len, NULL);
	if (!utf16) /* not valid UTF-8 at all: nothing sensible to hand to java */
		return (*env)->NewStringUTF(env, "");
	jstring str = (*env)->NewString(env, (const jchar *)utf16, len);
	g_free(utf16);
	return str;
}

JNIEnv *_gdb_get_jni_env(void)
{
	return get_jni_env();
}

/* Print a pending exception and clear it.
 *
 * ART's ExceptionDescribe puts the exception back after printing it
 * (art/runtime/jni/jni_internal.cc), so describing alone leaves it pending. The
 * next call into managed code then unwinds out of its first bytecode instead of
 * running, which is how one throwing app callback silently swallows the rest of
 * a lifecycle. Native call sites entered from the event loop have no Java frame
 * to propagate to, so they all have to clear. */
void atl_report_pending_exception(JNIEnv *env)
{
	if (!(*env)->ExceptionCheck(env))
		return;
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);
}

/* Resolve a class the runtime cannot run without.
 *
 * A NULL jclass is not something a later JNI call survives: GetMethodID
 * dereferences it inside the VM, so the miss surfaces as a SIGSEGV in a libjvm
 * frame instead of as the name that was not found. On ART these all come off the
 * boot class path; on a stock JVM one missing class path entry is enough to make
 * a framework class fail its <clinit>. */
jclass atl_find_class_or_exit(JNIEnv *env, const char *name)
{
	jclass class = (*env)->FindClass(env, name);
	if (class)
		return class;

	fprintf(stderr, "error: the framework needs %s and it did not load:\n", name);
	atl_report_pending_exception(env);
	exit(1);
}

void _gdb_get_java_stack_trace(void)
{
	JNIEnv *env = get_jni_env();
	(*env)->ExceptionDescribe(env);
}

void _gdb_force_java_stack_trace(void)
{
	JNIEnv *env = get_jni_env();
	(*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/Exception"), "forced stack trace");
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);
}

extern char *apk_path;
void extract_from_apk(const char *path, const char *target)
{
	JNIEnv *env = get_jni_env();
	(*env)->CallStaticVoidMethod(env, handle_cache.asset_manager.class, handle_cache.asset_manager.extractFromAPK, _JSTRING(apk_path), _JSTRING(path), _JSTRING(target));
	/* extractFromAPK throws if the copy itself fails; clear it here, or it would
	 * be delivered at whatever JNI call this caller makes next */
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
	}
}

/* logging with fallback to stderr */

typedef int __android_log_vprint_type(int prio, const char *tag, const char *fmt, va_list ap);

static int fallback_verbose_log(int prio, const char *tag, const char *fmt, va_list ap)
{
	int ret;

	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&mutex);
	static char buf[1024];
	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
	fprintf(stderr, "%w64u: %s\n", (uint64_t)pthread_self(), buf);
	pthread_mutex_unlock(&mutex);

	return ret;
}

static int android_log_vprintf(int prio, const char *tag, const char *fmt, va_list ap)
{

	static __android_log_vprint_type *_android_log_vprintf = NULL;
	if (!_android_log_vprintf) {
		_android_log_vprintf = dlsym(RTLD_DEFAULT, "__android_log_vprint");

		if (!_android_log_vprintf) {
			_android_log_vprintf = &fallback_verbose_log;
		}
	}

	return _android_log_vprintf(prio, tag, fmt, ap);
}

int android_log_printf(android_LogPriority prio, const char *tag, const char *fmt, ...)
{
	int ret;

	va_list ap;
	va_start(ap, fmt);

	ret = android_log_vprintf(prio, tag, fmt, ap);

	va_end(ap);

	return ret;
}

/*
 * How wide one element of this buffer is, as a shift. libcore's Buffer carries
 * it as the _elementSizeShift field and every GL binding reads it from there;
 * the JDK's Buffer has no such field, so ask what kind of buffer it is.
 */
static int nio_element_size_shift(JNIEnv *env, jobject buffer)
{
	static const struct {
		const char *name;
		int shift;
	} kinds[] = {
		{"java/nio/ByteBuffer", 0},
		{"java/nio/ShortBuffer", 1},
		{"java/nio/CharBuffer", 1},
		{"java/nio/IntBuffer", 2},
		{"java/nio/FloatBuffer", 2},
		{"java/nio/LongBuffer", 3},
		{"java/nio/DoubleBuffer", 3},
	};

	for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
		jclass class = (*env)->FindClass(env, kinds[i].name);
		if (!class) {
			(*env)->ExceptionClear(env);
			continue;
		}
		jboolean is_kind = (*env)->IsInstanceOf(env, buffer, class);
		(*env)->DeleteLocalRef(env, class);
		if (is_kind)
			return kinds[i].shift;
	}

	return 0; /* bytes: the only guess that cannot overrun the buffer */
}

void *get_nio_buffer(JNIEnv *env, jobject buffer, jarray *array_ref, jbyte **array)
{
	jclass class;
	void *pointer;
	int elementSizeShift, position;

	if (!buffer) {
		*array_ref = NULL;
		return NULL;
	}
	class = _CLASS(buffer);
	pointer = _PTR((*env)->GetLongField(env, buffer, _FIELD_ID(class, "address", "J")));
	elementSizeShift = nio_element_size_shift(env, buffer);
	position = (*env)->GetIntField(env, buffer, _FIELD_ID(class, "position", "I"));
	if (pointer) { // buffer is direct
		*array_ref = NULL;
		pointer += position << elementSizeShift;
	} else { // buffer is indirect
		*array_ref = (*env)->CallObjectMethod(env, buffer, _METHOD(class, "array", "()Ljava/lang/Object;"));
		jint offset = (*env)->CallIntMethod(env, buffer, _METHOD(class, "arrayOffset", "()I"));
		pointer = *array = (*env)->GetPrimitiveArrayCritical(env, *array_ref, NULL);
		pointer += (offset + position) << elementSizeShift;
	}
	return pointer;
}

void release_nio_buffer(JNIEnv *env, jarray array_ref, jbyte *array)
{
	if (array_ref)
		(*env)->ReleasePrimitiveArrayCritical(env, array_ref, array, 0);
}

int get_nio_buffer_size(JNIEnv *env, jobject buffer)
{
	jclass class = _CLASS(buffer);
	;
	int limit = (*env)->GetIntField(env, buffer, _FIELD_ID(class, "limit", "I"));
	int position = (*env)->GetIntField(env, buffer, _FIELD_ID(class, "position", "I"));

	return limit - position;
}

GVariant *intent_serialize(JNIEnv *env, jobject intent)
{
	if (!intent)
		return NULL;
	jstring action_jstr = _GET_OBJ_FIELD(intent, "action", "Ljava/lang/String;");
	jobject component = _GET_OBJ_FIELD(intent, "component", "Landroid/content/ComponentName;");
	jstring className_jstr = component ? _GET_OBJ_FIELD(component, "mClass", "Ljava/lang/String;") : NULL;
	jstring data_jstr = (*env)->CallObjectMethod(env, intent, handle_cache.intent.getDataString);

	GVariantBuilder extras_builder;
	g_variant_builder_init(&extras_builder, G_VARIANT_TYPE_VARDICT);
	jobject extras = _GET_OBJ_FIELD(intent, "extras", "Landroid/os/Bundle;");
	jobject extras_key_set = (*env)->CallObjectMethod(env, extras, handle_cache.bundle.keySet);
	jobjectArray extras_keys = (*env)->CallObjectMethod(env, extras_key_set, handle_cache.set.toArray);
	jsize extras_keys_length = (*env)->GetArrayLength(env, extras_keys);
	jclass parcelable_class = (*env)->FindClass(env, "android/os/Parcelable");
	for (jint i = 0; i < extras_keys_length; i++) {
		jstring key_jstr = (*env)->GetObjectArrayElement(env, extras_keys, i);
		jobject value_jobj = (*env)->CallObjectMethod(env, extras, handle_cache.bundle.get, key_jstr);
		if (!key_jstr || !value_jobj)
			continue;
		char *key = jstring_to_utf8(env, key_jstr);
		if ((*env)->IsInstanceOf(env, value_jobj, _CLASS(key_jstr))) {
			// a string extra can hold anything the app put there, so it takes the
			// UTF-16 route as well (see jstring_to_utf8 above)
			char *value = jstring_to_utf8(env, value_jobj);
			g_variant_builder_add(&extras_builder, "{sv}", key, g_variant_new_string(value));
			g_free(value);
		} else if ((*env)->IsInstanceOf(env, value_jobj, parcelable_class)) {
			GVariantBuilder parcel_builder;
			g_variant_builder_init(&parcel_builder, G_VARIANT_TYPE_TUPLE);
			jobject parcel = (*env)->NewObject(env, handle_cache.builder_parcel.class, handle_cache.builder_parcel.constructor, _INTPTR(&parcel_builder));
			(*env)->CallVoidMethod(env, parcel, handle_cache.parcel.writeParcelable, value_jobj, 0);
			GVariant *parcel_variant = g_variant_builder_end(&parcel_builder);
			g_variant_builder_add(&extras_builder, "{sv}", key, parcel_variant);
			(*env)->DeleteLocalRef(env, parcel);
		} else {
			printf("intent_serialize: skipping non-string, non-parcelable extra: %s\n", key);
		}
		g_free(key);
		(*env)->DeleteLocalRef(env, key_jstr);
		(*env)->DeleteLocalRef(env, value_jobj);
	}

	char *action = jstring_to_utf8(env, action_jstr);
	char *className = jstring_to_utf8(env, className_jstr);
	char *data = jstring_to_utf8(env, data_jstr);
	const char *dbus_name = g_application_get_application_id(g_application_get_default());
	GVariant *variant = g_variant_new(INTENT_G_VARIANT_TYPE_STRING, action ?: "", className ?: "", data ?: "", &extras_builder, dbus_name);
	g_free(action);
	g_free(className);
	g_free(data);
	return variant;
}

jobject intent_deserialize(JNIEnv *env, GVariant *variant)
{
	const char *action;
	const char *className;
	const char *data;
	GVariantIter *extras;
	g_variant_get(variant, INTENT_G_VARIANT_TYPE_STRING, &action, &className, &data, &extras, NULL);
	if (action && action[0] == '\0')
		action = NULL;
	if (className && className[0] == '\0')
		className = NULL;
	if (data && data[0] == '\0')
		data = NULL;

	jobject intent = (*env)->NewObject(env, handle_cache.intent.class, handle_cache.intent.constructor);
	_SET_OBJ_FIELD(intent, "action", "Ljava/lang/String;", _JSTRING(action));
	if (className)
		(*env)->CallObjectMethod(env, intent, handle_cache.intent.setClassName, _GET_STATIC_OBJ_FIELD(handle_cache.context.class, "this_application", "Landroid/app/Application;"), _JSTRING(className));
	if (data)
		_SET_OBJ_FIELD(intent, "data", "Landroid/net/Uri;", (*env)->CallStaticObjectMethod(env, handle_cache.uri.class, handle_cache.uri.parse, _JSTRING(data)));
	const char *key;
	GVariant *value;
	while (g_variant_iter_loop(extras, "{sv}", &key, &value)) {
		if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
			(*env)->CallObjectMethod(env, intent, handle_cache.intent.putExtraCharSequence, _JSTRING(key), _JSTRING(g_variant_get_string(value, NULL)));
		} else if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32)) {
			(*env)->CallObjectMethod(env, intent, handle_cache.intent.putExtraInt, _JSTRING(key), g_variant_get_int32(value));
		} else if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64)) {
			(*env)->CallObjectMethod(env, intent, handle_cache.intent.putExtraLong, _JSTRING(key), g_variant_get_int64(value));
		} else if (g_variant_is_of_type(value, G_VARIANT_TYPE_BYTESTRING)) {
			gsize size;
			const int8_t *message = g_variant_get_fixed_array(value, &size, 1);
			jbyteArray bytesMessage = (*env)->NewByteArray(env, size);
			(*env)->SetByteArrayRegion(env, bytesMessage, 0, size, message);
			(*env)->CallObjectMethod(env, intent, handle_cache.intent.putExtraByteArray, _JSTRING(key), bytesMessage);
		} else if (g_variant_is_of_type(value, G_VARIANT_TYPE_TUPLE)) {
			GVariantIter parcel_iter;
			g_variant_iter_init(&parcel_iter, value);
			jobject parcel = (*env)->NewObject(env, handle_cache.iter_parcel.class, handle_cache.iter_parcel.constructor, _INTPTR(&parcel_iter));
			/* the app has a class loader of its own; ours cannot see its classes */
			jclass loaded_app_class = (*env)->FindClass(env, "android/atl/ATLLoadedApp");
			jobject class_loader = (*env)->CallStaticObjectMethod(env, loaded_app_class,
			                                                      _STATIC_METHOD(loaded_app_class, "getPrimaryClassLoader", "()Ljava/lang/ClassLoader;"));
			jobject parcelable = (*env)->CallObjectMethod(env, parcel, handle_cache.parcel.readParcelable, class_loader);
			if ((*env)->ExceptionCheck(env)) {
				(*env)->ExceptionDescribe(env);
				(*env)->ExceptionClear(env);
			}
			(*env)->CallObjectMethod(env, intent, handle_cache.intent.putExtraParcelable, _JSTRING(key), parcelable);
			(*env)->DeleteLocalRef(env, parcelable);
			(*env)->DeleteLocalRef(env, parcel);
		}
	}
	g_variant_iter_free(extras);
	return intent;
}

const char *intent_actionname_from_type(int type)
{
	switch (type) {
		case 0:
			return "app.startActivity";
		case 1:
			return "app.startService";
		case 2:
			return "app.sendBroadcast";
		default:
			return NULL;
	}
}
