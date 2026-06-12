project('skia', ['c', 'cpp'], version: '0.1.0',
    default_options: ['cpp_std=c++17'])

gn_prog    = find_program('gn',    required: true)
ninja_prog = find_program('ninja', required: true)
python3    = import('python').find_installation('python3')

src_dir = meson.current_source_dir()
out_dir = meson.current_build_dir() / 'out'

# Skia strongly prefers (effectively requires) clang.
clang    = find_program('clang',   required: false)
clangcxx = find_program('clang++', required: false)
if not clang.found() or not clangcxx.found()
    warning('clang not found — Skia build will likely fail with GCC')
endif

gn_args = [
    'is_official_build=true',
    # Shared library: produces libskia.so
    'is_component_build=true',
    # Ganesh (OpenGL ES) GPU backend
    'skia_enable_ganesh=true',
    'skia_use_gl=true',
    # Disable backends we don't need yet
    'skia_use_vulkan=false',
    'skia_use_metal=false',
    'skia_use_dawn=false',
    # Don't build sample apps / tools / PDF
    'skia_enable_tools=false',
    'skia_enable_pdf=false',
    'skia_enable_skottie=false',
    'skia_use_expat=false',
    # Skip ICU to avoid the large bundled third-party dep
    'skia_use_icu=false',
    'skia_use_client_icu=false',
    # Text: fontconfig-based font manager
    'skia_use_fontconfig=true',
    # Prefer system libraries where possible
    'skia_use_system_freetype2=true',
    'skia_use_system_harfbuzz=true',
    'skia_use_system_libjpeg_turbo=true',
    'skia_use_system_libpng=true',
    'skia_use_system_libwebp=true',
    'skia_use_system_zlib=true',
    'skia_use_system_expat=true',
]

if clang.found() and clangcxx.found()
    gn_args += [
        'cc="'  + clang.full_path()    + '"',
        'cxx="' + clangcxx.full_path() + '"',
    ]
endif

libskia = custom_target('skia',
    input:   files('build_skia.py'),
    output:  'libskia.so',
    command: [python3, '@INPUT@',
              gn_prog.full_path(), ninja_prog.full_path(),
              src_dir, out_dir, '@OUTPUT@'] + gn_args,
    # build_by_default=false: only built when something links against skia_dep
    build_by_default: false,
    install: true,
    install_dir: get_option('libdir'),
)

skia_dep = declare_dependency(
    link_with:           [libskia],
    include_directories: include_directories('.'),
)
