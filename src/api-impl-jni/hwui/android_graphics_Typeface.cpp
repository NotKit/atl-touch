#include "../defines.h"
#include "MinikinGlue.h"

#include <include/core/SkTypeface.h>

extern "C" {
#include "../generated_headers/android_graphics_Typeface.h"
}

using android::Typeface;

JNIEXPORT jlong JNICALL Java_android_graphics_Typeface_nativeCreateNamed(JNIEnv *env, jclass, jstring family_name, jint style)
{
	Typeface *base;
	if (family_name) {
		const char *name = env->GetStringUTFChars(family_name, NULL);
		base = android::typeface_create_named(name);
		env->ReleaseStringUTFChars(family_name, name);
	} else {
		android::typeface_init_default();
		base = nullptr;
	}
	Typeface *result = Typeface::createRelative(base, (Typeface::Style)style);
	delete base;
	return _INTPTR(result);
}

JNIEXPORT jlong JNICALL Java_android_graphics_Typeface_nativeCreateRelative(JNIEnv *env, jclass, jlong base_ptr, jint style)
{
	android::typeface_init_default();
	return _INTPTR(Typeface::createRelative((Typeface *)_PTR(base_ptr), (Typeface::Style)style));
}

JNIEXPORT jlong JNICALL Java_android_graphics_Typeface_nativeCreateFromFile(JNIEnv *env, jclass, jstring path)
{
	android::typeface_init_default();
	if (!path)
		return 0;
	const char *path_str = env->GetStringUTFChars(path, NULL);
	std::shared_ptr<minikin::FontFamily> family = android::typeface_family_from_file(path_str);
	env->ReleaseStringUTFChars(path, path_str);
	if (!family)
		return 0;
	std::vector<std::shared_ptr<minikin::FontFamily>> families = {family};
	/* app fonts fall back to the system collection for missing glyphs */
	const Typeface *fallback = Typeface::resolveDefault(nullptr);
	for (const auto &family_ptr : fallback->fFontCollection->getFamilies())
		families.push_back(family_ptr);
	return _INTPTR(Typeface::createFromFamilies(std::move(families), android::RESOLVE_BY_FONT_TABLE,
	                                            android::RESOLVE_BY_FONT_TABLE));
}
