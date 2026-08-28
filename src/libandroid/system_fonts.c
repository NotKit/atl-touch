/*
 * The NDK system-font API (android/system_fonts.h, android/font.h), over
 * fontconfig.
 *
 * Android's ASystemFontIterator walks /system/etc/fonts.xml: a curated set of
 * roughly one family per script, not the whole filesystem.  Return that shape,
 * computed from fontconfig's own configuration rather than a hardcoded list.
 * For each generic family and style, ask fontconfig for its preference order
 * and keep a face only when it covers a language none of the earlier ones did.
 *
 * Handing over every face a distro installs would also work -- but a client
 * that has no font metadata of its own then pays for all of them.  Gecko's FT2
 * font list, for one, opens each file with FreeType to read its name table and
 * cmap, and a desktop font package is ~2500 files against Android's ~100.
 *
 * ATL_NO_SYSTEM_FONTS=1 makes the iterator report nothing, which sends clients
 * back to whatever fallback they have.
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h> /* not self-contained: needs the above */
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct AFont {
	char *path;
	char *locale;   /* the face's first language, or NULL */
	uint16_t weight;/* OpenType scale, 100..1000 */
	bool italic;
	size_t index;   /* face index within a .ttc */
};

struct ASystemFontIterator {
	size_t next;
};

/* Built once, then copied per AFont handed out: AFont_close() frees the copy
 * while the list stays for the next iterator. */
static pthread_mutex_t fonts_lock = PTHREAD_MUTEX_INITIALIZER;
static struct AFont *fonts;
static size_t fonts_count;
static bool fonts_built;

static void add_font(FcPattern *pattern)
{
	FcChar8 *file = NULL;
	if (FcPatternGetString(pattern, FC_FILE, 0, &file) != FcResultMatch || !file)
		return;

	int index = 0;
	FcPatternGetInteger(pattern, FC_INDEX, 0, &index);
	/* fontconfig packs a variable font's named instance into the high half. */
	size_t face = (size_t)(index & 0xffff);

	for (size_t i = 0; i < fonts_count; i++)
		if (fonts[i].index == face && !strcmp(fonts[i].path, (const char *)file))
			return;

	struct AFont *grown = realloc(fonts, (fonts_count + 1) * sizeof(*fonts));
	if (!grown)
		return;
	fonts = grown;

	struct AFont *font = &fonts[fonts_count];
	memset(font, 0, sizeof(*font));
	font->path = strdup((const char *)file);
	if (!font->path)
		return;
	font->index = face;

	int slant = FC_SLANT_ROMAN;
	FcPatternGetInteger(pattern, FC_SLANT, 0, &slant);
	font->italic = slant != FC_SLANT_ROMAN;

	int weight = FC_WEIGHT_REGULAR;
	FcPatternGetInteger(pattern, FC_WEIGHT, 0, &weight);
	font->weight = (uint16_t)FcWeightToOpenType(weight);

	FcLangSet *langs = NULL;
	if (FcPatternGetLangSet(pattern, FC_LANG, 0, &langs) == FcResultMatch && langs) {
		FcStrSet *set = FcLangSetGetLangs(langs);
		if (set) {
			FcStrList *it = FcStrListCreate(set);
			if (it) {
				FcChar8 *lang = FcStrListNext(it);
				if (lang)
					font->locale = strdup((const char *)lang);
				FcStrListDone(it);
			}
			FcStrSetDestroy(set);
		}
	}

	fonts_count++;
}

/* Walk one generic's preference order, keeping what adds language coverage. */
static void add_generic(const char *family, int weight, int slant)
{
	FcPattern *pattern = FcPatternCreate();
	if (!pattern)
		return;
	FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)family);
	FcPatternAddInteger(pattern, FC_WEIGHT, weight);
	FcPatternAddInteger(pattern, FC_SLANT, slant);
	FcConfigSubstitute(NULL, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);

	FcResult result;
	/* trim: fontconfig drops fonts that add no character coverage. */
	FcFontSet *set = FcFontSort(NULL, pattern, FcTrue, NULL, &result);
	FcPatternDestroy(pattern);
	if (!set)
		return;

	FcLangSet *covered = FcLangSetCreate();
	for (int i = 0; i < set->nfont; i++) {
		FcLangSet *langs = NULL;
		if (FcPatternGetLangSet(set->fonts[i], FC_LANG, 0, &langs) != FcResultMatch)
			continue;
		/* The first is the generic's own answer and is always kept. */
		if (i > 0 && covered && FcLangSetContains(covered, langs))
			continue;
		add_font(set->fonts[i]);
		if (covered) {
			FcLangSet *union_ = FcLangSetUnion(covered, langs);
			if (union_) {
				FcLangSetDestroy(covered);
				covered = union_;
			}
		}
	}
	if (covered)
		FcLangSetDestroy(covered);
	FcFontSetDestroy(set);
}

static void add_match(const char *family)
{
	FcPattern *pattern = FcPatternCreate();
	if (!pattern)
		return;
	FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)family);
	FcConfigSubstitute(NULL, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);

	FcResult result;
	FcPattern *match = FcFontMatch(NULL, pattern, &result);
	FcPatternDestroy(pattern);
	if (!match)
		return;
	add_font(match);
	FcPatternDestroy(match);
}

/* The framework's own faces -- Roboto, on Android and here.  Same directory
 * TypefaceCollection.cpp lays out sans-serif from, so a client's text and the
 * framework's agree; and it is where Gecko's "Roboto" default comes from. */
static const char *bundled_font_dir(void)
{
	static char dir[4096];
	const char *env = getenv("ATL_FONT_DIR");
	if (env && *env) {
		snprintf(dir, sizeof(dir), "%s", env);
	} else {
		Dl_info info;
		if (!dladdr((const void *)&bundled_font_dir, &info) || !info.dli_fname)
			return NULL;
		const char *slash = strrchr(info.dli_fname, '/');
		if (!slash)
			return NULL;
		snprintf(dir, sizeof(dir), "%.*s/system/fonts",
		         (int)(slash - info.dli_fname), info.dli_fname);
	}
	struct stat st;
	if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
		return NULL;
	return dir;
}

static void add_dir(const char *dir)
{
	DIR *d = opendir(dir);
	if (!d)
		return;

	struct dirent *entry;
	while ((entry = readdir(d))) {
		const char *ext = strrchr(entry->d_name, '.');
		if (!ext || (strcasecmp(ext, ".ttf") && strcasecmp(ext, ".otf") &&
		             strcasecmp(ext, ".ttc")))
			continue;

		char path[4096];
		snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

		/* FcFreeTypeQuery reads the file's own name table, so these faces
		 * need not be in any fontconfig path. */
		int faces = 1;
		for (int i = 0; i < faces; i++) {
			FcPattern *pattern = FcFreeTypeQuery((const FcChar8 *)path, i,
			                                     NULL, &faces);
			if (!pattern)
				break;
			add_font(pattern);
			FcPatternDestroy(pattern);
		}
	}
	closedir(d);
}

static bool build_font_list(void)
{
	static const char *const generics[] = { "sans-serif", "serif", "monospace" };
	static const struct {
		int weight;
		int slant;
	} styles[] = {
		{ FC_WEIGHT_REGULAR, FC_SLANT_ROMAN },
		{ FC_WEIGHT_BOLD,    FC_SLANT_ROMAN },
		{ FC_WEIGHT_REGULAR, FC_SLANT_ITALIC },
		{ FC_WEIGHT_BOLD,    FC_SLANT_ITALIC },
	};

	pthread_mutex_lock(&fonts_lock);
	if (fonts_built)
		goto out;
	fonts_built = true;

	if (getenv("ATL_NO_SYSTEM_FONTS"))
		goto out;
	if (!FcInit()) {
		fprintf(stderr, "ASystemFontIterator_open: fontconfig failed to "
		                "initialise, no system fonts\n");
		goto out;
	}

	const char *bundled = bundled_font_dir();
	if (bundled)
		add_dir(bundled);

	for (size_t g = 0; g < sizeof(generics) / sizeof(*generics); g++)
		for (size_t s = 0; s < sizeof(styles) / sizeof(*styles); s++)
			add_generic(generics[g], styles[s].weight, styles[s].slant);
	add_match("emoji");

	if (getenv("ATL_DEBUG_FONTS"))
		for (size_t i = 0; i < fonts_count; i++)
			fprintf(stderr, "ATL font %zu: %s index=%zu weight=%u%s lang=%s\n",
			        i, fonts[i].path, fonts[i].index, fonts[i].weight,
			        fonts[i].italic ? " italic" : "",
			        fonts[i].locale ? fonts[i].locale : "-");
out:
	pthread_mutex_unlock(&fonts_lock);
	return fonts_count > 0;
}

// --- the iterator

struct ASystemFontIterator *ASystemFontIterator_open(void)
{
	if (!build_font_list())
		return NULL; /* a client with a fallback path should take it */
	return calloc(1, sizeof(struct ASystemFontIterator));
}

void ASystemFontIterator_close(struct ASystemFontIterator *iterator)
{
	free(iterator);
}

struct AFont *ASystemFontIterator_next(struct ASystemFontIterator *iterator)
{
	if (!iterator || iterator->next >= fonts_count)
		return NULL;

	const struct AFont *src = &fonts[iterator->next++];
	struct AFont *font = calloc(1, sizeof(*font));
	if (!font)
		return NULL;
	font->path = strdup(src->path);
	if (!font->path) {
		free(font);
		return NULL;
	}
	font->locale = src->locale ? strdup(src->locale) : NULL;
	font->weight = src->weight;
	font->italic = src->italic;
	font->index = src->index;
	return font;
}

// --- one font

void AFont_close(struct AFont *font)
{
	if (!font)
		return;
	free(font->path);
	free(font->locale);
	free(font);
}

const char *AFont_getFontFilePath(const struct AFont *font)
{
	/* declared _Nonnull in the NDK: never hand back NULL */
	return font && font->path ? font->path : "";
}

uint16_t AFont_getWeight(const struct AFont *font)
{
	return font ? font->weight : 400;
}

bool AFont_isItalic(const struct AFont *font)
{
	return font ? font->italic : false;
}

const char *AFont_getLocale(const struct AFont *font)
{
	return font ? font->locale : NULL;
}

size_t AFont_getCollectionIndex(const struct AFont *font)
{
	return font ? font->index : 0;
}

/* Variable-font axes are not reported: the faces above are static instances. */
size_t AFont_getAxisCount(const struct AFont *font)
{
	return 0;
}

uint32_t AFont_getAxisTag(const struct AFont *font, uint32_t axisIndex)
{
	return 0;
}

float AFont_getAxisValue(const struct AFont *font, uint32_t axisIndex)
{
	return 0.0f;
}
