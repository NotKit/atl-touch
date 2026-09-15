/*
 * android.hardware.HardwareBuffer, see hardware_buffer.h.
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "../defines.h"

#include "hardware_buffer.h"

#include "../generated_headers/android_hardware_HardwareBuffer.h"

/* the HardwareBuffer formats ATL can size a plain allocation for */
#define HB_FORMAT_RGBA_8888    1
#define HB_FORMAT_RGBX_8888    2
#define HB_FORMAT_RGB_888      3
#define HB_FORMAT_RGB_565      4
#define HB_FORMAT_RGBA_FP16    0x16
#define HB_FORMAT_RGBA_1010102 0x2b
#define HB_FORMAT_BLOB         0x21
#define HB_FORMAT_YCBCR_420_888 0x23

struct atl_hardware_buffer {
	gint refcount;
	GMutex lock;
	uint8_t *data;
	size_t size;
	bool owns_data;
	int width;
	int height;
	int format;
	int layers;
	uint64_t usage;

	/* the device's gralloc buffer standing behind this one, see
	 * atl_hardware_buffer_gralloc(); allocated at most once */
	void *gralloc;
	bool gralloc_tried;
	bool gralloc_locked; /* by atl_hardware_buffer_map(), for the unmap */
	/* a foreign gralloc buffer's planes, mapped by whoever owns it */
	struct atl_window_frame mapped;
	bool have_mapped;
};

static jclass buffer_class;
static jmethodID buffer_from_native;

static size_t format_size(int format, int width, int height, int layers)
{
	size_t pixels = (size_t)width * height * layers;

	switch (format) {
	case HB_FORMAT_RGBA_8888:
	case HB_FORMAT_RGBX_8888:
	case HB_FORMAT_RGBA_1010102:
		return pixels * 4;
	case HB_FORMAT_RGB_888:
		return pixels * 3;
	case HB_FORMAT_RGB_565:
		return pixels * 2;
	case HB_FORMAT_RGBA_FP16:
		return pixels * 8;
	case HB_FORMAT_YCBCR_420_888:
		return pixels * 3 / 2;
	case HB_FORMAT_BLOB:
		return pixels;
	default:
		return 0;
	}
}

static struct atl_hardware_buffer *buffer_new(int width, int height, int format, int layers, uint64_t usage)
{
	struct atl_hardware_buffer *buffer = calloc(1, sizeof(*buffer));

	buffer->refcount = 1;
	g_mutex_init(&buffer->lock);
	buffer->width = width;
	buffer->height = height;
	buffer->format = format;
	buffer->layers = layers;
	buffer->usage = usage;
	return buffer;
}

struct atl_hardware_buffer *atl_hardware_buffer_alloc(int width, int height, int format,
                                                      int layers, uint64_t usage)
{
	struct atl_hardware_buffer *buffer;
	size_t size = format_size(format, width, height, layers);

	if (!size)
		return NULL;

	buffer = buffer_new(width, height, format, layers, usage);
	buffer->data = calloc(1, size);
	if (!buffer->data) {
		g_mutex_clear(&buffer->lock);
		free(buffer);
		return NULL;
	}
	buffer->size = size;
	buffer->owns_data = true;
	return buffer;
}

struct atl_hardware_buffer *atl_hardware_buffer_wrap(uint8_t *data, size_t size, int width,
                                                     int height, int format, uint64_t usage)
{
	struct atl_hardware_buffer *buffer = buffer_new(width, height, format, 1, usage);

	buffer->data = data;
	buffer->size = size;
	return buffer;
}

/* --- the gralloc buffer behind a HardwareBuffer --------------------------- */

/* android/hardware_buffer.h's, spelled out: the host headers are the Linux
 * ones and this file is built on a desktop too */
struct hb_desc {
	uint32_t width;
	uint32_t height;
	uint32_t layers;
	uint32_t format;
	uint64_t usage;
	uint32_t stride;
	uint32_t rfu0;
	uint64_t rfu1;
};

/* AHardwareBuffer_Planes: the only way to be told a YUV buffer's layout, since
 * a plain lock says nothing about where the chroma is */
struct hb_plane {
	void *data;
	uint32_t pixel_stride;
	uint32_t row_stride;
};

struct hb_planes {
	uint32_t plane_count;
	struct hb_plane planes[4];
};

#define HB_USAGE_CPU_READ_OFTEN   3ULL
#define HB_USAGE_CPU_WRITE_OFTEN  (3ULL << 4)
#define HB_USAGE_GPU_SAMPLED      (1ULL << 8)
#define HB_USAGE_GPU_COLOR_OUTPUT (1ULL << 9)

static struct {
	bool tried;
	int (*allocate)(const struct hb_desc *desc, void **out);
	void (*release)(void *buffer);
	void (*describe)(const void *buffer, struct hb_desc *desc);
	int (*lock)(void *buffer, uint64_t usage, int32_t fence, const void *rect, void **out);
	int (*lock_planes)(void *buffer, uint64_t usage, int32_t fence, const void *rect,
	                   struct hb_planes *out);
	int (*unlock)(void *buffer, int32_t *fence);
	void (*acquire)(void *buffer);
} gralloc;

/*
 * The device's own libnativewindow, through libhybris. Not ATL's forwarder in
 * src/hybris-ndk (that one exists for an *app's* native code and would come
 * back here): this is the Android library itself, the only thing on a Halium
 * device that can hand out a gralloc handle.
 */
static bool gralloc_load(void)
{
	void *(*android_dlopen_fn)(const char *, int);
	void *(*android_dlsym_fn)(void *, const char *);
	void *lib;

	if (gralloc.tried)
		return gralloc.allocate != NULL;
	gralloc.tried = true;

	android_dlopen_fn = dlsym(RTLD_DEFAULT, "android_dlopen");
	android_dlsym_fn = dlsym(RTLD_DEFAULT, "android_dlsym");
	if (!android_dlopen_fn || !android_dlsym_fn)
		return false; /* a desktop: no hybris, no gralloc */

	lib = android_dlopen_fn("libnativewindow.so", RTLD_LAZY);
	if (!lib) {
		fprintf(stderr, "HardwareBuffer: no libnativewindow.so, so a HardwareBuffer "
		                "stays a plain allocation\n");
		return false;
	}
	gralloc.allocate = android_dlsym_fn(lib, "AHardwareBuffer_allocate");
	gralloc.release = android_dlsym_fn(lib, "AHardwareBuffer_release");
	gralloc.describe = android_dlsym_fn(lib, "AHardwareBuffer_describe");
	gralloc.lock = android_dlsym_fn(lib, "AHardwareBuffer_lock");
	gralloc.lock_planes = android_dlsym_fn(lib, "AHardwareBuffer_lockPlanes");
	gralloc.unlock = android_dlsym_fn(lib, "AHardwareBuffer_unlock");
	gralloc.acquire = android_dlsym_fn(lib, "AHardwareBuffer_acquire");
	if (!gralloc.allocate || !gralloc.release) {
		gralloc.allocate = NULL;
		fprintf(stderr, "HardwareBuffer: libnativewindow.so has no AHardwareBuffer_allocate\n");
		return false;
	}
	return true;
}

/*
 * The NV21 ATL's own frames are, into whatever 4:2:0 layout gralloc picked:
 * only lockPlanes says which one that is, and the pixel strides make the copy
 * one loop. An app that binds this buffer as an EGLImage samples what gralloc
 * holds, so a buffer left empty here is one that samples as flat green.
 */
static void gralloc_fill_ycbcr(struct atl_hardware_buffer *buffer)
{
	struct hb_planes planes = {0};
	const uint8_t *y = buffer->data;
	const uint8_t *chroma = y + (size_t)buffer->width * buffer->height;
	size_t need = (size_t)buffer->width * buffer->height * 3 / 2;

	if (!gralloc.lock_planes || !gralloc.unlock || buffer->size < need)
		return;
	if (gralloc.lock_planes(buffer->gralloc, HB_USAGE_CPU_WRITE_OFTEN, -1, NULL, &planes) ||
	    planes.plane_count < 3)
		return;

	for (int row = 0; row < buffer->height; row++) {
		uint8_t *dst = (uint8_t *)planes.planes[0].data + (size_t)row * planes.planes[0].row_stride;
		const uint8_t *src = y + (size_t)row * buffer->width;

		if (planes.planes[0].pixel_stride == 1) {
			memcpy(dst, src, (size_t)buffer->width);
		} else {
			for (int col = 0; col < buffer->width; col++)
				dst[(size_t)col * planes.planes[0].pixel_stride] = src[col];
		}
	}
	/* NV21 is Cr first, so the odd byte of each pair is Cb */
	for (int row = 0; row < buffer->height / 2; row++) {
		const uint8_t *src = chroma + (size_t)row * buffer->width;
		uint8_t *cb = (uint8_t *)planes.planes[1].data + (size_t)row * planes.planes[1].row_stride;
		uint8_t *cr = (uint8_t *)planes.planes[2].data + (size_t)row * planes.planes[2].row_stride;

		for (int col = 0; col < buffer->width / 2; col++) {
			cb[(size_t)col * planes.planes[1].pixel_stride] = src[col * 2 + 1];
			cr[(size_t)col * planes.planes[2].pixel_stride] = src[col * 2];
		}
	}
	gralloc.unlock(buffer->gralloc, NULL);
}

/* the CPU copy into the gralloc buffer, row by row since gralloc pads rows.
 * Best effort: a buffer an app only ever renders into needs no contents. */
static void gralloc_fill(struct atl_hardware_buffer *buffer)
{
	struct hb_desc desc = {0};
	size_t bpp, row, i;
	void *mapped = NULL;

	if (!buffer->data || !buffer->size || !gralloc.lock || !gralloc.unlock || !gralloc.describe)
		return;
	gralloc.describe(buffer->gralloc, &desc);
	if (!desc.stride || !desc.height)
		return;

	/* a plane set is not a raster and cannot be memcpy'd row by row: gralloc
	 * chooses its own interleaving, so each plane goes over on its own */
	if (buffer->format == HB_FORMAT_YCBCR_420_888) {
		gralloc_fill_ycbcr(buffer);
		return;
	}

	bpp = format_size(buffer->format, 1, 1, 1);
	if (!bpp)
		return;

	if (gralloc.lock(buffer->gralloc, HB_USAGE_CPU_WRITE_OFTEN, -1, NULL, &mapped) || !mapped)
		return;

	row = (size_t)buffer->width * bpp;
	if (buffer->format == HB_FORMAT_BLOB) {
		/* a blob is one run of bytes, and its "stride" is that length */
		size_t n = buffer->size < (size_t)desc.stride ? buffer->size : desc.stride;

		memcpy(mapped, buffer->data, n);
	} else {
		for (i = 0; i < (size_t)buffer->height; i++) {
			size_t src = i * row, dst = i * (size_t)desc.stride * bpp;

			if (src + row > buffer->size)
				break;
			memcpy((uint8_t *)mapped + dst, buffer->data + src, row);
		}
	}
	gralloc.unlock(buffer->gralloc, NULL);
}

/*
 * The AHardwareBuffer handles ATL has handed out, so a buffer coming back
 * through an NDK entry point can be recognised as one of ours - a surface
 * transaction is handed the handle, not the object it came from.
 */
static GHashTable *native_handles;
static GMutex native_handles_lock;

static void native_handle_register(void *handle, struct atl_hardware_buffer *buffer)
{
	g_mutex_lock(&native_handles_lock);
	if (!native_handles)
		native_handles = g_hash_table_new(NULL, NULL);
	g_hash_table_insert(native_handles, handle, buffer);
	g_mutex_unlock(&native_handles_lock);
}

/* both of a buffer's handles at once: it may have been asked for by gralloc
 * handle and by itself, and neither may outlive it */
static void native_handle_forget(struct atl_hardware_buffer *buffer)
{
	g_mutex_lock(&native_handles_lock);
	if (native_handles) {
		g_hash_table_remove(native_handles, buffer);
		/* a camera HAL cycles through its gralloc buffers, so a later frame
		 * may already have registered its own wrap of this same handle */
		if (buffer->gralloc && g_hash_table_lookup(native_handles, buffer->gralloc) == buffer)
			g_hash_table_remove(native_handles, buffer->gralloc);
	}
	g_mutex_unlock(&native_handles_lock);
}

/* a gralloc buffer ATL cannot read back, once: what is presented from then on
 * is the CPU copy, which is blank for a buffer the app only rendered into */
static void gralloc_read_refused(struct atl_hardware_buffer *buffer)
{
	static bool logged;

	if (logged)
		return;
	logged = true;
	fprintf(stderr, "HardwareBuffer: gralloc refused a CPU read of a %dx%d format 0x%x buffer, "
	                "so a presented frame is whatever ATL last wrote\n",
	        buffer->width, buffer->height, buffer->format);
}

static struct atl_hardware_buffer *native_handle_lookup(void *handle)
{
	struct atl_hardware_buffer *buffer;

	g_mutex_lock(&native_handles_lock);
	buffer = native_handles ? g_hash_table_lookup(native_handles, handle) : NULL;
	g_mutex_unlock(&native_handles_lock);
	return buffer;
}

void *atl_hardware_buffer_gralloc(struct atl_hardware_buffer *buffer)
{
	struct hb_desc desc;

	if (!buffer)
		return NULL;

	g_mutex_lock(&buffer->lock);
	if (buffer->gralloc_tried) {
		void *out = buffer->gralloc;

		g_mutex_unlock(&buffer->lock);
		return out;
	}
	buffer->gralloc_tried = true;

	if (!gralloc_load()) {
		g_mutex_unlock(&buffer->lock);
		return NULL;
	}

	desc = (struct hb_desc){
	    .width = (uint32_t)buffer->width,
	    .height = (uint32_t)buffer->height,
	    .layers = (uint32_t)(buffer->layers > 0 ? buffer->layers : 1),
	    .format = (uint32_t)buffer->format,
	    /* whatever the app asked for, plus what it is asking for now: a
	     * gralloc handle is only ever wanted for the GPU, the fill below
	     * needs to write, and ATL composites on the CPU - presenting this
	     * buffer means reading back whatever the GPU rendered into it */
	    .usage = buffer->usage | HB_USAGE_GPU_SAMPLED | HB_USAGE_CPU_WRITE_OFTEN |
	             HB_USAGE_CPU_READ_OFTEN,
	};
	if (gralloc.allocate(&desc, &buffer->gralloc) || !buffer->gralloc) {
		buffer->gralloc = NULL;
		fprintf(stderr, "HardwareBuffer: gralloc refused %dx%d format 0x%x usage 0x%llx\n",
		        buffer->width, buffer->height, buffer->format,
		        (unsigned long long)desc.usage);
		g_mutex_unlock(&buffer->lock);
		return NULL;
	}
	gralloc_fill(buffer);
	native_handle_register(buffer->gralloc, buffer);
	fprintf(stderr, "HardwareBuffer: %dx%d format 0x%x is gralloc-backed now\n",
	        buffer->width, buffer->height, buffer->format);
	g_mutex_unlock(&buffer->lock);
	return buffer->gralloc;
}

/*
 * The AHardwareBuffer an app's native code should see for this buffer: the
 * device's gralloc handle, and where there is no gralloc at all (any desktop)
 * ATL's own buffer, which is opaque to the app and lets ATL's own consumers -
 * a surface transaction presenting it - still read the pixels.
 *
 * Never a plain allocation on a device: a handle that reaches the device's own
 * libnativewindow has to be one it allocated. Exported for
 * src/hybris-ndk/nativewindow_forward.c.
 */
void *atl_hardware_buffer_native(struct atl_hardware_buffer *buffer)
{
	void *gralloc = atl_hardware_buffer_gralloc(buffer);

	if (gralloc)
		return gralloc;
	if (!buffer || gralloc_load())
		return NULL;

	native_handle_register(buffer, buffer);
	return buffer;
}

/*
 * The pixels behind a handle atl_hardware_buffer_native() handed out: the CPU
 * copy where ATL owns them, and the gralloc mapping where the app may have
 * rendered into the buffer with the GPU since. Paired with
 * atl_hardware_buffer_unmap(). Exported for src/libandroid/surface_control.c.
 */
/* the gralloc mapping of a plane set: only lockPlanes can say where the chroma
 * is, and a buffer with fewer than three planes is not a YCbCr one */
static bool map_gralloc_planes(struct atl_hardware_buffer *buffer, struct atl_window_frame *frame)
{
	struct hb_planes planes = {0};

	if (!gralloc.lock_planes)
		return false;
	if (gralloc.lock_planes(buffer->gralloc, HB_USAGE_CPU_READ_OFTEN, -1, NULL, &planes) ||
	    planes.plane_count < 3)
		return false;

	for (int i = 0; i < 3; i++) {
		if (!planes.planes[i].data)
			return false;
		frame->planes[i] = (struct atl_window_plane){
		    planes.planes[i].data,
		    planes.planes[i].row_stride,
		    planes.planes[i].pixel_stride,
		};
	}
	return true;
}

/* a 4:2:0 buffer with no room for its chroma, once: reading it would make the
 * colours up out of whatever follows the luma */
static void map_short_buffer(const struct atl_hardware_buffer *buffer)
{
	static bool logged;

	if (logged)
		return;
	logged = true;
	fprintf(stderr, "HardwareBuffer: a %dx%d YCbCr buffer holds only %zu bytes, and 4:2:0 needs "
	                "%zu - nothing presented\n",
	        buffer->width, buffer->height, buffer->size,
	        (size_t)buffer->width * buffer->height * 3 / 2);
}

/* which source a presented buffer was read from, and what shape it was, once:
 * a viewfinder with the wrong colours is nearly always the wrong plane, and
 * the numbers say which without a second run */
static void map_explain(const struct atl_hardware_buffer *buffer,
                        const struct atl_window_frame *frame, const char *source)
{
	static bool logged;

	if (logged || !getenv("ATL_DEBUG_PRESENT"))
		return;
	logged = true;
	fprintf(stderr, "HardwareBuffer: presenting from %s - %dx%d format 0x%x, %s, size %zu"
	                " (luma would be %zu), planes y=%p/%zu/%zu cb=%p/%zu/%zu cr=%p/%zu/%zu\n",
	        source, buffer->width, buffer->height, buffer->format,
	        buffer->owns_data ? "allocated" : "wrapped", buffer->size,
	        (size_t)buffer->width * buffer->height,
	        (void *)frame->planes[0].data, frame->planes[0].row_stride, frame->planes[0].pixel_stride,
	        (void *)frame->planes[1].data, frame->planes[1].row_stride, frame->planes[1].pixel_stride,
	        (void *)frame->planes[2].data, frame->planes[2].row_stride, frame->planes[2].pixel_stride);
}

/*
 * A gralloc buffer that is nobody's atl_hardware_buffer: a camera HAL's own,
 * out of a PRIVATE Image. Only lockPlanes can say where the chroma is, and a
 * buffer with fewer than three planes is not one this can read.
 */
bool atl_gralloc_lock_planes(void *ahardwarebuffer, struct atl_window_frame *frame)
{
	struct hb_planes planes = {0};

	if (!ahardwarebuffer || !frame || !gralloc_load() || !gralloc.lock_planes || !gralloc.unlock)
		return false;
	if (gralloc.lock_planes(ahardwarebuffer, HB_USAGE_CPU_READ_OFTEN, -1, NULL, &planes) ||
	    planes.plane_count < 3)
		return false;
	for (int i = 0; i < 3; i++) {
		if (!planes.planes[i].data) {
			gralloc.unlock(ahardwarebuffer, NULL);
			return false;
		}
		frame->planes[i] = (struct atl_window_plane){
		    planes.planes[i].data,
		    planes.planes[i].row_stride,
		    planes.planes[i].pixel_stride,
		};
	}
	return true;
}

void atl_gralloc_unlock(void *ahardwarebuffer)
{
	if (ahardwarebuffer && gralloc.unlock)
		gralloc.unlock(ahardwarebuffer, NULL);
}

bool atl_hardware_buffer_map(void *handle, struct atl_window_frame *frame)
{
	struct atl_hardware_buffer *buffer = native_handle_lookup(handle);
	bool ycbcr;
	size_t bpp;
	void *mapped = NULL;

	if (!buffer || !frame)
		return false;

	memset(frame, 0, sizeof(*frame));
	g_mutex_lock(&buffer->lock);
	frame->width = buffer->width;
	frame->height = buffer->height;
	frame->format = buffer->format;
	ycbcr = buffer->format == HB_FORMAT_YCBCR_420_888;
	bpp = ycbcr ? 0 : format_size(buffer->format, 1, 1, 1);

	/*
	 * A buffer that is a window onto memory somebody else owns - an Image's
	 * pixels - is only ever as good as that memory, which its owner keeps up
	 * to date frame by frame. Its gralloc handle is a copy taken once, when
	 * the app first asked for one, so reading that back would present the
	 * frame as it was several frames ago, or as nothing at all.
	 *
	 * The gralloc mapping is the truth only for a buffer ATL allocated and
	 * handed over, where the app is entitled to have rendered into it with
	 * the GPU and never told the CPU copy.
	 */
	if (buffer->gralloc && !buffer->owns_data && buffer->data)
		goto cpu_copy;

	/* the owner's own mapping of a foreign buffer, while it is still there */
	if (buffer->have_mapped && ycbcr) {
		for (int i = 0; i < 3; i++)
			frame->planes[i] = buffer->mapped.planes[i];
		map_explain(buffer, frame, "the owner's mapped planes");
		g_mutex_unlock(&buffer->lock);
		return true;
	}

	if (buffer->gralloc && gralloc.describe && (bpp || ycbcr)) {
		struct hb_desc desc = {0};

		gralloc.describe(buffer->gralloc, &desc);
		if (ycbcr) {
			if (map_gralloc_planes(buffer, frame)) {
				buffer->gralloc_locked = true;
				map_explain(buffer, frame, "gralloc planes");
				g_mutex_unlock(&buffer->lock);
				return true;
			}
		} else if (gralloc.lock &&
		           !gralloc.lock(buffer->gralloc, HB_USAGE_CPU_READ_OFTEN, -1, NULL, &mapped) &&
		           mapped) {
			buffer->gralloc_locked = true;
			frame->pixels = mapped;
			frame->stride = (size_t)desc.stride * bpp;
			g_mutex_unlock(&buffer->lock);
			return true;
		}
		/* the CPU copy is all there is, and it is whatever ATL last put
		 * there - not what the app may have rendered with the GPU */
		gralloc_read_refused(buffer);
	}
cpu_copy:
	if (!buffer->data || (!bpp && !ycbcr)) {
		g_mutex_unlock(&buffer->lock);
		return false;
	}
	/* ATL's own frames are NV21, whatever else a 4:2:0 buffer can be */
	if (ycbcr) {
		/* a buffer too small to hold the chroma would have the reader
		 * running off the end of it, and its colours come from nowhere */
		if (buffer->size < (size_t)buffer->width * buffer->height * 3 / 2) {
			map_short_buffer(buffer);
			g_mutex_unlock(&buffer->lock);
			return false;
		}
		atl_window_frame_nv21(frame, buffer->data, buffer->width, buffer->height,
		                      (size_t)buffer->width);
		map_explain(buffer, frame, "the CPU copy as NV21");
	} else {
		frame->pixels = buffer->data;
		frame->stride = (size_t)buffer->width * bpp;
		map_explain(buffer, frame, "the CPU copy as a raster");
	}
	g_mutex_unlock(&buffer->lock);
	return true;
}

void atl_hardware_buffer_unmap(void *handle)
{
	struct atl_hardware_buffer *buffer = native_handle_lookup(handle);

	if (!buffer)
		return;
	g_mutex_lock(&buffer->lock);
	if (buffer->gralloc_locked && gralloc.unlock) {
		gralloc.unlock(buffer->gralloc, NULL);
		buffer->gralloc_locked = false;
	}
	g_mutex_unlock(&buffer->lock);
}

void atl_hardware_buffer_detach(struct atl_hardware_buffer *buffer)
{
	if (!buffer)
		return;
	g_mutex_lock(&buffer->lock);
	buffer->data = NULL;
	buffer->size = 0;
	buffer->have_mapped = false;
	g_mutex_unlock(&buffer->lock);
}

void atl_hardware_buffer_ref(struct atl_hardware_buffer *buffer)
{
	if (buffer)
		g_atomic_int_inc(&buffer->refcount);
}

struct atl_hardware_buffer *atl_hardware_buffer_wrap_native(void *ahardwarebuffer, int width,
                                                            int height, int format,
                                                            uint64_t usage,
                                                            const struct atl_window_frame *mapped)
{
	struct atl_hardware_buffer *buffer;
	struct hb_desc desc = {0};

	if (!ahardwarebuffer || !gralloc_load() || !gralloc.acquire)
		return NULL;
	/* the buffer's own description is the truth: a PRIVATE stream's gralloc
	 * format is whatever the HAL and gralloc agreed on */
	if (gralloc.describe) {
		gralloc.describe(ahardwarebuffer, &desc);
		if (desc.format)
			format = (int)desc.format;
		if (desc.width && desc.height) {
			width = (int)desc.width;
			height = (int)desc.height;
		}
		usage |= desc.usage;
	}
	buffer = buffer_new(width, height, format, 1, usage);
	gralloc.acquire(ahardwarebuffer);
	buffer->gralloc = ahardwarebuffer;
	buffer->gralloc_tried = true;
	if (mapped) {
		buffer->mapped = *mapped;
		buffer->have_mapped = true;
	}
	native_handle_register(buffer->gralloc, buffer);
	return buffer;
}

void atl_hardware_buffer_unref(struct atl_hardware_buffer *buffer)
{
	if (!buffer || !g_atomic_int_dec_and_test(&buffer->refcount))
		return;

	native_handle_forget(buffer);
	if (buffer->owns_data)
		free(buffer->data);
	if (buffer->gralloc && gralloc.release)
		gralloc.release(buffer->gralloc);
	g_mutex_clear(&buffer->lock);
	free(buffer);
}

jobject atl_hardware_buffer_to_java(JNIEnv *env, struct atl_hardware_buffer *buffer)
{
	jobject object;

	if (!buffer)
		return NULL;

	if (!buffer_class) {
		jclass class = (*env)->FindClass(env, "android/hardware/HardwareBuffer");

		if (class)
			buffer_from_native = (*env)->GetStaticMethodID(env, class, "fromNative",
			                                               "(JIIIIJ)Landroid/hardware/HardwareBuffer;");
		if (!class || !buffer_from_native) {
			fprintf(stderr, "HardwareBuffer: HardwareBuffer.fromNative not found\n");
			(*env)->ExceptionClear(env);
			return NULL;
		}
		buffer_class = (*env)->NewGlobalRef(env, class);
	}

	atl_hardware_buffer_ref(buffer);
	object = (*env)->CallStaticObjectMethod(env, buffer_class, buffer_from_native, _INTPTR(buffer),
	                                        buffer->width, buffer->height, buffer->format,
	                                        buffer->layers, (jlong)buffer->usage);
	if (!object)
		atl_hardware_buffer_unref(buffer);
	return object;
}

JNIEXPORT jlong JNICALL Java_android_hardware_HardwareBuffer_native_1create(JNIEnv *env, jclass class, jint width,
                                                                            jint height, jint format, jint layers,
                                                                            jlong usage)
{
	return _INTPTR(atl_hardware_buffer_alloc(width, height, format, layers, (uint64_t)usage));
}

JNIEXPORT void JNICALL Java_android_hardware_HardwareBuffer_native_1close(JNIEnv *env, jclass class, jlong ptr)
{
	atl_hardware_buffer_unref(_PTR(ptr));
}
