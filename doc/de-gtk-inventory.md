# GTK/GDK/GSK usage inventory (M1 de-GTK worklist)

Snapshot taken at fork point, after the launcher (`src/main-executable/main.c`) was
converted from GtkApplication+libportal to plain GApplication/GIO. Numbers are counts
of `gtk_*`/`gdk_*`/`gsk_*`/GTK-type references per file; "include-only" files just
need the `#include` dropped.

Note: nothing calls `gtk_init()` anymore (GtkApplication used to do it implicitly).
Any GTK codepath that still executes must call `gtk_init_check()` lazily first, or be
replaced. Until M1 completes, GTK-dependent features are expected to be broken in the
GApplication launcher; this list tracks them.

## Heavy — rendering/graphics spine (replace with Skia / Wayland)

| File | Refs | Role / replacement |
|---|---|---|
| `src/libandroid/egl.c` | 21 | NDK EGL over GdkSurface; extern `window` global from launcher → Wayland EGL (wl_egl_window) |
| `src/libandroid/native_window.c/.h` | 18+1 | ANativeWindow over GTK → Wayland surface/subsurface |
| `src/libandroid/input.c` | 7 | GDK event translation for NativeActivity → GLFW/Wayland input |
| `src/api-impl-jni/widgets/android_view_SurfaceView.c/.h` | 3+5 | GTK subsurface embedding → wl_subsurface |

(These are the NativeActivity/NDK GL path — only used by native games, currently
stubbed; they come back together in the wl_egl_window bring-up.)

## Medium — host integration (replace with Wayland/DBus/UT services)

| File | Refs | Role / replacement |
|---|---|---|
| `src/api-impl-jni/media/android_media_MediaCodec.c` | 23 | GdkDmabufTexture video frames → dmabuf/EGLImage into Skia/GL. **MediaCodec task**: codec-level backend — libavcodec hwdevice (VAAPI/V4L2 → DRM prime dmabuf) on mainline, Android codecs via libhybris on halium |

MediaPlayer and SoundPool are DONE (GStreamer playbin — see below); MediaCodec
is the remaining media file. Audio goes through playbin's default sink
(PulseAudio/PipeWire); `ATL_MEDIA_AUDIO_SINK` overrides it for headless test
machines. Video in MediaPlayer is currently decode-and-drop (fakesink) until
the MediaCodec/dmabuf rendering path lands.

## Done in M1 so far

- **MediaPlayer + SoundPool over GStreamer** — `android_media_MediaPlayer.c`
  rewritten as a playbin peer (prepare/prepareAsync/start/pause/stop/seek/
  isPlaying/looping/volume/reset/release; bus watch on the GLib loop posts
  prepared/completion/seek-complete/error to Java via `postEventFromNative`);
  `android_media_SoundPool.c` plays samples through transient playbins
  (overlap-safe, loop counts, volume, real `onLoadComplete`). GtkMediaStream
  is gone from both; shared lazy `gst_init` in `media/atl_gst.c`.
- **In-scene file picker + share dialog** — `ATLFilePicker.java` (open/save/
  select-folder on the dialog panel infra) replaced `nativeFileChooser`
  (GtkFileDialog) and `ATLMediaContentProvider`'s folder picker (that JNI file
  is deleted); the ACTION_SEND share dialog is a Java `AlertDialog`
  (Cancel/Copy/Email; Copy → GLFW clipboard as text, Email → the existing
  `org.freedesktop.portal.Email` call, kept as `nativeComposeEmail`).
- `android_content_Context.c` — screen size via `atl_screen_size()` (GLFW
  video mode) instead of GdkMonitor; the GtkSettings dark-theme setter is gone
  (the portal `color-scheme` → `Configuration.uiMode` propagation stays).
- `android_app_Activity.c` — GTK-free; `isInMultiWindowMode` uses
  `atl_window_is_maximized()` (GLFW_MAXIMIZED).
- `util.c` — GTK-free; the `atl_safe_gtk_*` snapshot-safety helpers and
  `util_gtk.h` are deleted (no callers since the AOSP drawable/text lifts).

- **Bitmap / ATLCanvas** — GdkTexture bridge deleted (`atl_skbitmap_to/from_gdk_texture`,
  `atl_canvas_to_gdk_texture`, `Bitmap.getGdkTexture` + the cached `gdk_texture`
  field). Both are GTK-free now that drawables draw through Skia.
- `android_content_ClipboardManager.c` — GLFW clipboard via `atl_window_set/get_clipboard`;
  round-trips (get/has now work, were null-stubs).
- `android_app_NotificationManager.c` — `atl_window_focus` + `g_application_get_default()`.
- `android_app_WallpaperManager.c` — Java-side `Bitmap.compress` to temp PNG + portal.
- `android_view_Window.c`, `audio/android_media_AudioTrack.c`, `jni_cpp.h`,
  `sensors/android_hardware_SensorManager.c`, `location/android_location_LocationManager.c`
  — include-only drops (glib/gio suffice).

## Done at fork point

- `src/main-executable/main.c` — GApplication/GIO only; libportal dynamic-launcher
  replaced by direct `.desktop` install; GTK/GSK VectorDrawable→SVG icon rendering
  dropped (TODO: re-add via Skia).
- `src/api-impl-jni/util.h` — GTK-free; GTK helper declarations moved to `util_gtk.h`.
- `src/api-impl-jni/graphics/android_graphics_drawable_Drawable.cpp`,
  `NinePatchPaintable.c/.h`, `android_graphics_drawable_DrawableContainer.c`,
  `android_graphics_drawable_NinePatchDrawable.cpp` — DELETED. The whole
  drawable stack is now AOSP-13 Java over the Skia Canvas; ninepatch drawing
  lives in `android_graphics_NinePatch.cpp` (no GTK).
