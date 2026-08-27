/*
 * Builds minikin FontCollections, replacing Android's fonts.xml parsing.
 *
 * The generic families (sans-serif and its weight aliases, serif, monospace)
 * come from the Roboto faces installed beside the framework, so that text is
 * laid out against the same fonts Android uses. Everything else -- named
 * families and the script/emoji fallback chain -- still comes from the host's
 * fontconfig via Skia's SkFontMgr.
 */

#include "MinikinSkia.h"
#include "Typeface.h"

#include <include/core/SkFontMgr.h>
#include <include/core/SkStream.h>
#include <include/core/SkTypeface.h>
#include <log/log.h>
#include <minikin/FontCollection.h>
#include <minikin/FontFamily.h>

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

sk_sp<SkFontMgr> atl_fontmgr(void);

namespace android {

/* minikin reads font tables through GetFontData(), so the raw file bytes must
 * stay alive for the font's lifetime; fonts live for the process lifetime */
static std::shared_ptr<minikin::MinikinFont> make_minikin_font(sk_sp<SkTypeface> typeface)
{
	if (!typeface)
		return nullptr;
	int ttc_index = 0;
	std::unique_ptr<SkStreamAsset> stream(typeface->openStream(&ttc_index));
	if (!stream)
		return nullptr;
	const void *data = stream->getMemoryBase();
	size_t size = stream->getLength();
	if (!data) {
		char *buf = (char *)malloc(size); // intentionally immortal
		if (stream->read(buf, size) != size) {
			free(buf);
			return nullptr;
		}
		data = buf;
	} else {
		stream.release(); // keep the mapping alive instead
	}
	SkString name;
	typeface->getFamilyName(&name);
	return std::make_shared<MinikinFontSkia>(std::move(typeface), 0 /* sourceId */, data, size,
	                                         name.c_str(), ttc_index,
	                                         std::vector<minikin::FontVariation>());
}

/* --- the bundled Roboto faces ------------------------------------------- */

/* What res/fonts.xml declares for the sans-serif family: the file, the weight
 * the family gives it, and its slant. The weights are the family's, not the
 * files' -- Roboto-Thin's own OS/2 says 250. Order matters too: minikin keeps
 * the first of two equally distant faces, so Black has to precede Bold for a
 * bolded 500 (which asks for 800) to land on Black rather than Bold. */
static const struct {
	const char *file;
	uint16_t weight;
	bool italic;
} BUNDLED_SANS[] = {
    {"Roboto-Thin.ttf", 100, false},         {"Roboto-ThinItalic.ttf", 100, true},
    {"Roboto-Light.ttf", 300, false},        {"Roboto-LightItalic.ttf", 300, true},
    {"Roboto-Regular.ttf", 400, false},      {"Roboto-Italic.ttf", 400, true},
    {"Roboto-Medium.ttf", 500, false},       {"Roboto-MediumItalic.ttf", 500, true},
    {"Roboto-Black.ttf", 900, false},        {"Roboto-BlackItalic.ttf", 900, true},
    {"Roboto-Bold.ttf", 700, false},         {"Roboto-BoldItalic.ttf", 700, true},
};

/* The names that resolve to that family, and the weight each one starts from,
 * so Typeface.create("sans-serif-medium", ...) really is a 500. These are
 * fonts.xml's sans-serif family and its aliases. */
static const struct {
	const char *name;
	uint16_t weight;
} BUNDLED_SANS_NAMES[] = {
    {"sans-serif", 400},        {"sans-serif-thin", 100},  {"sans-serif-light", 300},
    {"sans-serif-medium", 500}, {"sans-serif-black", 900}, {"Roboto", 400},
    {"arial", 400},             {"helvetica", 400},        {"tahoma", 400},
    {"verdana", 400},
};

/* Where those faces are installed: system/fonts beside the framework's own
 * library, which is how build-atlas.sh and the click both lay it out.
 * ATL_FONT_DIR overrides it; nothing there means fall back to fontconfig. */
static const char *bundled_font_dir(void)
{
	static std::string dir;
	static std::once_flag once;
	std::call_once(once, [] {
		const char *env = getenv("ATL_FONT_DIR");
		if (env && *env) {
			dir = env;
		} else {
			Dl_info info;
			if (!dladdr((const void *)&bundled_font_dir, &info) || !info.dli_fname)
				return;
			std::string path(info.dli_fname);
			size_t slash = path.rfind('/');
			if (slash == std::string::npos)
				return;
			dir = path.substr(0, slash) + "/system/fonts";
		}
		struct stat st;
		if (stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
			ALOGW("no bundled fonts at %s, falling back to fontconfig", dir.c_str());
			dir.clear();
		}
	});
	return dir.empty() ? nullptr : dir.c_str();
}

/* The base weight a generic name starts from, 0 if it is not one of ours. */
static uint16_t bundled_base_weight(const char *name)
{
	if (!name)
		return 0;
	for (const auto &entry : BUNDLED_SANS_NAMES)
		if (!strcasecmp(name, entry.name))
			return entry.weight;
	return 0;
}

/* The sans-serif family built from the bundled faces; nullptr if they are not
 * installed. Called under g_font_cache_mutex, so the memo needs no lock. */
static std::shared_ptr<minikin::FontFamily> bundled_sans(void)
{
	static std::shared_ptr<minikin::FontFamily> family;
	static bool tried = false;
	if (tried)
		return family;
	tried = true;
	const char *dir = bundled_font_dir();
	if (!dir)
		return nullptr;
	std::vector<std::shared_ptr<minikin::Font>> fonts;
	for (const auto &face : BUNDLED_SANS) {
		std::string path = std::string(dir) + "/" + face.file;
		std::shared_ptr<minikin::MinikinFont> font =
		    make_minikin_font(atl_fontmgr()->makeFromFile(path.c_str()));
		if (!font) {
			ALOGW("bundled font missing or unreadable: %s", path.c_str());
			continue;
		}
		minikin::FontStyle style(face.weight, face.italic ? minikin::FontStyle::Slant::ITALIC
		                                                  : minikin::FontStyle::Slant::UPRIGHT);
		fonts.push_back(minikin::Font::Builder(font).setStyle(style).build());
	}
	if (fonts.empty())
		return nullptr;
	family = std::make_shared<minikin::FontFamily>(std::move(fonts));
	return family;
}

/* Each matchFamilyStyle() is a fontconfig query against the whole font
 * database. build_collection() asks for ~22 families x 4 styles, and both the
 * per-name families and the (identical) fallback chain would otherwise be
 * re-queried on every Typeface.create() — thousands of fontconfig lookups
 * during a single layout of a text-heavy screen, which was taking >10s. Cache
 * families by name (negatives included) and whole collections by primary name.
 * Guarded by a mutex since text can be measured off the main thread. */
static std::mutex g_font_cache_mutex;
static std::map<std::string, std::shared_ptr<minikin::FontFamily>> g_family_cache;
static std::map<std::string, std::shared_ptr<minikin::FontCollection>> g_collection_cache;

static std::shared_ptr<minikin::FontFamily> make_family(const char *name)
{
	if (bundled_base_weight(name)) {
		if (auto family = bundled_sans())
			return family;
	}
	sk_sp<SkFontMgr> mgr = atl_fontmgr();
	std::vector<std::shared_ptr<minikin::Font>> fonts;
	/* only the four canonical styles: families like Noto Sans ship dozens of
	 * width/weight variants and loading them all costs seconds at startup */
	static const SkFontStyle styles[] = {
	    SkFontStyle::Normal(), SkFontStyle::Bold(),
	    SkFontStyle::Italic(), SkFontStyle::BoldItalic()};
	std::vector<SkTypefaceID> seen;
	for (const SkFontStyle &style : styles) {
		sk_sp<SkTypeface> typeface(mgr->matchFamilyStyle(name, style));
		if (!typeface)
			continue;
		if (std::find(seen.begin(), seen.end(), typeface->uniqueID()) != seen.end())
			continue;
		if (name) { /* fontconfig substitutes freely; reject wrong families */
			SkString actual;
			typeface->getFamilyName(&actual);
			if (!actual.equals(name) && strchr(name, '-') == nullptr)
				continue;
		}
		seen.push_back(typeface->uniqueID());
		std::shared_ptr<minikin::MinikinFont> font = make_minikin_font(std::move(typeface));
		if (font)
			fonts.push_back(minikin::Font::Builder(font).build());
	}
	if (fonts.empty())
		return nullptr;
	return std::make_shared<minikin::FontFamily>(std::move(fonts));
}

/* memoized make_family(); caller must hold g_font_cache_mutex */
static std::shared_ptr<minikin::FontFamily> family_cached(const char *name)
{
	std::string key = name ? name : "";
	auto it = g_family_cache.find(key);
	if (it != g_family_cache.end())
		return it->second;
	auto family = make_family(name); // may be nullptr; cache that too
	g_family_cache.emplace(std::move(key), family);
	return family;
}

static std::shared_ptr<minikin::FontCollection> build_collection(const char *primary)
{
	std::vector<std::shared_ptr<minikin::FontFamily>> families;
	if (auto family = family_cached(primary))
		families.push_back(family);
	/* script/emoji fallback; each family is skipped silently if absent */
	static const char *fallbacks[] = {
	    "Noto Sans", "DejaVu Sans", "Noto Color Emoji", "Noto Sans Symbols",
	    "Noto Sans Symbols2", "Noto Sans CJK SC", "Noto Sans CJK JP", "Noto Sans Arabic",
	    "Noto Sans Hebrew", "Noto Sans Devanagari", "Noto Sans Thai", "Noto Sans Bengali",
	    "Noto Sans Tamil", "Noto Sans Korean", "Noto Sans Georgian", "Noto Sans Armenian",
	    "Noto Sans Ethiopic", "Noto Sans Khmer", "Noto Sans Lao", "Noto Sans Myanmar",
	    "Noto Sans Sinhala",
	};
	for (const char *name : fallbacks) {
		if (primary && !strcmp(name, primary))
			continue;
		if (auto family = family_cached(name))
			families.push_back(family);
	}
	if (families.empty()) {
		/* last resort: whatever fontconfig calls the default */
		if (auto family = family_cached(nullptr))
			families.push_back(family);
	}
	LOG_ALWAYS_FATAL_IF(families.empty(), "no usable fonts found via fontconfig");
	return std::make_shared<minikin::FontCollection>(families);
}

/* memoized build_collection(); caller must hold g_font_cache_mutex */
static std::shared_ptr<minikin::FontCollection> collection_cached(const char *primary)
{
	std::string key = primary ? primary : "";
	auto it = g_collection_cache.find(key);
	if (it != g_collection_cache.end())
		return it->second;
	auto collection = build_collection(primary);
	g_collection_cache.emplace(std::move(key), collection);
	return collection;
}

static Typeface *make_typeface(const char *primary)
{
	std::lock_guard<std::mutex> lock(g_font_cache_mutex);
	Typeface *typeface = new Typeface();
	typeface->fFontCollection = collection_cached(primary);
	typeface->fAPIStyle = Typeface::kNormal;
	/* sans-serif-medium and friends are the same family at another weight */
	uint16_t weight = bundled_base_weight(primary);
	if (!weight)
		weight = SkFontStyle::kNormal_Weight;
	typeface->fBaseWeight = weight;
	typeface->fStyle = minikin::FontStyle(weight, minikin::FontStyle::Slant::UPRIGHT);
	return typeface;
}

const Typeface *typeface_init_default(void)
{
	static std::once_flag once;
	std::call_once(once, [] {
		/* fontconfig resolves the generic name to the configured UI font */
		Typeface::setDefault(make_typeface("sans-serif"));
	});
	return Typeface::resolveDefault(nullptr);
}

/* The face a Typeface's primary family resolves to. Paint's SkFont has to name
 * this one: its metrics are what Paint.getFontMetrics() reports, so a face that
 * differs from the collection's gives the right glyphs at the wrong line box. */
sk_sp<SkTypeface> typeface_sk_typeface(const Typeface *typeface)
{
	if (!typeface)
		typeface = typeface_init_default();
	const auto &families = typeface->fFontCollection->getFamilies();
	if (families.empty())
		return nullptr;
	auto closest = families[0]->getClosestMatch(typeface->fStyle);
	return static_cast<const MinikinFontSkia *>(closest.font->typeface().get())->RefSkTypeface();
}

/* Load a single font file into a minikin family (Typeface.createFromFile and
 * Typeface.Builder). weight/italic are RESOLVE_BY_FONT_TABLE unless the caller
 * declared them; a declared one overrides the file's OS/2 value, so a face
 * relabelled 700 matches a request for 700 and is not synthetically emboldened.
 * Same override the bundled sans-serif family above is built with. */
std::shared_ptr<minikin::FontFamily> typeface_family_from_file(const char *path, int weight,
                                                               int italic)
{
	sk_sp<SkTypeface> sk_typeface = atl_fontmgr()->makeFromFile(path);
	std::shared_ptr<minikin::MinikinFont> font = make_minikin_font(std::move(sk_typeface));
	if (!font)
		return nullptr;
	minikin::Font::Builder builder(font);
	if (weight != RESOLVE_BY_FONT_TABLE)
		builder.setWeight(std::clamp(weight, 1, 1000));
	if (italic != RESOLVE_BY_FONT_TABLE)
		builder.setSlant(italic ? minikin::FontStyle::Slant::ITALIC
		                        : minikin::FontStyle::Slant::UPRIGHT);
	std::vector<std::shared_ptr<minikin::Font>> fonts;
	fonts.push_back(builder.build());
	return std::make_shared<minikin::FontFamily>(std::move(fonts));
}

/* named lookup for Typeface.create(name, style): generic families and any
 * fontconfig family name both work; falls back to the default collection */
Typeface *typeface_create_named(const char *name)
{
	typeface_init_default();
	if (!name)
		return Typeface::createRelative(nullptr, Typeface::kNormal);
	Typeface *typeface = make_typeface(name);
	return typeface;
}

} // namespace android
