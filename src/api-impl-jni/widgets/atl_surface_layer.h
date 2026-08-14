#pragma once

#include <stdbool.h>

/*
 * ATLSurfaceLayer: a wl_subsurface + wl_egl_window hanging under an ATLWindow's
 * GLFW toplevel, so a SurfaceView's Surface can back a real EGLSurface that the
 * app presents to at its own cadence.
 *
 * Content layers sit above the toplevel and below a second, full-window "chrome"
 * sub-surface that carries the whole ATL scene, with an alpha hole where each
 * SurfaceView is. Ordering is by creation only - Mir implements neither
 * place_above nor place_below and fails at both silently - so the chrome is
 * recreated whenever a content layer appears. doc/SurfaceViewCompositing.md is
 * the design; ATL_SURFACE_CHROME=toplevel restores the old punch-hole-into-the-
 * toplevel behaviour.
 *
 * All entry points here are main-thread only: they talk Wayland on GLFW's
 * connection, which glfwPollEvents() owns.
 */

typedef struct ATLWindow ATLWindow;
typedef struct ATLSurfaceLayer ATLSurfaceLayer;

struct wl_egl_window;
struct wl_surface;

/* is the subsurface path usable at all (Wayland platform + wl_subcompositor)? */
bool atl_surface_layers_available(void);

ATLSurfaceLayer *atl_surface_layer_new(ATLWindow *parent);
void atl_surface_layer_destroy(ATLSurfaceLayer *layer);

/* view geometry in the parent's framebuffer pixels */
void atl_surface_layer_set_geometry(ATLSurfaceLayer *layer, int x, int y, int width, int height);
/* SurfaceHolder.setFixedSize: the buffer size the producer draws at */
void atl_surface_layer_set_buffer_size(ATLSurfaceLayer *layer, int width, int height);
void atl_surface_layer_set_above(ATLSurfaceLayer *layer, bool above);
void atl_surface_layer_set_visible(ATLSurfaceLayer *layer, bool visible);

struct wl_egl_window *atl_surface_layer_egl_window(ATLSurfaceLayer *layer);
struct wl_surface *atl_surface_layer_wl_surface(ATLSurfaceLayer *layer);
int atl_surface_layer_buffer_width(ATLSurfaceLayer *layer);
int atl_surface_layer_buffer_height(ATLSurfaceLayer *layer);
bool atl_surface_layer_is_above(ATLSurfaceLayer *layer);

/*
 * Called just before every commit of the toplevel: sends its opaque region, as
 * "everything except the holes" in the punch-hole mode and as the whole window
 * in every other case. Without the first a compositor is entitled to ignore the
 * alpha the scene wrote; without the second nobody declares the region at all,
 * because GLFW stops declaring it as soon as the window asks for a transparent
 * framebuffer - which ATL does whenever the chrome needs an alpha channel.
 */
void atl_surface_layers_before_swap(ATLWindow *window, struct wl_surface *parent,
                                    int fb_width, int fb_height, double scale);
/*
 * Should the window ask GLFW for an alpha-bearing framebuffer? Yes unless
 * ATL_SURFACE_CHROME_ALPHA=0: the chrome sub-surface's hole is opaque black
 * without one wherever the EGL driver has no EGL_EXT_present_opaque.
 */
bool atl_surface_chrome_alpha_enabled(void);
/* does this window have any layer below it (i.e. is a hole expected)? */
bool atl_surface_layers_window_has_holes(ATLWindow *window);

/* --- the chrome sub-surface: ATL's own scene, above every content layer --- */

/* is the split enabled at all (default yes; ATL_SURFACE_CHROME picks the mode) */
bool atl_surface_chrome_enabled(void);
/*
 * The wl_egl_window ATL renders its scene into, or NULL when this window has no
 * content layer (an ordinary app: the scene keeps going into the toplevel).
 * Creates the sub-surface on demand and resizes it; the returned pointer changes
 * identity whenever the chrome had to be recreated to get back on top, which is
 * the caller's signal to rebuild its EGLSurface.
 */
struct wl_egl_window *atl_surface_chrome_ensure(ATLWindow *window, int fb_width, int fb_height, double scale);
/* must the chrome be rebuilt (or dropped) before the next ensure()? The caller
 * destroys its EGLSurface first: destroying a wl_egl_window that an EGLSurface
 * still points at is a use-after-free inside the EGL driver. */
bool atl_surface_chrome_is_stale(ATLWindow *window);
/* give up on the chrome (its EGLSurface could not be created): fall back to the
 * pre-split behaviour, layers below the toplevel, for the rest of the process */
void atl_surface_chrome_fallback(ATLWindow *window);
/*
 * Does a sub-surface of this window have state that only lands on the *parent's*
 * next commit (set_position, and a newly created sub-surface's place-on-top)?
 * True once after any such change; the caller commits the toplevel and clears it.
 */
bool atl_surface_layers_take_parent_commit(ATLWindow *window);
