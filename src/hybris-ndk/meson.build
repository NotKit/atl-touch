# Host libraries that answer for Android libraries an app's own native code
# links against, forwarding into the device's real ones through libhybris.
# bionic_translation picks them up by soname from atl-android-ndk.cfg.

hybris_ndk_lib = static_library('atl_hybris_ndk', 'hybris_ndk.c',
                                dependencies: [dependency('jni')],
                                pic: true)

shared_library('atl_camera2ndk', 'camera2ndk_forward.c',
               link_with: hybris_ndk_lib,
               dependencies: [dependency('jni')],
               install: true, soversion: 0)

shared_library('atl_binder_ndk', 'binder_ndk_forward.c',
               link_with: hybris_ndk_lib,
               install: true, soversion: 0)

shared_library('atl_nativewindow', 'nativewindow_forward.c',
               link_with: hybris_ndk_lib,
               dependencies: [dependency('jni')],
               install: true, soversion: 0)

shared_library('atl_vndksupport', 'vndksupport.c',
               install: true, soversion: 0)

# the vendor libraries Google Camera's camera graph is actually built on: an
# empty stub kills the process on both (see vendor_forward.c)
foreach fwd : [
	['atl_aion', 'lib_aion_buffer.so', '/vendor/lib64/lib_aion_buffer.so',
	 'vendor_symbols_aion.h'],
	['atl_opencl', 'libOpenCL.so', '/vendor/lib64/libOpenCL.so',
	 'vendor_symbols_opencl.h'],
]
	shared_library(fwd[0], 'vendor_forward.c',
	               c_args: ['-DATL_VENDOR_SONAME="' + fwd[1] + '"',
	                        '-DATL_VENDOR_PATH="' + fwd[2] + '"',
	                        '-DATL_VENDOR_SYMBOLS="' + fwd[3] + '"'],
	               link_with: hybris_ndk_lib,
	               install: true, soversion: 0)
endforeach

# one empty library per vendor soname, so its constructor can name itself
foreach stub : [
	['atl_stub_libgxp', 'libgxp.so'],
	['atl_stub_libedgetpu_client', 'libedgetpu_client.google.so'],
	['atl_stub_libedgetpu_tachyon', 'libedgetpu_tachyon.google.so'],
	['atl_stub_libedgetpu_util', 'libedgetpu_util.so'],
	['atl_stub_libOpenCL_pixel', 'libOpenCL-pixel.so'],
	['atl_stub_lib_jpg_encoder', 'lib_jpg_encoder.so'],
]
	shared_library(stub[0], 'vendor_stub.c',
	               c_args: ['-DATL_VENDOR_SONAME="' + stub[1] + '"'],
	               install: true, soversion: 0)
endforeach

install_data('atl-android-ndk.cfg',
             install_dir: join_paths(get_option('prefix'), get_option('datadir')) + '/bionic_translation/cfg.d')
