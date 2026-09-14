# Packaging a camera-enabled atlas into a click

What the Mercurygram click packaging (and any click built the same way) needs
in order to ship the camera work.

Short version: **no build-system changes at all, one line in the launcher.**

## Dependencies: none new

* The hybris backend is `dlopen`-only (`libcamera.so.1`) with vendored headers,
  so `libhybris-dev` is *not* a build dependency and the click does not link
  against libhybris. Nothing to add to `clickable.yaml`'s
  `dependencies_build`/`dependencies_host`.
* The gst backend uses `gstreamer-app-1.0` / `gstreamer-video-1.0`, both already
  pulled in by atlas' existing GStreamer dependency (media codecs). A build that
  compiled atlas before compiles it now.
* Nothing from libhybris may be bundled into the click: `libcamera.so.1` must
  resolve to the *device's* copy. The launcher prepends click paths to
  `LD_LIBRARY_PATH` but keeps the system path, so the default search finds it —
  do not add a bundled `libcamera*`.

## Launcher: enable the gate

The camera subsystem is off unless `ATL_UGLY_ENABLE_CAMERA` is set (same
treatment as microphone/location). In `mercurygram.sh`, next to the other
`ATL_*` exports:

```sh
# Camera1 (android.hardware.Camera) through the libhybris compat layer;
# without the gate every app sees zero cameras. Pin the backend so a missing
# libcamera.so.1 fails loudly instead of quietly serving videotestsrc.
export ATL_UGLY_ENABLE_CAMERA=1
export ATL_CAMERA_BACKEND=hybris
```

Optional, and worth having while bringing a device up:

```sh
export ATL_CAMERA_DUMP_FRAMES="${ANDROID_APP_DATA_DIR}/camframes"   # debug only
export ATL_CAMERA_HYBRIS_LIB=/usr/lib/aarch64-linux-gnu/libcamera.so.1
```

Leave `ATL_CAMERA_DUMP_FRAMES` **out** of a release click: it writes a PNG every
30 frames for the whole session.

## camera2: the same click, one different backend

`android.hardware.camera2` on a device is served by the `camera2ndk` backend —
the device's own Android camera2 stack (`libcamera2ndk.so` + `libmediandk.so`)
loaded through libhybris. The `hybris` backend is Camera1 only and reports no
camera2 cameras at all, so a click for an app that uses camera2 pins the other
one:

```sh
export ATL_UGLY_ENABLE_CAMERA=1
export ATL_CAMERA_BACKEND=camera2ndk
```

Still no new dependency and nothing bundled: both libraries are `android_dlopen`ed
out of the Android container, exactly like `libcamera.so.1`. What the click does
have to get right is the **Android loader's** search path, which is not
`LD_LIBRARY_PATH`:

```sh
# libcamera2ndk.so pulls in libandroid_runtime.so, which aborts in its own
# constructors in a host process - the empty stub next to the runtime has to
# win.  libhybris' default namespace is isolated and permits neither
# /vendor/lib64 nor /odm/lib64, so a vendor library an app needs is only
# reachable if it is named here.  /system/lib64 goes first: the vendor tree
# carries VNDK copies of libutils/libcutils/libc++ and picking them up a second
# time breaks the process.
export HYBRIS_LD_LIBRARY_PATH="${PKG_ROOT}/usr/lib/hybris-stubs:/system/lib64:/vendor/lib64:/odm/lib64"
```

`/vendor/lib64/egl` belongs on the end when the port keeps its EGL there.

## App bundles: base plus splits

An app that ships as a bundle (a base APK plus `split_*.apk`) is launched by
naming the *directory* that holds them, or with `ATL_APK_SPLITS` as a
`:`-separated list:

```sh
exec "${PKG_ROOT}/usr/bin/android-translation-layer" "${PKG_ROOT}/app" \
    -l com.example.app.MainActivity "$@"
```

Order is base first, then splits, everywhere at once — classpath, `ApkAssets`,
`splitSourceDirs` and native-library extraction — because classpath order *is*
resource override order. The manifest is always read out of `base.apk` by name.
`doc/AppBundles.md` is the detail; the packaging-relevant part is that a click
ships the directory as it came out of the installer and changes nothing else.

An app that needs a modern SDK level gets it in the same launcher:

```sh
export ATL_SDK_INT=36      # per app, never a global default; doc/Envs.md
```

## atl-touch pin and the prebuilt SDK

`build.sh` derives `ATL_SDK_TAG=sdk-<atl-touch short sha>` and downloads a
prebuilt SDK when the submodule tree is clean. After bumping the submodule to a
camera commit, either publish a matching `sdk-<sha>` release or build from
source for that run:

```sh
ATL_SDK_TAG=off clickable build          # full source build of atlas
```

A dirty `atl-touch` working tree already forces the source path, so local camera
iteration is never silently overridden by an older SDK.

## Confinement

`mercurygram.apparmor` is `"template": "unconfined"`, so there is no camera
policy group to add today. If a click is ever confined, it needs
`"policy_groups": ["camera"]` plus whatever the hybris IPC path requires —
untested, and the reason the current click stays unconfined.

## Verifying the packaged click

1. `strings usr/lib/java/dex/android_translation_layer/natives/libtranslation_layer_main.so | grep android_camera_connect_by_id`
   — the hybris backend made it into the build.
2. Launch and look for `Camera: using backend 'hybris'` in
   `journalctl --user -u "lomiri-app-launch--application-click--<pkg>_<app>_<ver>--.service"`.
   For camera2 the line is `Camera: using backend 'camera2ndk'`, and the two
   lines above it name the libraries it loaded.
3. Full checklists: `doc/CameraDevice.md` (Camera1), `doc/Camera2Device.md`
   (camera2, including the failure table and the frame/metadata dumps).
