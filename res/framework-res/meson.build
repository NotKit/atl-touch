aapt = find_program('aapt', required: false)

if aapt.found()
  aapt_command = [aapt, 'package', '-x', '-f', '--custom-package', 'com.android.internal', '-S', join_paths(dir_base, 'res/framework-res/res'), '-A', join_paths(dir_base, 'res/framework-res/assets'), '-M', join_paths(dir_base, 'res/framework-res/AndroidManifest.xml'), '-J', join_paths(dir_base, 'src/api-impl/com/android/internal'), '-F', '@OUTPUT@']
else  # fallback to prebuilt APK
  aapt_command = ['cp', '@INPUT@', '@OUTPUT@']
endif

  # Declared as input so that the copy reruns when the prebuilt changes;
  # without it ninja kept shipping an apk from before the last rebuild.
  framework_res = custom_target('framework-res.apk',
    input : join_paths(dir_base, 'res/framework-res/framework-res.apk'),
    output : 'framework-res.apk',
    command : aapt_command,
    install : true,
    install_dir : get_option('libdir') / 'java/dex/android_translation_layer')
