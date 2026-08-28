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
	bool needs_parent_commit; /* set_position/creation is parent-double-buffered */
	struct ATLSurfaceLayer *next;
};

static ATLSurfaceLayer *layers;

/* the chrome sub-surface: one per window, created after every content layer */
typedef struct ATLSurfaceChrome {
	ATLWindow *parent;
	struct wl_surface *surface;
	struct wl_subsurface *subsurface;
	struct wp_viewport *viewport;
	struct wl_egl_window *egl_window;
	int width, height;   /* framebuffer px */
	int dest_width, dest_height; /* logical px, as last told to the compositor */
	bool needs_parent_commit;
	bool stale;          /* a layer appeared under it: rebuild to get back on top */
	struct ATLSurfaceChrome *next;
} ATLSurfaceChrome;

static ATLSurfaceChrome *chromes;

/* ATL_SURFACE_CHROME: "subsurface" (default) puts the scene in a chrome
 * sub-surface above the content layers; "toplevel" is the pre-split punch-hole
 * behaviour (content below the toplevel, which needs place_below and so only
 * works on wlroots); "none" is neither, i.e. what ATL did on Mir - kept so the
 * device's failure can be reproduced on a desktop compositor. */
static bool chrome_disabled;   /* set by the env, or by a failed EGLSurface */

bool atl_surface_chrome_enabled(void)
{
	static int cached = -1;

	if (cached < 0) {
		const char *mode = getenv("ATL_SURFACE_CHROME");
		cached = !mode || !strcmp(mode, "subsurface") || !strcmp(mode, "1");
		if (!cached)
			fprintf(stderr, "ATLSurfaceLayer: chrome sub-surface disabled (ATL_SURFACE_CHROME=%s)\n", mode);
	}
	return cached == 1 && !chrome_disabled;
}

/*
 * ATL_SURFACE_CHROME_ALPHA: ask GLFW for a framebuffer with an alpha channel,
 * which is the only way to get one where EGL_EXT_present_opaque is missing -
 * the phone. Without it the chrome's buffer is 8/8/8/0, a SurfaceView's hole
 * lands as opaque black and the app's own frames are never seen. On by default
 * since 2026-08-14; ATL_SURFACE_CHROME_ALPHA=0 is the way back.
 */
bool atl_surface_chrome_alpha_enabled(void)
{
	static int cached = -1;

	if (cached < 0) {
		const char *v = getenv("ATL_SURFACE_CHROME_ALPHA");
		cached = !(v && (!strcmp(v, "0") || !strcmp(v, "off") || !strcmp(v, "no")));
	}
	return cached == 1;
}

/*
 * ATL_SURFACE_OPAQUE_REGION=0 stops ATL declaring the toplevel's opaque region.
 * GLFW declares it itself for a window that did not ask to be transparent and
 * stops as soon as it does, so with the alpha framebuffer on this declaration
 * is the only one there is. The knob exists to measure what it is worth and to
 * escape a compositor that gets it wrong.
 */
bool atl_surface_opaque_region_enabled(void)
{
	static int cached = -1;

	if (cached < 0) {
		const char *v = getenv("ATL_SURFACE_OPAQUE_REGION");
		cached = !(v && (!strcmp(v, "0") || !strcmp(v, "off") || !strcmp(v, "no")));
	}
	return cached == 1;
}

/*
 * A chrome outlives the content layers it was created for.
 *
 * Freeing it when the last SurfaceView goes away hands the scene back to the
 * toplevel -- and in chrome mode that toplevel has not been committed since the
 * chrome went up ("chrome present ..., parent commit no", every frame). It
 * still holds the opaque black clear atl_window_present_chrome left in it, and
 * on Mir the frames drawn into it afterwards never reach the screen: measured
 * on the oneplus11, 181 full-window frames after the free with the window
 * uniformly #000000, byte-identical on the GPU and the raster path -- and the
 * raster path clears to opaque white before it blits, so this is not an empty
 * scene, it is a surface that is not being presented.
 *
 * So keep the chrome. It covers the whole window, it is the surface the
 * compositor is already showing, and the layer it was made for usually comes
 * straight back (a fragment swap out of the browser and back into it).
 *
 * ATL_FREE_CHROME_WITH_LAYERS=1 restores the old behaviour, so the black can be
 * reproduced against the same binary that fixes it.
 */
static bool window_has_layer(ATLWindow *window);

static bool free_chrome_with_layers(void)
{
	static int cached = -1;

	if (cached < 0) {
		const char *v = getenv("ATL_FREE_CHROME_WITH_LAYERS");
		cached = v && *v && strcmp(v, "0") ? 1 : 0;
	}
	return cached == 1;
}

/* the chrome is dropped when it must be rebuilt on top of a newer layer, or
 * when it is switched off -- not merely because no layer is up right now */
static bool chrome_should_go(ATLWindow *window, ATLSurfaceChrome *chrome)
{
	if (!chrome)
		return false;
	if (chrome->stale || !atl_surface_chrome_enabled())
		return true;
	return free_chrome_with_layers() && !window_has_layer(window);
}

/* "toplevel" keeps asking for place_below; "none" reproduces Mir's ordering */
static bool layers_place_below(void)
{
	const char *mode = getenv("ATL_SURFACE_CHROME");

	if (atl_surface_chrome_enabled())
		return false; /* the chrome is above us; below the parent would hide us */
	return !mode || strcmp(mode, "none");
}

static ATLSurfaceChrome *chrome_for(ATLWindow *window)
{
	for (ATLSurfaceChrome *c = chromes; c; c = c->next)
		if (c->parent == window)
			return c;
	return NULL;
}

static void chrome_free(ATLSurfaceChrome *chrome)
{
	ATLSurfaceChrome **link = &chromes;

	while (*link && *link != chrome)
		link = &(*link)->next;
	if (*link)
		*link = chrome->next;
	if (chrome->egl_window)
		wl_egl_window_destroy(chrome->egl_window);
	if (chrome->viewport)
		wp_viewport_destroy(chrome->viewport);
	if (chrome->subsurface)
		wl_subsurface_destroy(chrome->subsurface);
	if (chrome->surface)
		wl_surface_destroy(chrome->surface);
	free(chrome);
}

/*
 * Mark the chrome for rebuild, so the next atl_surface_chrome_ensure() creates a
 * new one - which Wayland places on top of the parent's child stack again. It is
 * not freed here: the EGLSurface the renderer holds must go first, because
 * destroying a wl_egl_window an EGLSurface still references is a use-after-free
 * inside the EGL driver.
 *
 * The frame that does the rebuild has to be asked for, like every other mutator
 * in this file does: without it the chrome is only marked, and the sub-surface
 * that was just created keeps sitting on top of it until some unrelated damage
 * happens to arrive.
 */
static void chrome_invalidate(ATLWindow *window)
{
	ATLSurfaceChrome *chrome = chrome_for(window);

	if (chrome)
		chrome->stale = true;
	atl_window_invalidate(window);
}

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

static int count_layers(ATLWindow *window)
{
	int n = 0;

	for (ATLSurfaceLayer *l = layers; l; l = l->next)
		if (l->parent == window)
			n++;
	return n;
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
	if (layers_place_below())
		wl_subsurface_place_below(layer->subsurface, parent_surface);
	layer->needs_parent_commit = true;

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
	if (getenv("ATL_DEBUG_LAYER"))
		fprintf(stderr, "ATLSurfaceLayer: content layer created (%d on this window)\n",
		        count_layers(parent));
	/* a sub-surface created after this one would land on top of it, so the
	 * chrome has to be built again - creation order is the only ordering
	 * primitive that works on Mir */
	chrome_invalidate(parent);
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
	if (layer->parent) {
		if (getenv("ATL_DEBUG_LAYER"))
			fprintf(stderr, "ATLSurfaceLayer: content layer destroyed (%d left on this window)\n",
			        count_layers(layer->parent));
		atl_window_invalidate(layer->parent);
	}
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
	layer->needs_parent_commit = true;

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
	if (atl_surface_chrome_enabled()) {
		/* "above" would mean above the chrome, which needs this layer's
		 * wl_surface recreated after it - and that destroys the EGLSurface the
		 * app is already presenting to. See doc/SurfaceViewCompositing.md. */
		static bool warned;
		if (above && !warned) {
			warned = true;
			fprintf(stderr, "ATLSurfaceLayer: setZOrderOnTop is not honoured with the chrome sub-surface; "
			                "the layer stays below ATL's own drawing\n");
		}
		return;
	}
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

/*
 * The whole toplevel, declared opaque. This is what GLFW itself sends for a
 * window that did not ask for GLFW_TRANSPARENT_FRAMEBUFFER, and ATL now does
 * ask (the chrome's hole needs the alpha channel), so it falls to ATL. The
 * toplevel really is opaque: in chrome mode it is one opaque black clear under
 * a chrome that covers it, and otherwise it carries the scene exactly as it did
 * when its config had no alpha bits at all.
 *
 * Re-sent only when the size changes, which is when GLFW re-sent its own (it
 * does it per xdg_toplevel.configure). One region per window, remembered here
 * because the ATLWindow struct is private to ATLWindow.c.
 */
struct toplevel_opaque {
	ATLWindow *window;
	int w, h;
	struct toplevel_opaque *next;
};
static struct toplevel_opaque *toplevel_opaques;

static void declare_whole_toplevel_opaque(ATLWindow *window, struct wl_compositor *compositor,
                                          struct wl_surface *parent, int w, int h)
{
	struct toplevel_opaque *o;
	struct wl_region *opaque;

	for (o = toplevel_opaques; o; o = o->next)
		if (o->window == window)
			break;
	if (o && o->w == w && o->h == h)
		return;
	if (!o) {
		o = calloc(1, sizeof(*o));
		if (!o)
			return;
		o->window = window;
		o->next = toplevel_opaques;
		toplevel_opaques = o;
	}
	o->w = w;
	o->h = h;

	opaque = wl_compositor_create_region(compositor);
	wl_region_add(opaque, 0, 0, w, h);
	wl_surface_set_opaque_region(parent, opaque);
	wl_region_destroy(opaque);
}

/* the punched region below replaces it, so the next full-window one must go out
 * again even at an unchanged size */
static void forget_whole_toplevel_opaque(ATLWindow *window)
{
	for (struct toplevel_opaque *o = toplevel_opaques; o; o = o->next)
		if (o->window == window)
			o->w = o->h = 0;
}

void atl_surface_layers_before_swap(ATLWindow *window, struct wl_surface *parent,
                                    int fb_width, int fb_height, double scale)
{
	struct wl_compositor *compositor = atl_wayland_compositor();
	struct wl_region *opaque;
	int w, h;

	if (!compositor || !parent)
		return;
	if (scale <= 0)
		scale = 1;
	w = (int)(fb_width / scale);
	h = (int)(fb_height / scale);

	/* in chrome mode no part of the toplevel is transparent, so it must not
	 * declare a punched region it does not fill - it declares all of itself */
	if (atl_surface_chrome_enabled() || !atl_surface_layers_window_has_holes(window)) {
		if (atl_surface_chrome_alpha_enabled() && atl_surface_opaque_region_enabled())
			declare_whole_toplevel_opaque(window, compositor, parent, w, h);
		return;
	}

	/* whole toplevel minus every hole; GLFW re-sets its own full-window opaque
	 * region when it handles a configure, but that happens in glfwPollEvents()
	 * earlier in the same tick */
	forget_whole_toplevel_opaque(window); /* so it is re-sent if the holes go */
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

/* --- the chrome sub-surface --- */

/* every window that has a content layer needs one; a window with none keeps
 * rendering straight into its toplevel, so ordinary apps never take this path */
static bool window_has_layer(ATLWindow *window)
{
	for (ATLSurfaceLayer *l = layers; l; l = l->next)
		if (l->parent == window)
			return true;
	return false;
}

struct wl_egl_window *atl_surface_chrome_ensure(ATLWindow *window, int fb_width, int fb_height, double scale)
{
	struct wl_compositor *compositor = atl_wayland_compositor();
	struct wl_subcompositor *subcompositor = atl_wayland_subcompositor();
	struct wl_surface *parent_surface = atl_window_wl_surface(window);
	ATLSurfaceChrome *chrome;
	struct wl_region *empty;

	/* free a chrome the renderer has already let go of: stale (a layer appeared
	 * under it), orphaned (its window's last SurfaceView went away) or disabled */
	chrome = chrome_for(window);
	if (chrome_should_go(window, chrome)) {
		/* unconditional, like the creation line below: without it the window
		 * silently stops compositing through the chrome and the log of a run
		 * that went black says nothing at all about it */
		fprintf(stderr, "ATLSurfaceChrome: chrome sub-surface freed (%s)\n",
		        chrome->stale ? "a content layer appeared under it"
		        : !atl_surface_chrome_enabled() ? "disabled"
		        : "its window has no content layer left");
		chrome_free(chrome);
		chrome = NULL;
	}

	if (!atl_surface_chrome_enabled() || !compositor || !subcompositor || !parent_surface)
		return NULL;
	/* only creating one needs a live layer to justify it: a window that never
	 * had a SurfaceView keeps drawing into its toplevel as before */
	if (!chrome && !window_has_layer(window))
		return NULL;
	if (fb_width < 1 || fb_height < 1)
		return NULL;
	if (scale <= 0)
		scale = 1;

	if (!chrome) {
		chrome = calloc(1, sizeof(*chrome));
		if (!chrome)
			return NULL;
		chrome->parent = window;
		chrome->surface = wl_compositor_create_surface(compositor);
		/* created last => on top of the parent's child stack, which is the one
		 * ordering rule Mir honours */
		chrome->subsurface = wl_subcompositor_get_subsurface(subcompositor, chrome->surface, parent_surface);
		wl_subsurface_set_desync(chrome->subsurface);
		/* input must reach the toplevel: atl_window_from_wl_surface() only
		 * matches GLFW's surface, so anything this swallowed would be dropped */
		empty = wl_compositor_create_region(compositor);
		wl_surface_set_input_region(chrome->surface, empty);
		wl_region_destroy(empty);
		if (atl_wayland_viewporter())
			chrome->viewport = wp_viewporter_get_viewport(atl_wayland_viewporter(), chrome->surface);
		chrome->egl_window = wl_egl_window_create(chrome->surface, fb_width, fb_height);
		if (!chrome->egl_window) {
			fprintf(stderr, "ATLSurfaceChrome: wl_egl_window_create failed\n");
			chrome_free(chrome);
			return NULL;
		}
		chrome->width = fb_width;
		chrome->height = fb_height;
		chrome->needs_parent_commit = true;
		chrome->next = chromes;
		chromes = chrome;
		int n = 0;
		for (ATLSurfaceLayer *l = layers; l; l = l->next)
			if (l->parent == window)
				n++;
		fprintf(stderr, "ATLSurfaceChrome: chrome sub-surface %dx%d above %d content layer(s)\n",
		        fb_width, fb_height, n);
	} else if (chrome->width != fb_width || chrome->height != fb_height) {
		wl_egl_window_resize(chrome->egl_window, fb_width, fb_height, 0, 0);
		chrome->width = fb_width;
		chrome->height = fb_height;
	}

	/* only on a change: this runs every frame, and both are requests */
	if (chrome->dest_width != (int)(fb_width / scale) || chrome->dest_height != (int)(fb_height / scale)) {
		chrome->dest_width = (int)(fb_width / scale);
		chrome->dest_height = (int)(fb_height / scale);
		if (chrome->viewport)
			wp_viewport_set_destination(chrome->viewport, chrome->dest_width, chrome->dest_height);
		else
			wl_surface_set_buffer_scale(chrome->surface, scale >= 2 ? (int)scale : 1);
	}
	return chrome->egl_window;
}

bool atl_surface_chrome_is_stale(ATLWindow *window)
{
	ATLSurfaceChrome *chrome = chrome_for(window);

	return chrome_should_go(window, chrome);
}

void atl_surface_chrome_fallback(ATLWindow *window)
{
	struct wl_surface *parent_surface = atl_window_wl_surface(window);

	if (chrome_disabled)
		return;
	fprintf(stderr, "ATLSurfaceChrome: falling back to the toplevel scene; ATL's drawing will be "
	                "below a SurfaceView on a compositor without place_below\n");
	chrome_disabled = true;
	chrome_invalidate(window); /* freed by the next ensure(), after its EGLSurface */
	/* the layers were created expecting something above them; put them back
	 * under the toplevel, which is the pre-split behaviour */
	for (ATLSurfaceLayer *l = layers; l; l = l->next) {
		if (l->parent != window || l->above || !parent_surface)
			continue;
		wl_subsurface_place_below(l->subsurface, parent_surface);
		l->needs_parent_commit = true;
	}
}

bool atl_surface_layers_take_parent_commit(ATLWindow *window)
{
	ATLSurfaceChrome *chrome = chrome_for(window);
	bool pending = false;

	for (ATLSurfaceLayer *l = layers; l; l = l->next) {
		if (l->parent != window || !l->needs_parent_commit)
			continue;
		l->needs_parent_commit = false;
		pending = true;
	}
	if (chrome && chrome->needs_parent_commit) {
		chrome->needs_parent_commit = false;
		pending = true;
	}
	return pending;
}
