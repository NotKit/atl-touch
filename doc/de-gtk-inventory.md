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
| `src/api-impl-jni/util.c` | 25 | misc helpers incl. `atl_safe_gtk_*` (declared in `util_gtk.h`); split/replace per-caller |
| `src/api-impl-jni/media/android_media_MediaCodec.c` | 23 | GdkDmabufTexture video frames → dmabuf/EGLImage into Skia/GL. **Media-backend task**: also uses GtkMediaStream-less libav decode |
| `src/api-impl-jni/media/android_media_MediaPlayer.c` | 18 | GtkMediaStream playback → GStreamer or libav+ALSA. **Media-backend task** |
| `src/api-impl-jni/audio/android_media_SoundPool.c` | 8 | GtkMediaStream one-shot playback → same media backend. **Media-backend task** |
| `src/api-impl-jni/content/android_content_Context.c` | 17 | GTK file chooser / portal-ish helpers → in-scene picker or UT Content Hub |
| `src/api-impl-jni/app/android_app_Activity.c` | 18 | dialogs, window glue → in-scene views |
| `src/api-impl-jni/content/android_atl_ATLMediaContentProvider.c` | 6 | `gtk_file_dialog` folder picker → in-scene picker / Content Hub |

The three media files form one **media-backend task**: GStreamer isn't
installed on the dev machine, and libavcodec is already a dependency, so the
likely path is libav decode → ALSA (audio) / dmabuf-EGLImage (video), replacing
the GtkMediaStream/GdkDmabufTexture usage in all three at once.

## Done in M1 so far

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
