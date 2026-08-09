#pragma once

#include <stdbool.h>

/*
 * ATLSurfaceLayer: a wl_subsurface + wl_egl_window hanging under an ATLWindow's
 * GLFW toplevel, so a SurfaceView's Surface can back a real EGLSurface that the
 * app presents to at its own cadence.
 *
 * The layer sits *below* the toplevel by default; SurfaceView punches an
 * alpha hole in the ATL-drawn scene where the view is, and everything ATL
 * draws over it (toolbars, dialogs, panels) composites on top for free.
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
 * Called from atl_window_render() just before glfwSwapBuffers: re-sends the
 * toplevel's opaque region as "everything except the holes", in the same commit
 * Mesa is about to make. Without it a compositor is entitled to ignore the
 * alpha the scene wrote.
 */
void atl_surface_layers_before_swap(ATLWindow *window, struct wl_surface *parent,
                                    int fb_width, int fb_height, double scale);
/* does this window have any layer below it (i.e. is a hole expected)? */
bool atl_surface_layers_window_has_holes(ATLWindow *window);
