# Camera bring-up on an Ubuntu Touch device

The desktop loop proves the Camera1 API against GStreamer; this proves it
against a real HAL. Written as a checklist — expect ~15 minutes on a device
that already runs a click with an ATL runtime (e.g. the Mercurygram one).

`doc/CameraClickPackaging.md` covers what the packaging repo needs.

## 1. Get a camera-enabled atlas onto the device

The camera code is part of `libtranslation_layer_main.so` + `api-impl.jar`; no
new dependency, no new package. Two ways in:

* **Full click build** — bump the packaging repo's `atl-touch` submodule and run
  `clickable build`, then `clickable install`. Slow (qemu), but produces a
  shippable click.
* **Hot deploy** (minutes, when only atlas changed) — rebuild atlas in the arm64
  container and copy the two halves over the installed click:

  ```sh
  # on the device: both halves together, or JNI and Java go out of sync
  D=/opt/click.ubuntu.com/<pkg>/current/usr/lib/java/dex/android_translation_layer
  sudo mv $D/oat/arm64/api-impl.odex $D/oat/arm64/api-impl.odex.bak   # stale AOT wins otherwise
  sudo cp api-impl.jar $D/api-impl.jar
  sudo cp libtranslation_layer_main.so $D/natives/libtranslation_layer_main.so
  ```

## 2. Enable the camera

The subsystem is off unless `ATL_UGLY_ENABLE_CAMERA` is set. Add it and the
backend pin next to the other `ATL_*` exports in the click's launcher script:

```sh
export ATL_UGLY_ENABLE_CAMERA=1
export ATL_CAMERA_BACKEND=hybris
```

To run an APK of your own instead of the packaged app, copy the launcher, point
its `exec` line at the APK, and start it over ssh:

```sh
XDG_RUNTIME_DIR=/run/user/32011 \
DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/32011/bus \
WAYLAND_DISPLAY=wayland-0 DESKTOP_FILE_HINT=lomiri \
    /home/phablet/camtest.sh 2>&1 | tee /home/phablet/camtest.log
```

`DESKTOP_FILE_HINT=lomiri` is what lets a shell-launched process open a Wayland
surface. If the screen looks frozen, it is probably off — check
`/sys/class/backlight/panel0-backlight/bl_power` and wake it with
`gdbus call --system -d com.canonical.Unity.Screen -o /com/canonical/Unity/Screen -m com.canonical.Unity.Screen.keepDisplayOn`.
Taps cannot be injected on the device, so a human has to touch the buttons.

Launched normally through `lomiri-url-dispatcher` instead, the logs go to
`journalctl --user -u "lomiri-app-launch--application-click--<pkg>_<app>_<ver>--.service"`.

## 3. What a good run looks like

```
Camera hybris: loaded libcamera.so.1
Camera: using backend 'hybris'
Camera hybris: opened camera 0 (12 preview sizes, 20 picture sizes)
Camera hybris: preview started (1280x720 @ 30000/1000 fps)
Camera hybris: first frame (1280x720, 1382400 bytes)
```

An app previewing into a `SurfaceTexture` gets two more lines, the second of
which is the texture path's proof of life:

```
Camera hybris: preview texture fast path on GL texture 3
Camera hybris: first preview texture frame
```

and, after a capture:

```
Camera hybris: captured a picture, 2481234 bytes of JPEG
```

Sizes and counts differ per device; the *shape* is the point. The front camera
must produce the same block for camera 1.

Failure modes and their first line:

| line | meaning |
| --- | --- |
| `Camera: disabled (set ATL_UGLY_ENABLE_CAMERA=1 to enable)` | the gate is off |
| `Camera hybris: dlopen(libcamera.so.1) failed: …` | libhybris not installed / not in the loader path |
| `… is not the libhybris camera compat layer (missing android_camera_*)` | the freedesktop libcamera got picked up instead — set `ATL_CAMERA_HYBRIS_LIB` to the hybris one |
| `Camera: backend 'hybris' requested but the libhybris camera compat layer is unavailable` | the above, and the backend refused to fall back to gst (by design) |
| `Camera hybris: failed to connect to camera N` | the compat layer reached the HAL and it said no — check the android container |
| `Camera: using backend 'gst'` on a device | auto-selection did not find the compat layer; you are about to test videotestsrc |
| opened, but no `first frame` | HAL is not delivering — see pitfalls below |

## 4. Checking without looking at the screen

`ATL_CAMERA_DUMP_FRAMES=/home/phablet/camframes` writes `frame-count` every
frame and `frame-%06d.png` every 30th, in **every** backend. `scp` one back and
look at it — that separates "the HAL gives us frames" from "compositing is
broken".

## 5. Environment variables

| var | meaning |
| --- | --- |
| `ATL_UGLY_ENABLE_CAMERA=1` | **required**; without it every app sees zero cameras |
| `ATL_CAMERA_BACKEND=hybris\|gst\|none` | default: hybris if its library loads, else gst. `hybris` never falls back |
| `ATL_CAMERA_HYBRIS_LIB=<soname or path>` | override `libcamera.so.1` |
| `ATL_CAMERA_GST_SRC="<gst description>"` | gst backend source, default `videotestsrc is-live=true`. `v4l2src` on a laptop |
| `ATL_CAMERA_DUMP_FRAMES=<dir>` | periodic PNG frame dump (see above) |
| `ATL_MEDIA_FOLDER=<dir>` | stops `ATLMediaContentProvider` popping its folder picker when an app queries MediaStore at startup |

## 6. Known Halium / device pitfalls

* **The binder thread pool is not your problem here.** The compat layer calls
  `ProcessState::self()->startThreadPool()` itself
  (`libhybris/compat/camera/camera_compatibility_layer.cpp`). That is the main
  difference from the camera2 NDK route, where *not* starting it gives a black
  screen and `requestStreamBuffer err:-110`.
* **`libcamera.so.1` is an ambiguous soname** (freedesktop libcamera uses it
  too). ATL probes for `android_camera_*` symbols and refuses a library that
  lacks them; `ATL_CAMERA_HYBRIS_LIB` picks the right one explicitly.
* **The android container must be up** — the compat layer talks to the Android
  camera service over binder. `systemctl status lxc@android`, and libhybris'
  `hybris/tests/test_camera` is the ATL-independent way to prove the HAL works
  at all.
* **camera2 is a zero-camera stub on purpose.** Apps that insist on camera2 will
  report no camera. The route for it is AImageReader + `AHardwareBuffer` →
  `EGLImageKHR` → `GL_TEXTURE_EXTERNAL_OES`, plus the halium patch that makes
  `CameraServiceProxyWrapper::isCameraDisabled` return false when the proxy
  binder is null ("Camera disabled by device policy" otherwise).
* **Orientation**: phone sensors are mounted at 90°/270°. The framework only
  stores `setDisplayOrientation` and forwards it; an app that does not call it
  gets a sideways preview, which is correct AOSP behaviour, not a bug.
* **Focus modes are not enumerated by the compat layer**, so
  `getSupportedFocusModes()` is what its `AutoFocusMode` enum can express, and
  focus callbacks are always reported as success (the layer has no failure
  message).
* **AppArmor**: an unconfined click needs no camera policy group. A confined one
  would need `"policy_groups": ["camera"]`, which has never been tested with the
  hybris path.
* **A camera with no preview target delivers nothing at all.**
  `android_camera_set_preview_texture()` is the compat layer's only
  `setPreviewTarget()` call, and without one `startPreview()` succeeds, reports
  no error and configures no streams -- the software `on_preview_frame_cb`
  frames stop too, not just the texture ones. So `setPreviewTexture()` arms the
  texture straight away (look for `Camera hybris: preview texture fast path on
  GL texture N` *before* `preview started`) rather than waiting for the first
  `updateTexImage()`, which would never come.
* **`setPreviewDisplay()` alone still gets no frames on this backend.** The
  SurfaceView preview path is a pure consumer of the NV21 callback and hands the
  HAL no target, so an app that only calls `setPreviewDisplay()` hits the case
  above. Only the `setPreviewTexture()` route works today.

## 7. Report back

Worth recording for each device: chipset/Halium version, the number of cameras,
the preview/picture sizes chosen, whether `first frame` appeared and after how
long, whether a capture produced a correctly-sized JPEG, whether the front
camera works, and any line from the failure table above.
