# minikin: AOSP's text shaping/measurement/line-breaking library,
# vendored as-is from frameworks/minikin (android-13, SDK 33).
# Support libs (libutils/liblog/libbase) come from art-standalone's
# installed .so's; their headers are vendored in ../android-support.

minikin_art_libdir = libart_dep.get_variable(pkgconfig: 'libdir') / 'art'

minikin_deps = [
	dependency('harfbuzz'),
	dependency('harfbuzz-icu'),
	dependency('icu-uc'),
	cc.find_library('utils', dirs: minikin_art_libdir),
	cc.find_library('log', dirs: minikin_art_libdir),
	cc.find_library('base', dirs: minikin_art_libdir),
]

minikin_inc = include_directories('include', 'libs', '../android-support/include')

minikin_lib = static_library('minikin', [
	'libs/minikin/BidiUtils.cpp',
	'libs/minikin/BoundsCache.cpp',
	'libs/minikin/CmapCoverage.cpp',
	'libs/minikin/Emoji.cpp',
	'libs/minikin/Font.cpp',
	'libs/minikin/FontCollection.cpp',
	'libs/minikin/FontFamily.cpp',
	'libs/minikin/FontFeatureUtils.cpp',
	'libs/minikin/FontFileParser.cpp',
	'libs/minikin/FontUtils.cpp',
	'libs/minikin/GraphemeBreak.cpp',
	'libs/minikin/GreedyLineBreaker.cpp',
	'libs/minikin/Hyphenator.cpp',
	'libs/minikin/HyphenatorMap.cpp',
	'libs/minikin/Layout.cpp',
	'libs/minikin/LayoutCore.cpp',
	'libs/minikin/LayoutUtils.cpp',
	'libs/minikin/LineBreaker.cpp',
	'libs/minikin/LineBreakerUtil.cpp',
	'libs/minikin/Locale.cpp',
	'libs/minikin/LocaleListCache.cpp',
	'libs/minikin/MeasuredText.cpp',
	'libs/minikin/Measurement.cpp',
	'libs/minikin/MinikinInternal.cpp',
	'libs/minikin/OptimalLineBreaker.cpp',
	'libs/minikin/SparseBitSet.cpp',
	'libs/minikin/SystemFonts.cpp',
	'libs/minikin/WordBreaker.cpp',
], include_directories: minikin_inc,
   dependencies: minikin_deps,
   cpp_args: ['-Wno-deprecated-declarations'])

minikin_dep = declare_dependency(
	link_with: minikin_lib,
	include_directories: minikin_inc,
	dependencies: minikin_deps)
