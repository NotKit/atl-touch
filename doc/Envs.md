The following environment variables are recognized by the main executable:

`JDWP_LISTEN=<port>` - if set, art will listen for a jdb connection at `<port>`

---

`RUN_FROM_BUILDDIR=` - if set, will search for `api-impl.jar` and `libtranslation_layer.so` in current working directory

                       (may need `LD_LIBRARY_PATH=.` as well for `libandroid.so.0`)

---

`ANDROID_APP_DATA_DIR=<path>` - if set, overrides the default path of `~/.local/android_translation_layer` for storing app data

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

`ATL_CAMERA_BACKEND=<gst|hybris|none>` - camera backend; default is hybris if its library loads, else gst.
                                         `hybris` never falls back to gst, `none` reports zero cameras.

---

`ATL_CAMERA_HYBRIS_LIB=<soname or path>` - library the hybris backend dlopens, default `libcamera.so.1`

---

`ATL_CAMERA_GST_SRC=<gstreamer description>` - source of the gst camera backend, default `videotestsrc is-live=true`
                                               (e.g. `v4l2src` for a real webcam)

---

`ATL_CAMERA_DUMP_FRAMES=<dir>` - if set, the active camera backend writes a frame counter and every 30th
                                 preview frame as a PNG into `<dir>`; see `doc/CameraDevice.md`

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
