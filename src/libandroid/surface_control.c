/*
 * android/surface_control.h: SurfaceControl and its transactions, the NDK's
 * route to SurfaceFlinger.
 *
 * ATL has no SurfaceFlinger and no layer tree: a surface control here is a
 * handle on the ANativeWindow it was made from, and a transaction is the one
 * buffer it is about. Applying it presents that buffer through the window, so
 * an app that composes its frames itself and hands them to the compositor -
 * Google Camera's viewfinder does exactly that, through
 * SurfaceControlBufferFlinger - ends up drawing into its SurfaceView.
 *
 * What a real SurfaceFlinger would also do and this does not: place the layer
 * anywhere but over its own window (the destination rectangle is the window's
 * business here), stack layers, or hold a buffer across transactions. The
 * source rectangle and the transform are honoured, because a viewfinder that
 * ignores them shows the wrong crop the wrong way up.
 */

#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "native_window.h"
#include "window_frame.h"

typedef struct ASurfaceControl ASurfaceControl;
typedef struct ASurfaceTransaction ASurfaceTransaction;
struct AHardwareBuffer;

/* android/rect.h's, spelled out so this file needs none of the NDK headers */
typedef struct ARect {
	int32_t left;
	int32_t top;
	int32_t right;
	int32_t bottom;
} ARect;

struct ASurfaceControl {
	struct ANativeWindow *window;
	int refcount;
};

struct ASurfaceTransaction {
	ASurfaceControl *control;
	struct AHardwareBuffer *buffer;
	int acquire_fence;
	bool has_geometry;
	ARect source;
	int32_t transform;
	bool visible;
};

ASurfaceControl *ASurfaceControl_createFromWindow(struct ANativeWindow *parent, const char *debug_name)
{
	ASurfaceControl *control;

	if (!parent)
		return NULL;

	control = calloc(1, sizeof(*control));
	if (!control)
		return NULL;
	control->refcount = 1;
	control->window = parent;
	ANativeWindow_acquire(parent);
	return control;
}

/*
 * A child layer of a layer. ATL has one surface per control and no tree to
 * hang another off, so a child would have nowhere to draw; NULL is what the
 * NDK returns when a control cannot be made, and every caller checks it.
 */
ASurfaceControl *ASurfaceControl_create(ASurfaceControl *parent, const char *debug_name)
{
	static bool logged;

	if (!logged) {
		logged = true;
		fprintf(stderr, "ASurfaceControl: ATL has no layer tree, so a surface control "
		                "cannot have children\n");
	}
	return NULL;
}

void ASurfaceControl_acquire(ASurfaceControl *surface_control)
{
	if (surface_control)
		__atomic_add_fetch(&surface_control->refcount, 1, __ATOMIC_SEQ_CST);
}

void ASurfaceControl_release(ASurfaceControl *surface_control)
{
	if (!surface_control)
		return;
	if (__atomic_sub_fetch(&surface_control->refcount, 1, __ATOMIC_SEQ_CST))
		return;
	ANativeWindow_release(surface_control->window);
	free(surface_control);
}

ASurfaceTransaction *ASurfaceTransaction_create(void)
{
	ASurfaceTransaction *transaction = calloc(1, sizeof(*transaction));

	if (transaction) {
		transaction->acquire_fence = -1;
		transaction->visible = true;
	}
	return transaction;
}

void ASurfaceTransaction_delete(ASurfaceTransaction *transaction)
{
	free(transaction);
}

/*
 * The pixels behind an AHardwareBuffer, from the main library: on a device
 * that is a gralloc handle the app may have rendered into with the GPU, and
 * only the side that allocated it can map it back. Resolved by name because
 * this library does not link against that one.
 */
static bool buffer_map(struct AHardwareBuffer *buffer, struct atl_window_frame *frame)
{
	static bool (*map)(void *, struct atl_window_frame *);
	static bool resolved;

	if (!buffer)
		return false;
	if (!resolved) {
		resolved = true;
		map = dlsym(RTLD_DEFAULT, "atl_hardware_buffer_map");
		if (!map) {
			void *main_lib = dlopen("libtranslation_layer_main.so", RTLD_LAZY | RTLD_NOLOAD);

			if (main_lib)
				map = dlsym(main_lib, "atl_hardware_buffer_map");
		}
		if (!map)
			fprintf(stderr, "ASurfaceTransaction: no atl_hardware_buffer_map, so a "
			                "presented buffer cannot be read\n");
	}
	return map && map(buffer, frame);
}

static void buffer_unmap(struct AHardwareBuffer *buffer)
{
	static void (*unmap)(void *);
	static bool resolved;

	if (!resolved) {
		resolved = true;
		unmap = dlsym(RTLD_DEFAULT, "atl_hardware_buffer_unmap");
		if (!unmap) {
			void *main_lib = dlopen("libtranslation_layer_main.so", RTLD_LAZY | RTLD_NOLOAD);

			if (main_lib)
				unmap = dlsym(main_lib, "atl_hardware_buffer_unmap");
		}
	}
	if (unmap)
		unmap(buffer);
}

/*
 * ATL_DEBUG_PRESENT: a transaction that presents nothing does it silently -
 * every guard below is an early return - so count the reasons and say so.
 * Without this, "the viewfinder froze" cannot be told apart from "the app
 * stopped asking for one", and those want opposite fixes.
 */
enum apply_outcome {
	APPLY_PRESENTED,
	APPLY_NO_TRANSACTION,
	APPLY_NO_CONTROL,
	APPLY_NO_BUFFER,
	APPLY_INVISIBLE,
	APPLY_NO_MAP,
	APPLY_OUTCOMES,
};

static void apply_counted(enum apply_outcome outcome)
{
	static const char *const name[APPLY_OUTCOMES] = {
		"presented", "no transaction", "no control", "no buffer", "not visible", "unmappable",
	};
	static uint64_t count[APPLY_OUTCOMES];
	static uint64_t calls;
	static int on = -1;

	if (on < 0)
		on = getenv("ATL_DEBUG_PRESENT") != NULL;
	if (!on)
		return;

	count[outcome]++;
	/* the first few as they happen, then every 120: an app that applies once
	 * and stops is exactly the case this exists to catch, and waiting for a
	 * round number would hide it */
	if (++calls > 4 && calls % 120)
		return;

	fprintf(stderr, "ASurfaceTransaction: %llu applied -", (unsigned long long)calls);
	for (int i = 0; i < APPLY_OUTCOMES; i++)
		if (count[i])
			fprintf(stderr, " %s %llu,", name[i], (unsigned long long)count[i]);
	fprintf(stderr, "\n");
}

void ASurfaceTransaction_apply(ASurfaceTransaction *transaction)
{
	struct atl_window_frame frame;
	int32_t source[4];

	if (!transaction) {
		apply_counted(APPLY_NO_TRANSACTION);
		return;
	}
	if (!transaction->control) {
		apply_counted(APPLY_NO_CONTROL);
		return;
	}
	if (!transaction->buffer) {
		apply_counted(APPLY_NO_BUFFER);
		return;
	}
	if (!transaction->visible) {
		apply_counted(APPLY_INVISIBLE);
		return;
	}
	if (!buffer_map(transaction->buffer, &frame)) {
		apply_counted(APPLY_NO_MAP);
		return;
	}
	apply_counted(APPLY_PRESENTED);

	source[0] = transaction->source.left;
	source[1] = transaction->source.top;
	source[2] = transaction->source.right;
	source[3] = transaction->source.bottom;

	atl_native_window_present(transaction->control->window, &frame,
	                          transaction->has_geometry ? source : NULL, transaction->transform);
	buffer_unmap(transaction->buffer);
}

void ASurfaceTransaction_setBuffer(ASurfaceTransaction *transaction,
                                   ASurfaceControl *surface_control,
                                   struct AHardwareBuffer *buffer, int acquire_fence_fd)
{
	if (!transaction)
		return;
	transaction->control = surface_control;
	transaction->buffer = buffer;
	transaction->acquire_fence = acquire_fence_fd;
}

/* ATL draws in sRGB and nothing else, so a data space is recorded and ignored */
void ASurfaceTransaction_setBufferDataSpace(ASurfaceTransaction *transaction,
                                            ASurfaceControl *surface_control, int32_t data_space)
{
	if (transaction && surface_control)
		transaction->control = surface_control;
}

void ASurfaceTransaction_setVisibility(ASurfaceTransaction *transaction,
                                       ASurfaceControl *surface_control, int8_t visibility)
{
	if (!transaction)
		return;
	if (surface_control)
		transaction->control = surface_control;
	transaction->visible = visibility != 0;
}

/*
 * The rectangles are C++ references in the NDK header, so they arrive by
 * pointer whatever the declaration here looks like. `source` crops the buffer;
 * `destination` places the layer in its parent, which for ATL is the window
 * the frame is going into anyway.
 */
void ASurfaceTransaction_setGeometry(ASurfaceTransaction *transaction,
                                     ASurfaceControl *surface_control, const ARect *source,
                                     const ARect *destination, int32_t transform)
{
	if (!transaction)
		return;
	if (surface_control)
		transaction->control = surface_control;
	if (source) {
		transaction->source = *source;
		transaction->has_geometry = true;
	}
	transaction->transform = transform;
}
