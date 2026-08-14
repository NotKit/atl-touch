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
* **Mir honours a sub-surface's alpha per pixel**, so a sub-surface with holes in
  it over another sub-surface is a real compositing primitive there, not a hint.
  What is measured is *binary* alpha — 0 lets the layer below through, 255 covers
  it, inside one frame of one EGL-backed chrome buffer on an Adreno 740
  (`firefox-atl` §18.3, and `testapps/evidence/device-7f6af1cc-mir-default.png`).
  **Partial alpha on an EGL sub-surface has never been exercised on Mir**; this
  design does not use any.

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
* **The chrome's buffer must have an alpha channel, and on Ubuntu Touch it does
  not unless it is asked for.** GLFW ignores every `EGLConfig` with alpha unless
  the window asked to be transparent and the driver lacks `EGL_EXT_present_opaque`
  (`glfw/src/egl_context.c`, "HACK: ... ignore any config with an alpha channel to
  ensure the buffer is opaque"). hybris EGL has no such extension, so on the
  phone ATL's context comes up 8/8/8/0, alpha 0 lands as opaque black, and the
  content sub-surface underneath is hidden completely. **ATL therefore asks for
  the transparent framebuffer by default** (`ATLWindow.c`, `atl_window_new`);
  `ATL_SURFACE_CHROME_ALPHA=0` is the way back. Giving the chrome surface its own
  alpha-bearing config instead is tried first, and on hybris/Adreno the driver
  refuses it: `eglMakeCurrent` answers `EGL_BAD_MATCH` (`0x3009`) and ATL falls
  back to the context's own config. That is measured, on an Adreno 740 through
  hybris EGL, in `firefox-atl`'s
  `testapps/evidence/device-7f6af1cc-mir-alpha0.log`; between `eb906148` and
  2026-08-14 the same claim was in this file and in `eb906148`'s commit message
  with no run behind it.
* `SurfaceView.draw()` still issues `drawColor(0, PorterDuff.Mode.CLEAR)` over
  its own bounds. It is not removed and does not need to be: it now writes alpha
  0 into the chrome sub-surface instead of into the toplevel, which is exactly
  the "stops drawing the punch-hole into the toplevel" this design asks for.
  `SurfaceView.java` is unchanged by the split.
* The **toplevel** is cleared to opaque black once, on the frame the chrome
  becomes active and on every framebuffer resize, and is not drawn into again.
  Opaque black is deliberate: the whole toplevel is declared opaque, and a
  surface must not declare an opaque region it does not fill (§16.1 lane 4,
  design constraint 2). Black is also what shows for the one frame between the
  toplevel resize and the chrome resize.
* **ATL declares the toplevel's opaque region itself**, because GLFW only does
  it for a window that did not ask for `GLFW_TRANSPARENT_FRAMEBUFFER` and ATL now
  always asks. `atl_surface_layers_before_swap()` sends the whole window in
  chrome mode and in the ordinary no-`SurfaceView` case, and the window minus
  every hole in the legacy `toplevel` mode; it is re-sent only when the size
  changes, which is when GLFW re-sent its own. In chrome mode the toplevel
  commits rarely, so the call is made from `atl_window_present_chrome()` on the
  commit that does happen. `ATL_SURFACE_OPAQUE_REGION=0` turns the declaration
  off, which is the state atlas shipped between `eb906148` and this change.

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
| **one of two `SurfaceView`s removed** | its layer is destroyed and the chrome is left alone: destroying a sub-surface cannot reorder the others. |
| **the last `SurfaceView` removed** | the chrome is dropped on the next frame (its `EGLSurface` first, then the `wl_egl_window`) and the scene goes back into the toplevel, i.e. back to the ordinary-app path. |
| **`setZOrderOnTop(true)`** | **not honoured** in chrome mode: the layer stays below the chrome. Doing it properly means recreating that layer's `wl_surface` after the chrome, which would destroy the app's `EGLSurface` under it. It is logged once. The hole is still punched, though — `SurfaceView.draw` skips it only when the chrome is off — because without it the opaque chrome hides the app's content entirely rather than merely losing the ordering. On Mir the call was never honoured either, but there `place_above` being unimplemented left the layer above the toplevel, so the app got the visible result by accident. |
| **teardown** | the chrome is freed when a window's last content layer goes away (the row above). There is no window-teardown path to hang it on otherwise: nothing in `src/` destroys an `ATLWindow`. |
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

## Why the alpha framebuffer is the default (2026-08-14)

It was a knob for two days, and a knob is the wrong shape for it: without it
every pixel a `SurfaceView` app draws is lost on Ubuntu Touch (0 GL px and
272,191 px of black in a 300,000-px rect, `firefox-atl` §18.3). The one cost of
turning it on is that GLFW stops declaring the toplevel's opaque region, and
that is now ATL's own call, so there is nothing left to trade.

Measured on a headless sway (Mesa 26.1.4 on llvmpipe, 720x1440), three states of
the same build — the new default, `ATL_SURFACE_CHROME_ALPHA=0` (the old default)
and `ATL_SURFACE_OPAQUE_REGION=0` (alpha on, nobody declaring). The frames, the
logs and the wire traces are `firefox-atl`'s
`testapps/evidence/desktop-7f6af1cc-sway-*`:

* `surfacetest` with a dialog over the `SurfaceView`: **one frame, byte-identical
  in all three** (sha256 `dc3dd205…`, which is also the frame §17.6 and §18.3
  recorded), 72,023 dialog px and 24,000 yellow-bar px inside the layer rect.
* An animating ordinary app (`LibreSudoku`), two runs of each state: two runs of
  the *same* state differ by 3,901–4,292 px of 1,036,800 and two runs of
  *different* states by 0–4,292, so the cross-state range lies inside the
  same-state one and two cross-state pairs are byte-identical. The invariant, not
  a difference, is the result: **no pair of states differs by more than two runs
  of one state do**, and every difference is inside that app's animated centre.
* On the wire (`WAYLAND_DEBUG=1`): the old default sends
  `wl_surface.set_opaque_region` twice, both from GLFW's own `wl_compositor`;
  the new default sends it once, `wl_region.add(0,0,720,1440)` from ATL's
  `wl_compositor` immediately before the toplevel's `attach`/`commit`;
  `ATL_SURFACE_OPAQUE_REGION=0` sends it zero times.

Mesa has `EGL_EXT_present_opaque` and its config carries alpha anyway, so all
three states get a chrome config with 8 alpha bits there: **the desktop measures
the cost of the change, never the failure it fixes.** That one is the phone's.

Mir ignores `set_opaque_region` altogether (`firefox-atl` §17.4), so on the
target device the declaration is expected to be worth nothing either way; what
it buys is that no wlroots/Mesa desktop is asked to blend a window ATL knows to
be opaque. A `WAYLAND_DEBUG` trace from the phone shows ATL sending exactly one
`set_opaque_region(0,0,1080,2349)` on the toplevel, immediately before its one
and only `attach`, and Mir raising no complaint; what Mir then does with it is
not visible from the client side.

And on the phone the default is the difference between an app being visible and
not. One build of `7f6af1cc`, one `ATL_TEST_DIALOG` run each, 600x500
`SurfaceView` rect (300,000 px): the default gives 196,762 px of the app's own GL
frames, a 23,904 px overlay bar and a 75,200 px dialog over them;
`ATL_SURFACE_CHROME_ALPHA=0` on the same build gives **0** app pixels and 272,191
px of black. `firefox-atl` `testapps/evidence/device-7f6af1cc-mir-{default,alpha0}.*`.

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
* It is verified on a headless wlroots compositor only. The `none` mode makes
  that compositor stack surfaces the way Mir does, which is what makes a desktop
  run evidence at all, but nothing here has run on Mir, at 1080x2412, or through
  a window resize.
