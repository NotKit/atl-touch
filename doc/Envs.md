The following environment variables are recognized by the main executable:

`JDWP_LISTEN=<port>` - if set, art will listen for a jdb connection at `<port>`

---

`RUN_FROM_BUILDDIR=` - if set, will search for `api-impl.jar` and `libtranslation_layer.so` in current working directory

                       (may need `LD_LIBRARY_PATH=.` as well for `libandroid.so.0`)

---

`ANDROID_APP_DATA_DIR=<path>` - if set, overrides the default path of `~/.local/android_translation_layer` for storing app data

---

`ATL_ANDROID_ROOT=<path>` - where a Halium device's Android system image is mounted, default `/android`.
                           The shared Java libraries an app declares with `<uses-library>` are resolved
                           against `<root>/<partition>/etc/permissions/*.xml` and the jars named there go
                           on the app's class loader before any app code runs. A desktop has no such
                           tree, so nothing resolves and every entry is logged as not provided.

---

`ATL_SDK_INT=<level>` - the SDK level ATL claims to the app (`Build.VERSION.SDK_INT`, `ro.build.version.sdk`).
                        Default 9. `--sdk-int <level>` on the command line does the same thing and wins over
                        this variable, as does `-X "-DBuild.VERSION.SDK_INT=<level>"`. Apps have no code path
                        for a level they were never built against, so this is per app, not a global setting:
                        Google Camera needs 36, most apps are happier at 9 or 28.

---

`ATL_RESOURCES_SDK_INT=<level>` - the level used to pick the app's `-vNN` resource buckets
                                  (`Build.VERSION.RESOURCES_SDK_INT`), default the same as `ATL_SDK_INT`.
                                  Setting it lower than `ATL_SDK_INT` gets an app's older resources with
                                  its modern code paths; the mismatch is logged, and it can crash an app
                                  that assumes the two agree.

---

`ATL_SDK_RELEASE=<version>` - the user-visible Android version (`Build.VERSION.RELEASE`, e.g. `16`).
                              Default: the release that shipped with `ATL_SDK_INT`.

---

`ATL_SDK_CODENAME=<codename>` - `Build.VERSION.CODENAME`, default `REL` (a release build). Anything else
                                means a preview platform, and apps whose manifest names a different
                                codename will then refuse to be parsed.

The four variables above, and `--sdk-int`, are carried into the desktop entry `--install` writes, so an
app installed with a level keeps it when it is launched from its icon. A click sets them in its launcher
script instead (`doc/CameraClickPackaging.md`).

---

`ATL_DISABLE_WINDOW_DECORATIONS=` - if set, window decorations will be disabled; 
                                    this is useful for saving screen space on phone screens

---

`ATL_UGLY_ENABLE_LOCATION=` - if set, apps will be able to get location data using the relevant android APIs. (TODO: use bubblewrap)  

---

`ATL_UGLY_ENABLE_MICROPHONE=` - if set, apps will be able record microphone audio using the relevant android APIs. (TODO: use bubblewrap)

---

`ATL_UGLY_ENABLE_CAMERA=` - if set, apps will be able to use `android.hardware.Camera`;
                            without it `getNumberOfCameras()` is 0 and `open()` fails. (TODO: use bubblewrap)

---

`ATL_CAMERA_BACKEND=<gst|camera2ndk|hybris|replay|none>` - camera backend; default is the first whose libraries
                                         load: camera2ndk, then hybris, then gst. `camera2ndk` (the device's
                                         own Android camera2 stack through libhybris) is the only backend
                                         that serves `android.hardware.camera2` on a device; `hybris` is
                                         Camera1 only. A backend named here never falls back to another one,
                                         `none` reports zero cameras. `replay` is a recording as a camera,
                                         see `ATL_CAMERA_REPLAY` and `doc/CameraRecording.md`.

---

`ATL_CAMERA_HYBRIS_LIB=<soname or path>` - library the hybris backend dlopens, default `libcamera.so.1`

---

`ATL_CAMERA_GST_SRC=<gstreamer description>` - source of the gst camera backend, default `videotestsrc is-live=true`
                                               (e.g. `v4l2src` for a real webcam)

---

`ATL_CAMERA_GST_SRC_1=<gstreamer description>` - if set, adds a second, front-facing gst camera with this source,
                                                 so an app's camera-switch path has somewhere to switch to

---

`ATL_CAMERA_ZERO_COPY=0` - turns off the camera2ndk backend's zero-copy preview, so preview frames are
                           copied out of the HAL's buffers into NV21 and uploaded to the app's texture by
                           the CPU. On by default; it only exists at all where the EGL is an Android one
                           (a device), and it is the frame rate at 1080p - see `doc/CameraDevice.md`

---

`ATL_CAMERA_DUMP_FRAMES=<dir>` - if set, the active camera backend writes a frame counter and every 30th
                                 preview frame as a PNG into `<dir>`; see `doc/CameraDevice.md`.
                                 Only the CPU path can dump, so setting this keeps the NV21 copies alive
                                 even when the zero-copy preview is running

---

`ATL_CAMERA_DUMP_METADATA=<file>` - if set, each camera's camera2 characteristics are appended to `<file>`
                                    in readable form the first time an app reads them (tag names, ids,
                                    types and values; vendor tags show as `<vendor tag>`)

---

`ATL_CAMERA2_NARROW_STREAMS=1` - cuts a real HAL's characteristics down to the formats ATL's camera2 can
                                 actually deliver: the stream-configuration, min-frame-duration and
                                 stall-duration lists keep only PRIVATE/YUV_420_888/JPEG, the depth, HEIC
                                 and JPEG/R lists go, and the RAW and DEPTH_OUTPUT capabilities with them.
                                 Off by default, because it makes Google Camera *worse*: libgcam builds its
                                 own camera list out of the RAW configurations and refuses to open a camera
                                 without them. It is for an app that configures a stream ATL never fills.

---

`ATL_CAMERA_RECORD=<dir>` - if set, every camera2 session is kept in a ring in memory and a burst around
                            each still capture is written to `<dir>/capture-*.atlcam`, frames, results,
                            requests and characteristics together. See `doc/CameraRecording.md`

---

`ATL_CAMERA_RECORD_MB=<n>` - how much of that ring there is, default 512. It bounds the pre-roll: about
                             32 full-sensor RAW10 frames per 512 MiB

---

`ATL_CAMERA_RECORD_AFTER=<n>` - frames to keep recording after a trigger before the file is written,
                                default 12

---

`ATL_CAMERA_RECORD_TRIGGER=<oneshot|intent|manual>` - what starts a burst: any non-repeating request
                                                      (default), only one whose `CONTROL_CAPTURE_INTENT`
                                                      is `STILL_CAPTURE`, or only the trigger file
                                                      (`touch <dir>/trigger`, which works in every mode)

---

`ATL_CAMERA_RECORD_PREVIEW=<px>` - long edge of the downsampled NV21 reference kept for a PRIVATE
                                   viewfinder stream, default 640; 0 records no pixels for it at all

---

`ATL_CAMERA_RECORD_LEVEL=<n>` - zstd level for the writer, default 1. Raw Bayer barely compresses; the
                                level is there for the metadata-heavy and YUV cases

---

`ATL_CAMERA_REPLAY=<file>` - the recording the `replay` backend serves. The whole file is read into memory

---

`ATL_CAMERA_REPLAY_SPEED=<factor>` - how fast the recorded intervals are played back, default 1.0

---

`ATL_MEDIA_FOLDER=<dir>` - the folder `ATLMediaContentProvider` answers MediaStore queries from. Without it
                           an app that queries MediaStore at startup gets the folder picker.

---

`ATL_DUMP_HIERARCHY=1` - prints every View in every window with its class, id, bounds, measured size,
                         visibility and (for a TextView) its text. It is the substitute for a screenshot
                         on a device where nothing can capture one; accumulate the parents' left/top for
                         absolute coordinates. Enormous - millions of lines for one launch - so turn it on
                         only for the run that needs it.

---

`ATL_UGLY_ENABLE_WEBVIEW=` - if not set, WebView will be stubbed as a generic View; this will avoid
                             wasting resources on WebViews which are only used for fingerprinting and ads

                             (set this for apps that use WebView for it's intended purpose)

---

`ATL_FORCE_FULLSCREEN` - if set, will open the app window at the size of the primary
                         monitor's work area on start;
                         this is useful for saving screen space on phone screens,
                         as well as making apps that can't handle arbitrary screen dimensions
                         for some reason happier (may need gamescope if the hardcoded resolution
                         doesn't match your device)

                         the window is sized to the monitor, not made a fullscreen
                         toplevel: that aborts Lomiri when the app is restored from
                         minimised

---

`ATL_IS_AUTOMOTIVE` - if set, when an app checks if it's running in a vehicle, ATL will return true.

---

`ATL_IS_TELEVISION` - if set, when an app checks if it's running on a television, ATL will return true.

---

`ATL_IS_WATCH` - if set, when an app checks if it's running on a watch, ATL will return true.

---

`ATL_SKIP_NATIVES_EXTRACTION` - if set, natives will not be extracted automatically;
                                it's already possible to replace a native lib, but removing it entirely will normally result
                                in it getting re-extracted, which would prevent you from replacing libs with native ones
                                (since bionic_translation linker considers everything inside the app's lib dir non-native)

---

`ATL_DIRECT_EGL` - if set, SurfaceViews will be mapped directly to a Wayland subsurface or X11 window
                   instead of using GtkGraphicsOffload. This might be beneficial for CPU usage and rendering latency,
                   but (on Gtk < 4.22 and on X11) does not allow the application to render other Views on top of
                   the SurfaceView

                   On Gtk >= 4.22, we punch a hole through the Gtk scene graph and (on Wayland) put the native surface below the Gtk window.

---

`ATL_VALIDATE_CERTS` - if set, the signing certificate of the APK file will be validated on startup. This adds a few extra seconds to the startup time for large APKs.

---

`ATL_APK_SPLITS` - a `:`-separated list of split APKs to load alongside the base APK
                   given on the command line. The other way in is to pass a directory
                   holding `base.apk` and the `split_<name>.apk` files, which is the
                   layout the package installer leaves behind; see
                   [App bundles](AppBundles.md).

---

`ATL_SURFACE_MODE=<subsurface|none>` - how a `SurfaceView`'s content reaches the screen.
                                       `subsurface` (default) gives it a Wayland sub-surface of its own,
                                       so a GL producer presents straight to the compositor;
                                       anything else keeps the CPU read-back path
                                       (`Surface.postFrame` into the scene), which cannot serve a GL producer.

---

`ATL_SURFACE_CHROME=<subsurface|toplevel|none>` - where ATL's own drawing goes when a `SurfaceView` exists;
                                                  see `doc/SurfaceViewCompositing.md`.
                                                  `subsurface` (default) puts the whole scene - views, dialogs,
                                                  popups - in a second sub-surface created after the content one,
                                                  which is the only ordering that works on Mir.
                                                  `toplevel` is the old punch-hole behaviour (needs
                                                  `wl_subsurface.place_below`, i.e. wlroots).
                                                  `none` is neither, and only exists to reproduce Mir's
                                                  stacking on a desktop compositor.

---

`ATL_SURFACE_CHROME_ALPHA=0` - do *not* ask GLFW for a framebuffer with an alpha channel. ATL asks by
                               default (Wayland only), because wherever the EGL driver has no
                               `EGL_EXT_present_opaque` GLFW skips every config that has an alpha channel,
                               and then the chrome sub-surface cannot carry a `SurfaceView`'s hole - the
                               hole comes out opaque black and the app's own frames are never seen. That is
                               the case on Ubuntu Touch (hybris EGL), where the hint is what makes a dialog
                               over a `SurfaceView` work at all. GLFW stops declaring the toplevel's opaque
                               region once the hint is set, so ATL declares it itself; see
                               `ATL_SURFACE_OPAQUE_REGION`. This is the way back to the pre-2026-08-14
                               behaviour if some driver dislikes the alpha config.

---

`ATL_SURFACE_OPAQUE_REGION=0` - do not declare the toplevel's opaque region. ATL declares it (the whole
                                window, or the window minus the `SurfaceView` holes in
                                `ATL_SURFACE_CHROME=toplevel` mode) because GLFW stops doing it for a window
                                that asked for a transparent framebuffer. Only useful for measuring what
                                that declaration is worth, or to escape a compositor that mishandles it.
                                Note that the rectangle is the *framebuffer* size, which is not always the
                                size of the buffer the toplevel attaches: a window created at 960x540 (the
                                default - see `-w`/`-h`) and then configured to the display size declares
                                the configured rectangle over a buffer still at the created size. Since
                                2026-08-17 that lasts one commit rather than the life of the window - the
                                toplevel is re-attached at the new size on the same tick - and the surplus
                                is what the protocol says the compositor drops. Measured on Mir 1.8.3, an
                                oversized region changes nothing in the output either;
                                `doc/SurfaceViewCompositing.md` has both measurements and their limits.

---

`ATL_DEBUG_RESIZE=<seconds>[:<w>x<h>]` - once, `<seconds>` after the first frame tick, ask the compositor
                                to resize this window to `<w>x<h>` (default 600x800). A diagnostic, not a
                                feature: a phone shell configures a window when it maps and never again, so
                                without this there is no way to exercise what ATL does when a
                                `xdg_toplevel.configure` arrives at a *new* size - which is the case the
                                chrome/toplevel split has to get right and the one no run had ever taken.
                                `xdg_toplevel` has no "resize me" request; what this sends is a size *limit*
                                (`glfwSetWindowSizeLimits`, min = max = the asked size), which a compositor
                                answers with a fresh configure. Measured on Mir 1.8.3, where `set_max_size`
                                alone is enough; a compositor that ignores size hints will not resize.
                                Pair it with `ATL_DEBUG_CHROME=1`, which then logs each new framebuffer size
                                and every toplevel commit.
                                The limit is set once and never unset, but it does not pin the window: in a
                                run on 2026-08-17 the shell rotated the phone and configured the toplevel
                                back to 1080x2349 948 ms after the 600x800 it had just granted. That is a
                                second genuine resize for free, and worth reading the trace for.
