#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include "viewporter-client-protocol.h"

#include "../ATLWindow.h"
#include "atl_surface_layer.h"

struct ATLSurfaceLayer {
	ATLWindow *parent;
	struct wl_surface *surface;
	struct wl_subsurface *subsurface;
	struct wp_viewport *viewport;
	struct wl_egl_window *egl_window;
	int buffer_width, buffer_height; /* what the producer draws at */
	int x, y, width, height;         /* view geometry, framebuffer px */
	bool fixed_size;
	bool above;   /* setZOrderOnTop */
	bool visible;
	struct ATLSurfaceLayer *next;
};

static ATLSurfaceLayer *layers;

bool atl_surface_layers_available(void)
{
	static int cached = -1;

	if (cached < 0) {
		const char *mode = getenv("ATL_SURFACE_MODE");

		cached = atl_wayland_compositor() && atl_wayland_subcompositor() ? 1 : 0;
		if (mode && strcmp(mode, "subsurface"))
			cached = 0; /* anything but "subsurface" keeps the old CPU-post path */
		fprintf(stderr, "ATLSurfaceLayer: subsurface path %s%s\n",
		        cached ? "available" : "unavailable",
		        cached && !atl_wayland_viewporter() ? " (no wp_viewporter: no setFixedSize scaling)" : "");
	}
	return cached == 1;
}

ATLSurfaceLayer *atl_surface_layer_new(ATLWindow *parent)
{
	struct wl_compositor *compositor = atl_wayland_compositor();
	struct wl_subcompositor *subcompositor = atl_wayland_subcompositor();
	struct wl_surface *parent_surface = atl_window_wl_surface(parent);
	ATLSurfaceLayer *layer;
	struct wl_region *empty;

	if (!atl_surface_layers_available() || !parent_surface)
		return NULL;

	layer = calloc(1, sizeof(*layer));
	if (!layer)
		return NULL;
	layer->parent = parent;
	layer->visible = true;
	layer->buffer_width = layer->buffer_height = 1;

	layer->surface = wl_compositor_create_surface(compositor);
	layer->subsurface = wl_subcompositor_get_subsurface(subcompositor, layer->surface, parent_surface);
	wl_subsurface_set_desync(layer->subsurface); /* the app presents at its own cadence */
	wl_subsurface_place_below(layer->subsurface, parent_surface);

	/* input must keep going to the view hierarchy: ATL resolves a wl_surface to
	 * a window by comparing against the GLFW toplevel, so anything the
	 * subsurface swallowed would simply be dropped */
	empty = wl_compositor_create_region(compositor);
	wl_surface_set_input_region(layer->surface, empty);
	wl_region_destroy(empty);

	if (atl_wayland_viewporter())
		layer->viewport = wp_viewporter_get_viewport(atl_wayland_viewporter(), layer->surface);

	layer->egl_window = wl_egl_window_create(layer->surface, 1, 1);
	if (!layer->egl_window) {
		fprintf(stderr, "ATLSurfaceLayer: wl_egl_window_create failed\n");
		atl_surface_layer_destroy(layer);
		return NULL;
	}
	wl_surface_commit(layer->surface);

	layer->next = layers;
	layers = layer;
	return layer;
}

void atl_surface_layer_destroy(ATLSurfaceLayer *layer)
{
	ATLSurfaceLayer **link = &layers;

	if (!layer)
		return;
	while (*link && *link != layer)
		link = &(*link)->next;
	if (*link)
		*link = layer->next;

	if (layer->surface) {
		wl_surface_attach(layer->surface, NULL, 0, 0);
		wl_surface_commit(layer->surface);
	}
	if (layer->egl_window)
		wl_egl_window_destroy(layer->egl_window);
	if (layer->viewport)
		wp_viewport_destroy(layer->viewport);
	if (layer->subsurface)
		wl_subsurface_destroy(layer->subsurface);
	if (layer->surface)
		wl_surface_destroy(layer->surface);
	if (layer->parent)
		atl_window_invalidate(layer->parent);
	free(layer);
}

static void layer_apply_buffer_size(ATLSurfaceLayer *layer)
{
	int w = layer->buffer_width > 0 ? layer->buffer_width : 1;
	int h = layer->buffer_height > 0 ? layer->buffer_height : 1;

	wl_egl_window_resize(layer->egl_window, w, h, 0, 0);
}

void atl_surface_layer_set_geometry(ATLSurfaceLayer *layer, int x, int y, int width, int height)
{
	double scale;

	if (!layer)
		return;
	if (layer->x == x && layer->y == y && layer->width == width && layer->height == height)
		return;
	layer->x = x;
	layer->y = y;
	layer->width = width;
	layer->height = height;

	scale = atl_window_scale(layer->parent);
	if (scale <= 0)
		scale = 1;
	if (getenv("ATL_DEBUG_LAYER"))
		fprintf(stderr, "ATLSurfaceLayer: geometry %d,%d %dx%d scale %.3f -> pos %d,%d dest %dx%d\n",
		        x, y, width, height, scale, (int)(x / scale), (int)(y / scale),
		        (int)(width / scale), (int)(height / scale));
	wl_subsurface_set_position(layer->subsurface, (int)(x / scale), (int)(y / scale));
	if (layer->viewport && width > 0 && height > 0)
		wp_viewport_set_destination(layer->viewport, (int)(width / scale), (int)(height / scale));
	else if (!layer->viewport)
		wl_surface_set_buffer_scale(layer->surface, scale >= 2 ? (int)scale : 1);

	if (!layer->fixed_size) {
		layer->buffer_width = width;
		layer->buffer_height = height;
		layer_apply_buffer_size(layer);
	}
	wl_surface_commit(layer->surface);
	/* set_position is double-buffered on the *parent*, so it only lands on the
	 * parent's next commit - which is glfwSwapBuffers in atl_window_render */
	atl_window_invalidate(layer->parent);
}

void atl_surface_layer_set_buffer_size(ATLSurfaceLayer *layer, int width, int height)
{
	if (!layer)
		return;
	if (width <= 0 || height <= 0) {
		layer->fixed_size = false;
		layer->buffer_width = layer->width;
		layer->buffer_height = layer->height;
	} else {
		layer->fixed_size = true;
		layer->buffer_width = width;
		layer->buffer_height = height;
	}
	layer_apply_buffer_size(layer);
	wl_surface_commit(layer->surface);
	atl_window_invalidate(layer->parent);
}

void atl_surface_layer_set_above(ATLSurfaceLayer *layer, bool above)
{
	struct wl_surface *parent_surface;

	if (!layer || layer->above == above)
		return;
	layer->above = above;
	parent_surface = atl_window_wl_surface(layer->parent);
	if (above)
		wl_subsurface_place_above(layer->subsurface, parent_surface);
	else
		wl_subsurface_place_below(layer->subsurface, parent_surface);
	atl_window_invalidate(layer->parent);
}

void atl_surface_layer_set_visible(ATLSurfaceLayer *layer, bool visible)
{
	if (!layer || layer->visible == visible)
		return;
	layer->visible = visible;
	if (!visible) {
		/* unmap, but keep the objects: the app's EGLSurface stays valid */
		wl_surface_attach(layer->surface, NULL, 0, 0);
		wl_surface_commit(layer->surface);
	}
	atl_window_invalidate(layer->parent);
}

struct wl_egl_window *atl_surface_layer_egl_window(ATLSurfaceLayer *layer)
{
	return layer ? layer->egl_window : NULL;
}

struct wl_surface *atl_surface_layer_wl_surface(ATLSurfaceLayer *layer)
{
	return layer ? layer->surface : NULL;
}

int atl_surface_layer_buffer_width(ATLSurfaceLayer *layer)
{
	return layer ? layer->buffer_width : 0;
}

int atl_surface_layer_buffer_height(ATLSurfaceLayer *layer)
{
	return layer ? layer->buffer_height : 0;
}

bool atl_surface_layer_is_above(ATLSurfaceLayer *layer)
{
	return layer ? layer->above : false;
}

bool atl_surface_layers_window_has_holes(ATLWindow *window)
{
	for (ATLSurfaceLayer *l = layers; l; l = l->next)
		if (l->parent == window && l->visible && !l->above)
			return true;
	return false;
}

void atl_surface_layers_before_swap(ATLWindow *window, struct wl_surface *parent,
                                    int fb_width, int fb_height, double scale)
{
	struct wl_compositor *compositor = atl_wayland_compositor();
	struct wl_region *opaque;
	int w, h;

	if (!compositor || !parent || !atl_surface_layers_window_has_holes(window))
		return;
	if (scale <= 0)
		scale = 1;
	w = (int)(fb_width / scale);
	h = (int)(fb_height / scale);

	/* whole toplevel minus every hole; GLFW re-sets its own full-window opaque
	 * region when it handles a configure, but that happens in glfwPollEvents()
	 * earlier in the same tick */
	opaque = wl_compositor_create_region(compositor);
	wl_region_add(opaque, 0, 0, w, h);
	for (ATLSurfaceLayer *l = layers; l; l = l->next) {
		if (l->parent != window || !l->visible || l->above)
			continue;
		wl_region_subtract(opaque, (int)(l->x / scale), (int)(l->y / scale),
		                   (int)(l->width / scale), (int)(l->height / scale));
	}
	wl_surface_set_opaque_region(parent, opaque);
	wl_region_destroy(opaque);
}
