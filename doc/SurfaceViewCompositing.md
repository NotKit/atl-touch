# Where ATL draws when a `SurfaceView` is on screen

This is the design decision behind the two-sub-surface split: which Wayland
surface owns which pixels once an app has a `SurfaceView`, who clears what, and
what the toplevel is still for. It is written down before the code because the
seam — panels, dialogs, popups, the IME, two `SurfaceView`s, teardown — is the
expensive part to get wrong.

Background: `firefox-atl/PLAN.md` §13.2 (the original punch-hole design), §15.4
(what the oneplus11 said), §16.1 (the design decision) and §16.6 item 6.

## The problem, in one paragraph

The punch-hole design assumes the content sub-surface can be put *below* the
toplevel: ATL draws its whole scene into the toplevel, writes alpha 0 over the
`SurfaceView`'s rectangle, and the compositor shows the sub-surface through the
hole. Anything ATL draws afterwards — a toolbar, a dialog, a popup — is above
the content because it is in the toplevel. That works on wlroots and it does not
work on the target device: Mir 1.8.3 implements neither
`wl_subsurface.place_below` nor `wl_subsurface.place_above`, and both fail
*silently* (a server-side TODO, no protocol error), so the sub-surface stays
where a newly created sub-surface goes — **above** its parent. On the oneplus11
that inverts the design: the content covers ATL's chrome, and inside the layer
rect (240,924)-(839,1423) there are 299,975 of 300,000 px of the GL client's own
colours and zero dialog pixels. `wl_region.subtract` is unimplemented too, so
the punched opaque region cannot even be expressed there.

Two things follow, and they are what makes the design below the only one that
needs no capability detection:

* **Creation order is the one ordering primitive that works everywhere.** A
  newly created sub-surface is placed at the top of its parent's child stack by
  Wayland's own rules, and Mir honours that (`firefox-atl/testapps/wlsubz`, §16.1).
* **Mir blends a sub-surface per pixel**, so a mostly transparent sub-surface
  over another sub-surface is a real compositing primitive there, not a hint.

## The stack

Bottom to top, for a window that has at least one `SurfaceView`:

| # | surface | who creates it | what is in it |
|---|---|---|---|
| 0 | the GLFW toplevel | GLFW, at `atl_window_new` | nothing after the split — one opaque black frame, kept only so the surface has a full-size buffer |
| 1 | content sub-surface(s) | `atl_surface_layer_new`, one per `SurfaceView` | the app's own frames: a GL producer's `eglSwapBuffers`, or `ANativeWindow_lock`/`unlockAndPost` |
| 2 | the chrome sub-surface | `atl_surface_chrome_ensure`, created *after* every content sub-surface | **the entire ATL scene**: the view hierarchy, every panel, every dialog, every popup — with alpha 0 over each `SurfaceView` rectangle |

Nothing in that table needs `place_above`, `place_below`, `wl_region.subtract`
or a compositor capability query. The only ordering rule is *when* a surface is
created, and that rule is enforced in one place:

> **Whenever a content sub-surface is created, the chrome sub-surface is
> destroyed and created again**, so it is on top of the child stack once more.
> The chrome's `wl_egl_window` changes identity when that happens, which is the
> signal `ATLWindow` uses to rebuild its `EGLSurface`.

## Where each kind of drawing lands

The seam turns out to be trivial once the whole scene moves, and that is the
main argument for moving the whole scene rather than splitting it:

* **`ViewRootImpl`'s panels — `Dialog`, `PopupWindow`, spinner drop-downs,
  context menus, the dim-behind scrim, panel shadows — are not surfaces in ATL.**
  `ViewRootImpl.performDraw` draws the main view and then every panel into *one*
  canvas (`src/api-impl/android/view/ViewRootImpl.java:284-313`). They therefore
  land in the chrome sub-surface for free, above every content sub-surface,
  whether or not they overlap it. **A dialog that overlaps a `SurfaceView` and
  one that does not take exactly the same path**; there is no per-panel geometry
  decision to make, and no panel needs a surface of its own.
* **Ordinary views drawn after the `SurfaceView` in the hierarchy** (the test
  app's yellow overlay bar, a Fenix toolbar over the content view) are in the
  same canvas and are above the content for the same reason.
* **The soft keyboard is not ATL's surface at all.** On Ubuntu Touch it is
  lomiri-keyboard's own window, driven over D-Bus
  (`src/api-impl-jni/input/input_method_maliit.c`); ATL only reserves an inset,
  which shrinks the *layout* and so lands in the chrome like everything else.
  There is nothing to place.
* **A `SurfaceView` that posts CPU frames through `Surface.postFrame`** (camera
  preview, `MediaCodec`, `SurfaceHolder.lockCanvas`) keeps working exactly as it
  does today: `SurfaceView.draw` unmaps the layer, blits the posted Bitmap into
  the scene, and draws no hole (`SurfaceView.java:105-120`). Those frames are
  scene pixels, so they land in the chrome sub-surface. This is the path atlas
  `fe620a98` exists for and it is untouched.
* **`WebView`** composites its EGLImage into the scene as well, so it follows
  the scene into the chrome.

## What clears what

* `atl_canvas_begin_frame` clears the frame's damage rect to
  `SK_ColorTRANSPARENT`, and both present paths blit with `SkBlendMode::kSrc`
  (`atl_canvas_gpu_present`, and the raster path's blend-less textured quad), so
  the scene's alpha reaches the chrome's buffer unmodified. That is what makes
  the hole a hole. **This is the same mechanism the punch-hole already relied
  on; the only thing that changes is which buffer it lands in.**
* `SurfaceView.draw()` still issues `drawColor(0, PorterDuff.Mode.CLEAR)` over
  its own bounds. It is not removed and does not need to be: it now writes alpha
  0 into the chrome sub-surface instead of into the toplevel, which is exactly
  the "stops drawing the punch-hole into the toplevel" this design asks for.
  `SurfaceView.java` is unchanged by the split.
* The **toplevel** is cleared to opaque black once, on the frame the chrome
  becomes active and on every framebuffer resize, and is not drawn into again.
  Opaque black is deliberate: GLFW declares a full-window opaque region for a
  window that did not ask for `GLFW_TRANSPARENT_FRAMEBUFFER`, and a surface must
  not declare an opaque region it does not fill (§16.1 lane 4, design
  constraint 2). Black is also what shows for the one frame between the toplevel
  resize and the chrome resize.
* ATL's punched opaque region (`atl_surface_layers_before_swap`) is **not
  issued** in chrome mode. It exists to tell the compositor which part of the
  toplevel is transparent; in chrome mode no part of the toplevel is
  transparent. It stays for the legacy mode.

## What the toplevel is still for

Three things, and they are all real:

1. **Input.** ATL resolves a `wl_surface` to a window by comparing it against
   the GLFW toplevel (`ATLWindow.c:224-231`), so every sub-surface must set an
   empty input region or the events it swallows are simply dropped. The content
   layers already do; the chrome does too. All pointer, touch and keyboard
   input therefore lands on the toplevel, which is where the dispatch expects
   it.
2. **A full-size buffer.** A `wl_surface`'s input region is clipped to its own
   buffer, so a 1×1 toplevel is not an optimisation, it is a way to lose all
   window input (§16.1 lane 4, design constraint 1, measured on sway). The
   toplevel keeps a full-size buffer; it is simply not redrawn.
3. **Window management.** `xdg_toplevel` — the title, the app id, configure and
   ACTIVATED, maximise, close — belongs to the toplevel and to nothing else.

It is *not* needed as a transparent surface. That is worth stating because it
removes the one untested risk in the chosen design: nothing here depends on
whether Mir composites a fully transparent toplevel (it does — lane 4 measured
it — but the chrome covers the toplevel completely, so the answer does not
matter), and nothing depends on `GLFW_TRANSPARENT_FRAMEBUFFER`, which has never
been measured on any compositor.

## Cases

| case | what happens |
|---|---|
| **no `SurfaceView`** (every ordinary app) | no content layer is ever created, so no chrome sub-surface is ever created either, and the scene keeps going straight into the toplevel. The split costs those apps nothing and cannot regress them. |
| **one `SurfaceView`** | content layer at creation; chrome created immediately after it, on the next frame. |
| **two `SurfaceView`s** | two content layers; the chrome is recreated after the *second* one, so it is above both. Their order relative to each other is their creation order, which is what AOSP's default gives too. |
| **a dialog over the `SurfaceView`** | drawn into the chrome, above the content. This is M2's exit criterion. |
| **a dialog beside the `SurfaceView`** | same path, no special case. |
| **a `SurfaceView` that is removed** | its layer is destroyed; the chrome stays (it covers the whole window and costs one present either way). Recreating the chrome is not needed because destroying a sub-surface cannot reorder the others. |
| **`setZOrderOnTop(true)`** | **not honoured** in chrome mode: the layer stays below the chrome. Doing it properly means recreating that layer's `wl_surface` after the chrome, which would destroy the app's `EGLSurface` under it. It is logged once. On Mir it was never honoured anyway (`place_above` is the other unimplemented call). |
| **teardown** | the chrome is destroyed with the window; the content layers keep the lifetime they already had (`Activity.detachWindowViews` → `surfaceDestroyed` → `native_destroyLayer`, atlas `062965c6`). |
| **X11 / no `wl_subcompositor`** | `atl_surface_layers_available()` is false, no layer, no chrome, unchanged behaviour. |

## The knob and the fallback

`ATL_SURFACE_CHROME` selects the mode (`doc/Envs.md`):

* unset or `subsurface` — the design above. **Default.**
* `toplevel` — the pre-split behaviour: the content layer asks for
  `place_below` and the scene stays in the toplevel. Correct on wlroots,
  inverted on Mir. Kept as the fallback and as the A/B control.
* `none` — content sub-surface above the toplevel *and* the scene in the
  toplevel, i.e. what ATL does on Mir today. It is not a useful runtime mode; it
  exists so the device's failure can be reproduced on a desktop compositor,
  because after this change sway and Mir agree about ordering.

If the chrome's `EGLSurface` cannot be created, ATL logs it, drops back to
`toplevel` mode for the rest of the process and issues `place_below` on the
layers that already exist. On Mir that fallback is the inverted behaviour, which
is what it was before this change.

## What this design does not do

* It does not make ATL's chrome cheaper. The chrome sub-surface is a full-window
  buffer presented every frame, exactly like the toplevel was. The toplevel is
  presented only when it must be (chrome activation, resize, and any frame where
  a sub-surface has parent-side state to apply), so the steady state is still one
  full-window present per frame.
* It does not give the chrome an opaque region, so a compositor blends the whole
  window every frame even where the chrome is opaque. Declaring the real opaque
  region (the window minus the holes) on the *chrome* surface is a legitimate
  optimisation and is exactly the call that Mir ignores today, so it can only
  ever be a hint. Not done.
* It does not change `SurfaceView`'s CPU-post path, `setFixedSize` scaling, or
  the `wp_viewporter` handling.
* Sub-surface synchronisation is left as it is: content layers are desync (the
  app presents at its own cadence) and so is the chrome.
