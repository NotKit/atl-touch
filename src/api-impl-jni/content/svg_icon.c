/* SVG rasterizer for content-hub peer icons.
 *
 * The apps content-hub offers as import sources carry their icons as SVG, and
 * nothing else in ATL can decode that. librsvg and cairo are present in the
 * Ubuntu Touch rootfs but are not build dependencies of the SDK, so they are
 * dlopen'd: without them the picker just draws a lettered placeholder.
 *
 * Unlike the rest of the content-hub client this builds everywhere — it needs
 * no content-hub headers, and off-device it is what makes the picker testable.
 */
#define _GNU_SOURCE

#include <dlfcn.h>
#include <glib-object.h>
#include <glib.h>

#include <jni.h>
#include <stdio.h>

#include "../generated_headers/android_atl_ATLContentHub.h"

#define LOGE(...) do { fprintf(stderr, "[atl-svg] " __VA_ARGS__); fputc('\n', stderr); } while (0)

typedef struct { double x, y, width, height; } rsvg_rectangle;

static struct {
	int   tried;
	void *(*handle_new_from_data)(const guint8 *, gsize, GError **);
	gboolean (*render_document)(void *, void *, const rsvg_rectangle *, GError **);
	void *(*surface_create)(int format, int width, int height);
	void *(*cairo_create)(void *surface);
	void  (*cairo_destroy)(void *cr);
	void  (*surface_destroy)(void *surface);
	void  (*surface_flush)(void *surface);
	unsigned char *(*surface_get_data)(void *surface);
	int   (*surface_get_stride)(void *surface);
} svg;

static gboolean svg_load(void)
{
	static GMutex lock;
	g_mutex_lock(&lock);
	if (!svg.tried) {
		svg.tried = 1;
		void *rsvg = dlopen("librsvg-2.so.2", RTLD_LAZY);
		void *cairo = dlopen("libcairo.so.2", RTLD_LAZY);
		if (rsvg && cairo) {
			svg.handle_new_from_data = dlsym(rsvg, "rsvg_handle_new_from_data");
			svg.render_document      = dlsym(rsvg, "rsvg_handle_render_document");
			svg.surface_create       = dlsym(cairo, "cairo_image_surface_create");
			svg.cairo_create         = dlsym(cairo, "cairo_create");
			svg.cairo_destroy        = dlsym(cairo, "cairo_destroy");
			svg.surface_destroy      = dlsym(cairo, "cairo_surface_destroy");
			svg.surface_flush        = dlsym(cairo, "cairo_surface_flush");
			svg.surface_get_data     = dlsym(cairo, "cairo_image_surface_get_data");
			svg.surface_get_stride   = dlsym(cairo, "cairo_image_surface_get_stride");
		}
		if (!svg.handle_new_from_data || !svg.render_document || !svg.surface_create
		    || !svg.cairo_create || !svg.surface_get_data || !svg.surface_get_stride) {
			LOGE("librsvg/cairo unavailable, SVG icons disabled");
			svg.handle_new_from_data = NULL;
		}
	}
	g_mutex_unlock(&lock);
	return svg.handle_new_from_data != NULL;
}

/* Render into a size x size ARGB_8888 buffer the caller frees. cairo hands back
 * premultiplied native-endian ARGB; Android Color ints are straight alpha, so
 * undo the premultiply on the way out. */
static guint32 *svg_render(const guchar *data, gsize len, int size)
{
	if (!svg_load())
		return NULL;

	GError *err = NULL;
	void *handle = svg.handle_new_from_data(data, len, &err);
	if (!handle) {
		LOGE("parse: %s", err ? err->message : "?");
		g_clear_error(&err);
		return NULL;
	}

	guint32 *pixels = NULL;
	void *surface = svg.surface_create(0 /* CAIRO_FORMAT_ARGB32 */, size, size);
	void *cr = surface ? svg.cairo_create(surface) : NULL;
	rsvg_rectangle viewport = { 0, 0, size, size };
	if (cr && svg.render_document(handle, cr, &viewport, &err)) {
		svg.surface_flush(surface);
		const unsigned char *base = svg.surface_get_data(surface);
		int stride = svg.surface_get_stride(surface);
		if (base) {
			pixels = g_new(guint32, (gsize)size * size);
			for (int y = 0; y < size; y++) {
				const guint32 *row = (const guint32 *)(base + (gsize)y * stride);
				for (int x = 0; x < size; x++) {
					guint32 p = row[x];
					guint32 a = p >> 24, r = (p >> 16) & 0xff, g = (p >> 8) & 0xff, b = p & 0xff;
					if (a != 0 && a != 0xff) {
						r = MIN(r * 255 / a, 255u);
						g = MIN(g * 255 / a, 255u);
						b = MIN(b * 255 / a, 255u);
					}
					pixels[(gsize)y * size + x] = (a << 24) | (r << 16) | (g << 8) | b;
				}
			}
		}
	} else if (err) {
		LOGE("render: %s", err->message);
	}
	g_clear_error(&err);

	if (cr) svg.cairo_destroy(cr);
	if (surface) svg.surface_destroy(surface);
	g_object_unref(handle);
	return pixels;
}

/* Returns size*size ARGB_8888 pixels, or NULL when SVG can't be rendered. */
JNIEXPORT jintArray JNICALL Java_android_atl_ATLContentHub_nativeRenderSvg(JNIEnv *env, jclass cls,
        jbyteArray jdata, jint size)
{
	(void)cls;
	if (!jdata || size <= 0)
		return NULL;
	jsize len = (*env)->GetArrayLength(env, jdata);
	jbyte *data = (*env)->GetByteArrayElements(env, jdata, NULL);
	guint32 *pixels = svg_render((const guchar *)data, (gsize)len, size);
	(*env)->ReleaseByteArrayElements(env, jdata, data, JNI_ABORT);
	if (!pixels)
		return NULL;

	jintArray arr = (*env)->NewIntArray(env, size * size);
	if (arr)
		(*env)->SetIntArrayRegion(env, arr, 0, size * size, (const jint *)pixels);
	g_free(pixels);
	return arr;
}
