# GTK/GDK/GSK usage inventory (M1 de-GTK worklist)

Snapshot taken at fork point, after the launcher (`src/main-executable/main.c`) was
converted from GtkApplication+libportal to plain GApplication/GIO. Numbers are counts
of `gtk_*`/`gdk_*`/`gsk_*`/GTK-type references per file; "include-only" files just
need the `#include` dropped.

**M1 IS COMPLETE: nothing links GTK anymore** — `libgtk-4`/`libgdk` are gone
from the NEEDED entries of the launcher, `libtranslation_layer_main.so` and
`libandroid.so`. Remaining follow-ups are features, not de-GTK:

- **NDK-GL bring-up** (`src/libandroid/egl.c`, `native_window.c`, `input.c`):
  the GTK code there was dead (stubs) and has been stripped; NativeActivity
  rendering returns with the wl_subsurface + wl_egl_window work (handles from
  GLFW, geometry from SurfaceView, input teed from ATLWindow callbacks).
- **Zero-copy video** (dmabuf → EGLImage into a GL-composited scene) for
  MediaCodec/MediaPlayer; decode is already hw-accelerated where available,
  presentation goes through CPU RGBA today.
- **hybris "ndk" codec backend** (libmediandk via libhybris) for Android
  hardware codecs on halium devices — slot into `media/codec_backend.h`.
- The OSK (ATLKeyboardDialog) lost its gtk4-layer-shell window; needs a
  canvas-drawn replacement.

## Done in M1 so far

- **MediaCodec over the pluggable codec backend** — `media/codec_backend.h`
  defines a decoder-level interface (create/configure/queue-input/
  dequeue-audio/dequeue-video); `codec_backend_ffmpeg.c` implements it with
  libavcodec (hwdevice VAAPI/DRM_PRIME decode, CPU transfer + swscale → RGBA
  output); `ATL_MEDIA_CODEC_BACKEND` selects (default `ffmpeg`; `ndk` reserved
  for hybris/libmediandk). Video frames now flow `Surface.postFrame(Bitmap)` →
  `SurfaceView` → Skia scene — the GdkDmabufTexture/GdkMemoryTexture path and
  the `SurfaceViewWidget` GTK type are deleted.

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
