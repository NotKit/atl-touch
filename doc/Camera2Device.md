# camera2 bring-up on an Ubuntu Touch device

The camera2 twin of `doc/CameraDevice.md`: that file is Camera1 through the
libhybris compat layer, this one is `android.hardware.camera2` through the
device's own Android camera2 NDK (`libcamera2ndk.so`), also through libhybris.
The desktop loop proves the API against GStreamer; this proves it against a real
HAL.

Everything here was measured on a Pixel 9 Pro ("caiman") running Ubuntu Touch
24.04. Packaging: `doc/CameraClickPackaging.md`.

## 0. The rule that comes before everything

Ask the host camera service how many cameras it has *before* blaming ATL:

```sh
dumpsys media.camera | head
```

`Number of camera devices: 0` means the HAL provider crashed, and **every**
symptom of that looks like an ATL bug: zero cameras, `openCamera` failures,
`onDisconnected`, `camera device error 4`. Recovery on this port ends at
`sudo reboot`. Never `kill -9` the camera provider (it wedges the Mali OpenCL
context; its own watchdog uses `kill -6`). Numbers measured while the provider
is in its crash-restart cycle are not measurements.

## 1. Get the runtime onto the device

Either ship a click (`doc/CameraClickPackaging.md`) or rsync a self-contained
tree to `/home/phablet/atl`: the executable, `api-impl.jar`, `libcore-shim.jar`,
`libtranslation_layer_main.so`, the NDK forwarders and the patched shim linker.
Those share globals, so they are redeployed together or the run is half old
code.

Whatever launches ATL has to export the libhybris environment a Lomiri session
has and a plain ssh does not:

```sh
export LD_PRELOAD=libhybris-common.so.1        # glibc's static-TLS surplus, see §3
export HYBRIS_LD_LIBRARY_PATH=<runtime>/usr/lib/hybris-stubs:/system/lib64:/vendor/lib64:/odm/lib64
```

`hybris-stubs` holds an empty `libandroid_runtime.so`; it has to win over the
real one, which aborts in its own constructors in a host process. A headless
check can run a class under `dalvikvm` instead of launching an APK — the camera
backend needs no window, only `ATL_TEST_MAINLOOP=1` when it waits on an ATL
callback.

## 2. Run something

Open Camera in camera2 mode is the cheapest end-to-end run, and the recipe to
copy for an app of your own:

```sh
nohup env ANDROID_APP_DATA_DIR=/home/phablet/oc-appdata ATL_SDK_INT=23 \
    ATL_UGLY_ENABLE_CAMERA=1 ATL_CAMERA_BACKEND=camera2ndk \
    ATL_CAMERA_DUMP_FRAMES=/home/phablet/oc-frames \
    /home/phablet/atl/android-translation-layer /home/phablet/opencamera.apk \
    -l net.sourceforge.opencamera.TakePhoto >/home/phablet/oc.log 2>&1 &
```

Two things in there are the whole trick:

* **Taps cannot be injected on this device.** Open Camera's `TakePhoto`
  shortcut activity sets a static flag and starts `MainActivity`, and
  `Preview.cameraOpened()` takes the picture by itself — so the shutter never
  has to be pressed. An app without such an entry point needs a human.
* **The app's own preferences are seeded before the launch**, in
  `<data>/<apk name>_/shared_prefs/`, which is how Open Camera is put in
  camera2 mode (`preference_camera_api=preference_camera_api_camera2`) and its
  torch turned on (`flash_value_0=flash_torch`).

## 3. What a good run looks like

```
Camera camera2ndk: loading the Android camera2 libraries, HYBRIS_LD_LIBRARY_PATH=…/hybris-stubs:/system/lib64:/vendor/lib64:/odm/lib64:/vendor/lib64/egl
Camera camera2ndk: loaded libcamera2ndk.so and libmediandk.so
Camera camera2ndk: 2 camera(s)
Camera: using backend 'camera2ndk'
Camera camera2ndk: camera '0' has 138 metadata entries (32 vendor tags, read by id), 103 request and 123 result keys
Camera camera2ndk: opened camera '0' (32 preview / 32 picture sizes)
Camera camera2ndk: session with 3 HAL stream(s):
Camera camera2ndk:   stream 0: 1280x960 PRIVATE x11
Camera camera2ndk:   stream 1: 4080x3072 RAW10 x37, physical camera 2
Camera camera2ndk:   stream 2: 1280x960 YUV_420_888 x52
camera2: session configured (3 output(s), HAL streams, largest 4080x3072)
Camera camera2ndk: repeating request 1: template 1, 3 target(s), streams 0x7, sequence 0
Camera camera2ndk: first buffer on stream 0 (1280x960 format 0x22, 0 plane(s), gralloc)
Camera camera2ndk: first buffer on stream 1 (4080x3072 format 0x25, 1 plane(s), gralloc)
SurfaceTexture: camera buffers bound to GL texture 3 as they are (1280x960, no CPU copy)
```

That is a camera2 session: one HAL stream per output Surface, in the app's
own format (section 8). A Camera1 app (`android.hardware.Camera`) drives the
older one-stream session instead, and its good run reads:

```
Camera camera2ndk: session with a 4080x3072 YUV stream
Camera camera2ndk: preview started 4080x3072
Camera camera2ndk: first frame (4080x3072)
Camera camera2ndk: zero-copy preview fast path on GL texture 3 (4080x3072)
```

(the session line gains ` and a WxH JPEG stream` when the app configured a JPEG
output as well, and the zero-copy line is absent whenever anything still needs
the CPU copies — `ATL_CAMERA_DUMP_FRAMES` is one such thing)

and, for a still:

```
Camera camera2ndk: captured a picture, 985996 bytes of JPEG
```

and at teardown:

```
Camera camera2ndk: closed after 331 frames, 331 of them copied to the CPU
```

That last line is the second witness for the frame counter: `N frames` is what
the HAL delivered, `M of them copied to the CPU` is how many went through NV21
(everything, when `ATL_CAMERA_DUMP_FRAMES` is set; nothing, on the zero-copy
preview path with no other consumer).

The numbers are caiman's — 2 cameras, both `FULL`, 138/120 metadata entries,
32/23 vendor tags. The **shape** is the point.

### Failure modes and their first line

| line | meaning |
| --- | --- |
| `dumpsys media.camera` says `Number of camera devices: 0` | the HAL provider crashed. Not an ATL bug and not fixable from the host — reboot. Everything below is worthless until this reads the device's real camera count |
| `Camera: disabled (set ATL_UGLY_ENABLE_CAMERA=1 to enable)` | the gate is off; every app sees zero cameras |
| `Camera camera2ndk: 0 camera(s)` | the backend loaded and the *service* has nothing to give it — the same crashed provider, seen from inside ATL |
| `Camera camera2ndk: no libhybris android loader in this process (android_dlopen missing)` | nothing preloaded `libhybris-common`, so there is no Android loader (§1) |
| `…libhybris-common.so.1: cannot allocate memory in static TLS block` | glibc's dlopen TLS surplus is smaller than libhybris' 4224-byte initial-exec segment. Affects every glibc process on the device — `LD_PRELOAD` it (§1) |
| the process dies inside `loading the Android camera2 libraries` (from ART: `free(): invalid pointer`) | `libcamera2ndk.so` pulled in the real `libandroid_runtime.so`, which aborts in its own constructors in a host process. `HYBRIS_LD_LIBRARY_PATH` must point at the empty stub in `usr/lib/hybris-stubs` (§1) |
| `Camera camera2ndk: libbinder_ndk.so has no ABinderProcess_startThreadPool` | the binder thread pool never starts, the HAL cannot dequeue this process's buffers, and every frame times out (`requestStreamBuffer err:-110`). A camera2 session that opens and then delivers nothing is almost always this |
| `Camera: backend 'camera2ndk' requested but the Android camera2 stack is unavailable` | a named backend never falls back. Drop `ATL_CAMERA_BACKEND` to let auto-selection try `hybris`, then `gst` |
| `Camera: using backend 'hybris'` and an app that reports no camera2 cameras | correct: the Camera1 compat layer has no metadata, so the `hybris` backend serves Camera1 only. camera2 needs `camera2ndk` |
| `Camera camera2ndk: openCamera('0') failed (…)` / `onDisconnected` / `camera device error 4` a second in | another client still holds the camera. Usually a previous ATL process that never finished closing — a process wedged in `close()` keeps the camera open, and the *next* run gets the error. Find it by pid (ATL rewrites its command line, so `pgrep -f` will not) |
| `camera device error 4` immediately after `first frame`, on a launch that opened and configured normally | caiman's own flake, and it is not the size of the stream: measured 4 times in 8 launches of the same app, always right after the first frame, always on a launch following one that was killed while streaming. The provider restarts each time — the `ICameraProvider/internal/0-N` index in `dumpsys media.camera` counts up — and the next launch is fine, so a check worth trusting retries once |
| `camera2: backend 'camera2ndk' rejected a WxH stream` | the size is not in the HAL's stream-configuration list for `YUV_420_888`. ATL sizes the single stream to the largest output surface (see §6) |
| `camera2: output surface is backed by neither a SurfaceTexture nor an ImageReader` | the app handed the session a Surface ATL has no sink for. Every session output is one of those two |
| `Camera camera2ndk: this EGL has no EGL_ANDROID_get_native_client_buffer` | no zero-copy preview here (a desktop EGL, or a Mesa fallback on the device). Frames still arrive, through the CPU, at roughly half the rate |
| a camera2 preview runs but no `zero-copy preview fast path` line | either the row above, or the app bound its texture name before the session was configured, which fixed the object as a 2D texture |
| frames flow but every dumped PNG is black | the lens is looking at a dark room. Measured on caiman with everything else identical: with the torch on a dumped frame has mean brightness 107, without it 0.0. Light the scene before blaming the pipeline |
| `GLFW error 0x10006: EGL: Failed to find support for OpenGL` | harmless on this device — GLFW probes desktop GL first and then takes GLES. `ATLWindow: GPU rendering (Ganesh) on Mali-G715` on the next lines is the real answer |

## 4. Checking without looking at the screen

* **Frames.** `ATL_CAMERA_DUMP_FRAMES=<dir>` writes `frame-count` every frame
  and `frame-%06d.png` every 30th, in every backend. `scp` one back and look at
  it: that separates "the HAL gives us frames" from "compositing is broken". The
  newest PNG can be half-written if the process was killed mid-dump — judge the
  one before it, and say which one you judged.
* **Metadata.** `ATL_CAMERA_DUMP_METADATA=<file>` appends each camera's
  characteristics the first time an app reads them:

  ```
  == camera 0 characteristics: 138 entries ==
  android.sensor.info.activeArraySize (0x00030004) int32[4] = 0 0 4080 3072
  android.control.aeAvailableTargetFpsRanges (0x00010005) int32[8] = 15 15 15 30 …
  <vendor tag> (0x80010000) byte[1] = 1
  ```

  Tag names come from the generated table, so a Pixel-only vendor tag shows as
  `<vendor tag>` with its id and stays opaque rather than throwing. This is how
  "the app asked for a key the HAL does not have" gets answered without a
  debugger.
* **Photos** land in the app's own data dir under `ANDROID_APP_DATA_DIR`
  (Open Camera: `<data>/<apk name>_/DCIM/OpenCamera`).
* **A whole capture.** `ATL_CAMERA_RECORD=<dir>` keeps the session in a ring and
  writes the burst around each still - frames, results, requests and the
  camera's characteristics - as one file, which
  `ATL_CAMERA_BACKEND=replay` then serves back to an app on a desktop. That is
  the short way to argue about a merge without the phone in the loop;
  `doc/CameraRecording.md`.
## 5. Environment variables

| var | meaning |
| --- | --- |
| `ATL_UGLY_ENABLE_CAMERA=1` | **required**; without it every app sees zero cameras |
| `ATL_CAMERA_BACKEND=camera2ndk` | the device's own camera2 stack, and the only backend that serves `android.hardware.camera2` on a device. Default is the first that loads (camera2ndk, then hybris, then gst); a named backend never falls back |
| `ATL_CAMERA_ZERO_COPY=0` | turn the zero-copy preview off (HAL buffer → `EGLImage` → external texture, for a Camera1 preview texture and a camera2 SurfaceTexture output alike) and copy every frame through the CPU instead |
| `ATL_CAMERA2_STREAMS=0` | emulate every camera2 session off one NV21 stream, even on a backend with HAL streams of its own (section 8) — the bisect lever between the two paths |
| `ATL_CAMERA_DUMP_FRAMES=<dir>` | frame counter + every 30th frame as a PNG. Keeps the NV21 copies alive even on the zero-copy path, so it costs frame rate — debug only, never in a release click |
| `ATL_CAMERA_DUMP_METADATA=<file>` | append each camera's characteristics the first time they are read, and every submitted request |
| `ATL_CAMERA2_HIDE_VENDOR_TAGS=<substr>[,…]` | drop vendor tags whose name contains a substring from the characteristics and key lists; `!substr` keeps one. Google Camera needs `com.google` |
| `ATL_CAMERA2_HIDE_PHYSICAL_IDS=<id>[,…]` | drop physical camera ids from a logical camera's list and refuse their characteristics. Measured on caiman with Google Camera: hiding the ultrawide (`3`) makes the app take its virtual sibling (`9`) instead, and hiding both makes its camera setup NPE - so not the answer there |
| `ATL_CAMERA2_DROP_REQUEST_TAGS=<substr>[,…]` | request entries whose tag name contains a substring never reach the HAL. Inert for Google Camera's lens switches, which the Pixel HAL decides on its own (`vendor.camera.debug.st_3a_overwrite_enable`) |
| `ATL_SDK_INT=<level>` | the level ATL claims. Per app: Open Camera runs at 23, Google Camera needs 36 |
| `ATL_CAMERA_RECORD=<dir>` | record the camera2 session and write a burst around each capture; `ATL_CAMERA_BACKEND=replay` with `ATL_CAMERA_REPLAY=<file>` plays one back. `doc/CameraRecording.md` |
| `ATL_TEST_MAINLOOP=1` | headless runs only: preload a GLib main loop, needed by anything that waits on an ATL callback |
| `ANDROID_APP_DATA_DIR=<dir>` | the app data dir; a fresh one is what makes repeated runs start from identical first-launch state |
| `ATL_MEDIA_FOLDER=<dir>` | stops `ATLMediaContentProvider` popping its folder picker when an app queries MediaStore at startup |

`doc/Envs.md` is the full list.

## 6. Device pitfalls specific to camera2

* **The binder thread pool is ours to start.** The Camera1 compat layer calls
  `ProcessState::self()->startThreadPool()` itself; the camera2 NDK does not, so
  ATL loads `libbinder_ndk.so` and starts it. Without it the HAL cannot dequeue
  buffers from this process and every frame times out — an opened, configured,
  frameless session.
* **One backend pipeline, one stream size.** ATL sizes the session's single
  stream to the *largest* output surface, and an `ImageReader` that wants less
  scales the frame down as it converts it. So an app that asks for a full-sensor
  JPEG gets a full-sensor *preview* stream too: on caiman, Open Camera's default
  picture size makes the repeating request 4080x3072. That works (measured: 331
  frames, ~8/s, and a 4080x3072 JPEG), but it is the reason a preview can be
  slower on a device than the same app on the desktop.
* **`AImageReader` callbacks must be counted in and out.** The callback holds an
  `AImage` that belongs to the reader being deleted, and `AImageReader_delete`
  hangs forever if it loses that race. A process stuck in teardown keeps the
  camera open and the next run reads exactly like a flaky HAL.
* **Never call a backend while holding the camera2 device lock.** On a real HAL
  that call becomes a capture-session call that waits for the delivery thread,
  and the delivery thread wants the same lock.
* **`ACameraMetadata_fromCameraMetadata` and `AHardwareBuffer_{from,to}HardwareBuffer`
  cannot be forwarded** to an app's own native code: they read a native pointer
  out of a Java object that under ATL is one of ATL's own bags. They return NULL
  with one log line.
* **The camera-id set is the HAL's, not a Pixel's.** ATL exposes the ids the NDK
  lists ("0" and "1" on caiman) and no logical/physical grouping; an app that
  expects a Pixel's rear camera to be a group of physical ids will say so
  (Google Camera logs `E/CAM_mdb: Invalid number of camera ids: 1`).
* **Vendor tags stay opaque.** Unknown tags keep their id and type and are
  handed back unchanged, so a Pixel-only key never throws — but nothing
  interprets it either.
* **AppArmor**: the runtime used here is a plain tree in `/home/phablet/atl`,
  unconfined. A confined click would need `"policy_groups": ["camera"]`, still
  untested with either camera backend.

## 7. Report back

Worth recording for each device: chipset/Halium version, the
`Camera camera2ndk: N camera(s)` line, the metadata-entry and vendor-tag counts
per camera, the stream size the session settled on, whether `first frame`
appeared and after how long, whether the zero-copy line appeared, whether a
still produced a correctly-sized JPEG, and any row of the failure table above.

## 8. The camera2 session is the HAL's own streams

`android.hardware.camera2` under ATL has the shape of AOSP's camera service
(`Camera3Device` as `CameraDeviceClient` sees it), not a Camera1 preview with
conversions bolted on:

* **A session is a set of output streams, one per app Surface**, each in the
  app's own format and size, with the physical camera id of its
  `OutputConfiguration`. On the camera2ndk backend every stream is an
  `AImageReader` of that format, a session output (a physical one where the app
  said so) and a request target — `camera_backend_camera2ndk.c`, "the stream
  session". The HAL accepts Google Camera's whole set on caiman: a PRIVATE
  viewfinder, a YUV stream and four RAW10 streams on four physical cameras.
* **A request names the streams it targets** and carries the app's settings and
  its request id (the NDK request's user context); the template is the one the
  app's `CONTROL_CAPTURE_INTENT` names.
* **The events are the HAL's**: `onCaptureStarted` with the HAL's frame number
  (the API 33 `captureCallbacksV2`, or a counter of ATL's where the device's
  libcamera2ndk predates them), the result paired to it by
  `SENSOR_TIMESTAMP`, `onCaptureFailed`, and `onCaptureBufferLost` naming the
  output — which a physical camera's stream reports on every frame while
  another lens is the active one, exactly as on Android.
* **Buffers are handed out as they are.** An ImageReader is a consumer of its
  stream (`image_reader.h`): a RAW10 reader gets the sensor's RAW10 plane, a
  YUV reader the HAL's three planes, a PRIVATE reader the gralloc buffer as its
  `HardwareBuffer` — the one Google Camera posts to its viewfinder, which ATL
  then composites from the buffer's own planes. A SurfaceTexture output binds
  the gralloc buffer to the app's external texture with no copy. A buffer holds
  its stream alive until the app releases it, so a reader is never deleted
  under an image.
* **The one-stream backends (gst on a desktop, hybris) get the same session
  emulated** in `camera_streams.c`: the backend's NV21 preview is sized for the
  largest stream and converted into each one — YUV and PRIVATE keep the NV21
  layout, JPEG is encoded, and RAW10/RAW_SENSOR are the frame's luma on a Bayer
  grid, because those backends have no raw data. That fabrication is what made
  Google Camera's pictures magenta while it stood in for the HAL on the device
  too; `ATL_CAMERA2_STREAMS=0` brings it back on purpose. The emulation is also
  the fallback when a HAL refuses a stream combination.

A session can also have **one input stream**, which is what makes it
reprocessable (`InputConfiguration`, `createReprocessableCaptureSession`).
Google Camera builds one when its Pixel vendor tags are visible: a RAW10 input
beside its outputs, so a shutter press sends a frame it already has back
through the HAL's own pipeline. Only a backend with real streams can serve it,
because the buffers going in are the camera's own:

* the input is created inside the same configuration as the outputs and cannot
  be added afterwards, so `native_createInputSurface` only records it and
  `native_configure` carries it down;
* `ImageWriter.queueInputImage(image)` hands the Image's gralloc buffer to
  `ACameraCaptureSession_queueInputBuffer` with no copy, and the app hears
  `OnImageReleasedListener` when the camera gives it back, not before;
* a reprocess request is built from the **real** `ACameraMetadata` of the frame
  being reprocessed, kept in a ring by sensor timestamp, because the bag ATL
  passes up to Java has lost the vendor sections the HAL wants back;
* a reprocess capture carries its input frame's `SENSOR_TIMESTAMP`, so it is
  kept out of the timestamp pairing that gives ordinary results their frame
  numbers — matching it there reports the reprocess against whichever request
  produced the original frame.

The public NDK has no input streams; these entry points come from a Halium
extension to `libcamera2ndk` and are resolved by name, so a device whose
library predates it simply has no reprocessing. The emulated session serves an
input of its own, from the `ImageWriter`'s NV21 queue.

Every camera2 stream is CPU-readable whatever usage the app asked for: the
planes are read there, and ATL composites on the CPU, so a viewfinder buffer
the app posts has to be lockable — without that flag gralloc refuses the read
and the screen stays black.
