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
| `src/api-impl-jni/graphics/ATLCanvas.cpp/.h` | 27+7 | GSK render-node canvas; replace with the Skia canvas path everywhere, then delete |
| `src/api-impl-jni/graphics/android_graphics_drawable_Drawable.cpp` | 15 | GdkPaintable-based drawable rasterization → Skia |
| `src/api-impl-jni/graphics/NinePatchPaintable.c/.h` | 14+2 | GdkPaintable ninepatch → Skia ninepatch |
| `src/api-impl-jni/graphics/android_graphics_drawable_DrawableContainer.c` | 12 | GdkPaintable container → Skia |
| `src/api-impl-jni/graphics/android_graphics_drawable_NinePatchDrawable.cpp` | 5 | ditto |
| `src/libandroid/egl.c` | 21 | NDK EGL over GdkSurface; extern `window` global from launcher → Wayland EGL (wl_egl_window) |
| `src/libandroid/native_window.c/.h` | 18+1 | ANativeWindow over GTK → Wayland surface/subsurface |
| `src/libandroid/input.c` | 7 | GDK event translation for NativeActivity → GLFW/Wayland input |
| `src/api-impl-jni/widgets/android_view_SurfaceView.c/.h` | 3+5 | GTK subsurface embedding → wl_subsurface |

## Medium — host integration (replace with Wayland/DBus/UT services)

| File | Refs | Role / replacement |
|---|---|---|
| `src/api-impl-jni/util.c` | 25 | misc helpers incl. `atl_safe_gtk_*` (declared in `util_gtk.h`); split/replace per-caller |
| `src/api-impl-jni/media/android_media_MediaCodec.c` | 23 | GdkTexture video frames (GStreamer) → dmabuf/EGLImage into Skia/GL |
| `src/api-impl-jni/media/android_media_MediaPlayer.c` | 10 | ditto |
| `src/api-impl-jni/content/android_content_Context.c` | 15 | GTK file chooser / portal-ish helpers → in-scene picker or UT Content Hub |
| `src/api-impl-jni/app/android_app_Activity.c` | 14 | dialogs, window glue → in-scene views |
| `src/api-impl-jni/android_view_Window.c` | 1 | mostly converted already (GLFW) |
| `src/api-impl-jni/content/android_atl_ATLMediaContentProvider.c` | 5 | GdkTexture thumbnails → Skia |
| `src/api-impl-jni/content/android_content_ClipboardManager.c` | 2 | GDK clipboard → wl_data_device / UT Content Hub |
| `src/api-impl-jni/app/android_app_WallpaperManager.c` | 2 | trivial |
| `src/api-impl-jni/app/android_app_NotificationManager.c` | 2 | GNotification is GIO; drop stray GTK refs, keep DBus path |
| `src/api-impl-jni/audio/android_media_SoundPool.c` | 5 | stray GTK use (likely main-loop glue) → GLib only |

## Include-only (drop the #include)

- `src/api-impl-jni/sensors/android_hardware_SensorManager.c`
- `src/api-impl-jni/location/android_location_LocationManager.c`
- `src/api-impl-jni/jni_cpp.h`
- `src/api-impl-jni/audio/android_media_AudioTrack.c`

## Done at fork point

- `src/main-executable/main.c` — GApplication/GIO only; libportal dynamic-launcher
  replaced by direct `.desktop` install; GTK/GSK VectorDrawable→SVG icon rendering
  dropped (TODO: re-add via Skia).
- `src/api-impl-jni/util.h` — GTK-free; GTK helper declarations moved to `util_gtk.h`.
