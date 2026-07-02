/*
 * Builds minikin FontCollections from the host's fontconfig fonts (via
 * Skia's fontconfig SkFontMgr), replacing Android's fonts.xml parsing.
 */

#include "MinikinSkia.h"
#include "Typeface.h"

#include <include/core/SkFontMgr.h>
#include <include/core/SkStream.h>
#include <include/core/SkTypeface.h>
#include <log/log.h>
#include <minikin/FontCollection.h>
#include <minikin/FontFamily.h>

#include <algorithm>
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

static std::shared_ptr<minikin::FontFamily> make_family(const char *name)
{
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

static std::shared_ptr<minikin::FontCollection> build_collection(const char *primary)
{
	std::vector<std::shared_ptr<minikin::FontFamily>> families;
	if (auto family = make_family(primary))
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
		if (auto family = make_family(name))
			families.push_back(family);
	}
	if (families.empty()) {
		/* last resort: whatever fontconfig calls the default */
		if (auto family = make_family(nullptr))
			families.push_back(family);
	}
	LOG_ALWAYS_FATAL_IF(families.empty(), "no usable fonts found via fontconfig");
	return std::make_shared<minikin::FontCollection>(families);
}

static Typeface *make_typeface(const char *primary)
{
	Typeface *typeface = new Typeface();
	typeface->fFontCollection = build_collection(primary);
	typeface->fAPIStyle = Typeface::kNormal;
	typeface->fBaseWeight = SkFontStyle::kNormal_Weight;
	typeface->fStyle = minikin::FontStyle();
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

/* load a single font file into a minikin family (Typeface.createFromFile) */
std::shared_ptr<minikin::FontFamily> typeface_family_from_file(const char *path)
{
	sk_sp<SkTypeface> sk_typeface = atl_fontmgr()->makeFromFile(path);
	std::shared_ptr<minikin::MinikinFont> font = make_minikin_font(std::move(sk_typeface));
	if (!font)
		return nullptr;
	std::vector<std::shared_ptr<minikin::Font>> fonts;
	fonts.push_back(minikin::Font::Builder(font).build());
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
