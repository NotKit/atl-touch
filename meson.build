project('android_translation_layer', ['c', 'cpp', 'java'], default_options: ['b_lundef=false', 'cpp_std=c++17'])

gnome = import('gnome')
fs = import('fs')

incdir_dep = declare_dependency(include_directories: '.')
add_project_dependencies(incdir_dep, language: 'c')

cc = meson.get_compiler('c')
dir_base = meson.current_source_dir()
builddir_base = meson.current_build_dir()
libart_dep = dependency('art-standalone')
libdl_bio_dep = [
	cc.find_library('dl_bio')
]
libc_bio_dep = [
	cc.find_library('c_bio')
]
bootclasspath_dir = libart_dep.get_variable(pkgconfig: 'libdir') / 'java'

bootclasspath = bootclasspath_dir / 'core-all_classes.jar' + ':' + bootclasspath_dir / 'core-junit_classes.jar' + ':' + bootclasspath_dir / 'junit-runner_classes.jar'

marshal_files = gnome.genmarshal('marshal',
	sources: 'src/api-impl-jni/widgets/marshal.list',
	valist_marshallers: true,
	internal: true,
)

mpris = gnome.gdbus_codegen('mpris-dbus',
                            'src/api-impl-jni/media/org.mpris.MediaPlayer2.xml',
                            interface_prefix: 'org.mpris')
portal_openuri = gnome.gdbus_codegen('portal-openuri',
                                     'src/api-impl-jni/content/org.freedesktop.portal.OpenURI.xml',
                                     interface_prefix: 'org.freedesktop.portal')
portal_email = gnome.gdbus_codegen('portal-email',
                                   'src/api-impl-jni/content/org.freedesktop.portal.Email.xml',
                                   interface_prefix: 'org.freedesktop.portal')
unifiedpush_distributor = gnome.gdbus_codegen('unifiedpush-distributor',
                                              'src/api-impl-jni/content/org.unifiedpush.Distributor1.xml',
                                              interface_prefix: 'org.unifiedpush')
unifiedpush_connector = gnome.gdbus_codegen('unifiedpush-connector',
                                            'src/api-impl-jni/content/org.unifiedpush.Connector1.xml',
                                            interface_prefix: 'org.unifiedpush')

extra_deps = []
extra_jni_srcs = []

# Skia (rendering): built as a subproject via GN+Ninja, see subprojects/skia.wrap
skia_dep = dependency('skia')

layer_shell_dep = dependency('gtk4-layer-shell-0', required : false)
if layer_shell_dep.found()
  extra_deps = [ layer_shell_dep ]
  # the OSK (ATLKeyboardDialog) depended on the GTK widget hierarchy; needs a
  # canvas-drawn replacement

endif

# libandroid
libandroid_so = shared_library('android', [
                                          	'src/libandroid/asset_manager.c',
                                          	'src/libandroid/bitmap.c',
                                          	'src/libandroid/configuration.c',
                                          	'src/libandroid/egl.c',
                                          	'src/libandroid/input.c',
                                          	'src/libandroid/looper.c',
                                          	'src/libandroid/media.c',
                                          	'src/libandroid/native_window.c',
                                          	'src/libandroid/sensor.c',
                                          	'src/libandroid/trace.c',
                                          ],
                                          install: true,
                                          soversion: 0,
                                          dependencies: [
                                          	dependency('gtk4'), dependency('jni'), dependency('vulkan'), dependency('openxr').partial_dependency(includes: true)
                                          ],
                                          c_args: [
                                          	'-D_LARGEFILE64_SOURCE',
                                          ],
                                          link_args : [
                                          	'-Wl,-z,lazy',  # outdated Nvidia driver version 340 lacks EGL symbols
                                          ])

wayland_protos_dep = dependency('wayland-protocols', version: '>=1.12')

# wayland protocols for osk
subdir('protocol')

libtranslationlayer_so = shared_library('translation_layer_main', [
                                                                  	'src/api-impl-jni/android_app_NativeActivity.c',
                                                                  	'src/api-impl-jni/android_content_res_AssetManager.c',
                                                                  	'src/api-impl-jni/android_opengl_GLES20.c',
                                                                  	'src/api-impl-jni/android_opengl_GLUtils.c',
                                                                  	'src/api-impl-jni/android_os_Environment.c',
                                                                  	'src/api-impl-jni/android_os_MessageQueue.c',
                                                                  	'src/api-impl-jni/android_os_SystemClock.c',
                                                                  	'src/api-impl-jni/android_os_Vibrator.c',
                                                                  	'src/api-impl-jni/android_util_Log.c',
                                                                  	'src/api-impl-jni/android_view_Window.c',
                                                                  	'src/api-impl-jni/app/android_app_Activity.c',
                                                                  	'src/api-impl-jni/app/android_app_Dialog.c',
                                                                  	'src/api-impl-jni/app/android_app_NotificationManager.c',
                                                                  	'src/api-impl-jni/app/android_app_WallpaperManager.c',
                                                                  	'src/api-impl-jni/AssetInputStream.c',
                                                                  	'src/api-impl-jni/audio/android_media_AudioManager.c',
                                                                  	'src/api-impl-jni/audio/android_media_AudioRecord.c',
                                                                  	'src/api-impl-jni/audio/android_media_AudioTrack.c',
                                                                  	'src/api-impl-jni/audio/android_media_SoundPool.c',
                                                                  	'src/api-impl-jni/audio/audio.c',
                                                                  	'src/api-impl-jni/com_google_android_gles_jni_GLImpl.c',
                                                                  	'src/api-impl-jni/content/android_atl_ATLMediaContentProvider.c',
                                                                  	'src/api-impl-jni/content/android_content_ClipboardManager.c',
                                                                  	'src/api-impl-jni/content/android_content_ContentResolver.c',
                                                                  	'src/api-impl-jni/content/android_content_Context.c',
                                                                  	'src/api-impl-jni/content/android_content_res_XmlBlock.c',
                                                                  	'src/api-impl-jni/database/android_database_SQLiteCommon.c',
                                                                  	'src/api-impl-jni/database/android_database_SQLiteConnection.c',
                                                                  	'src/api-impl-jni/database/android_database_SQLiteGlobal.c',
                                                                  	'src/api-impl-jni/egl/com_google_android_gles_jni_EGLImpl.c',
                                                                  	'src/api-impl-jni/graphics/ATLCanvas.cpp',
                                                                  	'src/api-impl-jni/graphics/NinePatchPaintable.c',
                                                                  	'src/api-impl-jni/graphics/NinePatchSkia.cpp',
                                                                  	'src/api-impl-jni/graphics/android_atl_GskCanvas.cpp',
                                                                  	'src/api-impl-jni/graphics/android_graphics_Bitmap.cpp',
                                                                  	'src/api-impl-jni/graphics/android_graphics_BitmapFactory.cpp',
                                                                  	'src/api-impl-jni/graphics/android_graphics_Matrix.cpp',
                                                                  	'src/api-impl-jni/graphics/android_graphics_Paint.cpp',
                                                                  	'src/api-impl-jni/graphics/android_graphics_Path.cpp',
                                                                  	'src/api-impl-jni/graphics/android_graphics_Shader.cpp',
                                                                  	'src/api-impl-jni/graphics/android_view_RenderNode.cpp',
                                                                  	'src/api-impl-jni/graphics/android_graphics_drawable_Drawable.cpp',
                                                                  	'src/api-impl-jni/graphics/android_graphics_drawable_DrawableContainer.c',
                                                                  	'src/api-impl-jni/graphics/android_graphics_drawable_NinePatchDrawable.cpp',
                                                                  	'src/api-impl-jni/location/android_location_LocationManager.c',
                                                                  	'src/api-impl-jni/media/android_media_MediaCodec.c',
                                                                  	'src/api-impl-jni/media/android_media_MediaPlayer.c',
                                                                  	'src/api-impl-jni/media/android_media_session_MediaSession.c',
                                                                  	'src/api-impl-jni/net/android_net_ConnectivityManager.c',
                                                                  	'src/api-impl-jni/os/android_atl_GVariantBuilderParcel.c',
                                                                  	'src/api-impl-jni/os/android_atl_GVariantIterParcel.c',
                                                                  	'src/api-impl-jni/os/android_os_Process.c',
                                                                  	'src/api-impl-jni/sensors/android_hardware_SensorManager.c',
                                                                  	'src/api-impl-jni/text/android_text_Layout.cpp',
                                                                  	'src/api-impl-jni/util.c',
                                                                  	'src/api-impl-jni/ATLWindow.c',
                                                                  	'src/api-impl-jni/handle_cache.c',
                                                                  	'src/api-impl-jni/widgets/android_view_SurfaceView.c',
                                                                  	mpris,
                                                                  	portal_openuri,
                                                                  	portal_email,
                                                                  	unifiedpush_distributor,
                                                                  	unifiedpush_connector,
                                                                        wl_proto_headers,
                                                                        wl_proto_sources,
                                                                        extra_jni_srcs,
                                                                  ] + marshal_files,
                                                                  install: true,
                                                                  install_dir : get_option('libdir') / 'java/dex/android_translation_layer/natives',
                                                                  install_rpath: '$ORIGIN/:' + get_option('prefix') / get_option('libdir') / 'art',
                                                                  dependencies: [
                                                                        extra_deps,
                                                                  	dependency('gtk4', version: '>=4.14'), dependency('gl'), dependency('egl'), dependency('wayland-client'), dependency('jni'),
                                                                  	dependency('glfw3'), dependency('pangocairo'),
                                                                  	dependency('libportal'), dependency('sqlite3'), dependency('libavcodec', version: '>=59'), dependency('libdrm'),
                                                                  	dependency('gudev-1.0'), dependency('libswscale'), dependency('webkitgtk-6.0'),
                                                                        libart_dep, wayland_protos_dep, skia_dep
                                                                  ],
                                                                  link_with: [ libandroid_so ],
                                                                  link_args: [
                                                                  	'-lasound'
                                                                  ],
                                                                  c_args: [
                                                                  	'-D_LARGEFILE64_SOURCE',
                                                                  ],
                                                                  # skia is built without RTTI; subclassing its types requires matching
                                                                  cpp_args: [
                                                                  	'-fno-rtti',
                                                                  ])

# atl.gresources.xml
resources = gnome.compile_resources('com.gitlab.android_translation_layer.android_translation_layer', files('src/main-executable/atl.gresource.xml'))

executable('android-translation-layer', [
                                        	'src/main-executable/main.c',
                                        	'src/main-executable/actions.c',
                                        	'src/main-executable/bionic_compat.c',
                                        	'src/main-executable/libc_bio_path_overrides.c',
                                        	resources
                                        ],
                                        install: true,
                                        dependencies: [
                                        	dependency('gtk4'), dependency('jni'), dependency('glfw3'), declare_dependency(link_with: libtranslationlayer_so), libart_dep, dependency('dl'), libdl_bio_dep, libc_bio_dep, dependency('libportal'), dependency('fontconfig'),
                                        ],
                                        link_args: [
                                        	'-rdynamic'
                                        ],
                                        install_rpath: get_option('prefix') / get_option('libdir') / 'art' + ':' +
                                                       get_option('prefix') / get_option('libdir') / 'java/dex/android_translation_layer/natives',
                                        c_args: [
						'-DINSTALL_DATADIR="'+join_paths(get_option('prefix'), get_option('datadir'))+'"'
                                        ])

# hax.jar
subdir('src/api-impl')

# api-impl.jar
custom_target('api-impl.jar', build_by_default: true, input: [hax_jar], output: ['api-impl.jar'],
              install: true,
              install_dir : get_option('libdir') / 'java/dex/android_translation_layer',
              command: ['dx', '--dex', '--incremental', '--output='+join_paths(builddir_base, 'api-impl.jar'), hax_jar.full_path()])

# test-runner.jar
subdir('src/test-runner')

custom_target('test_runner.jar', build_by_default: false, input: [test_runner_jar], output: ['test_runner.jar'],
              install: true,
              install_dir : get_option('libdir') / 'java/dex/android_translation_layer',
              command: ['dx', '--dex', '--incremental', '--output='+join_paths(builddir_base, 'test_runner.jar'), test_runner_jar.full_path()])

#framework-res.apk
subdir('res/framework-res')

install_data([
             	'res/fonts.xml'
             ],
             install_dir: join_paths(get_option('prefix'), get_option('datadir')) + '/atl/system/etc')
