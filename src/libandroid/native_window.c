/*
 * parts of this file originally from AOSP:
 *
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vulkan/vulkan_wayland.h>

// FIXME: move this together with the other stuff that doesn't belong in this file
#include <openxr/openxr.h>
#define XR_USE_PLATFORM_EGL
#include <openxr/openxr_platform.h>
#ifndef XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT
	#define XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT 1000426000
#endif

#include <dlfcn.h>

#include <jni.h>

// FIXME: put the header in a common place
#include "../api-impl-jni/defines.h"

#include "native_window.h"

/**
 * Transforms that can be applied to buffers as they are displayed to a window.
 *
 * Supported transforms are any combination of horizontal mirror, vertical
 * mirror, and clockwise 90 degree rotation, in that order. Rotations of 180
 * and 270 degrees are made up of those basic transforms.
 */
enum ANativeWindowTransform {
	ANATIVEWINDOW_TRANSFORM_IDENTITY = 0x00,
	ANATIVEWINDOW_TRANSFORM_MIRROR_HORIZONTAL = 0x01,
	ANATIVEWINDOW_TRANSFORM_MIRROR_VERTICAL = 0x02,
	ANATIVEWINDOW_TRANSFORM_ROTATE_90 = 0x04,

	ANATIVEWINDOW_TRANSFORM_ROTATE_180 = ANATIVEWINDOW_TRANSFORM_MIRROR_HORIZONTAL | ANATIVEWINDOW_TRANSFORM_MIRROR_VERTICAL,
	ANATIVEWINDOW_TRANSFORM_ROTATE_270 = ANATIVEWINDOW_TRANSFORM_ROTATE_180 | ANATIVEWINDOW_TRANSFORM_ROTATE_90,
};

typedef struct ANativeWindow ANativeWindow;

typedef struct ANativeWindow_Buffer {
	// The number of pixels that are show horizontally.
	int32_t width;

	// The number of pixels that are shown vertically.
	int32_t height;

	// The number of *pixels* that a line in the buffer takes in
	// memory. This may be >= width.
	int32_t stride;

	// The format of the buffer. One of AHARDWAREBUFFER_FORMAT_*
	int32_t format;

	// The actual bits.
	void *bits;

	// Do not touch.
	uint32_t reserved[6];
} ANativeWindow_Buffer;

void ANativeWindow_acquire(struct ANativeWindow *native_window)
{
	/* every entry point tolerates the NULL window ATL used to be the only
	 * source of, and an app that got one is entitled to release it */
	if (!native_window)
		return;
	__atomic_add_fetch(&native_window->refcount, 1, __ATOMIC_SEQ_CST);
}

void ANativeWindow_release(struct ANativeWindow *native_window)
{
	if (!native_window)
		return;
	/* the Wayland objects belong to the ATLSurfaceLayer, which destroys them
	 * on the main thread after surfaceDestroyed(); this can run on any thread */
	if (__atomic_sub_fetch(&native_window->refcount, 1, __ATOMIC_SEQ_CST) == 0)
		free(native_window);
}

struct ANativeWindow *atl_native_window_new(struct wl_display *display, struct wl_surface *surface,
                                            void *egl_window, int width, int height)
{
	struct ANativeWindow *window = calloc(1, sizeof(*window));

	if (!window)
		return NULL;
	/* one reference for the android.view.Surface that owns it */
	window->refcount = 1;
	window->wayland_display = display;
	window->wayland_surface = surface;
	window->egl_window = (EGLNativeWindowType)egl_window;
	window->width = width;
	window->height = height;
	return window;
}

/* the view (and so the layer's buffer) resized under a window the app may
 * already hold; the wl_egl_window itself is resized by the layer */
void atl_native_window_set_size(struct ANativeWindow *native_window, int width, int height)
{
	if (!native_window || width <= 0 || height <= 0)
		return;
	native_window->width = width;
	native_window->height = height;
}

void atl_native_window_detach(struct ANativeWindow *native_window)
{
	if (!native_window)
		return;
	native_window->egl_window = (EGLNativeWindowType)NULL;
	native_window->wayland_display = NULL;
	native_window->wayland_surface = NULL;
}

int32_t ANativeWindow_getWidth(struct ANativeWindow *native_window)
{
	return native_window ? native_window->width : -1;
}

int32_t ANativeWindow_getHeight(struct ANativeWindow *native_window)
{
	return native_window ? native_window->height : -1;
}

int32_t ANativeWindow_getFormat(ANativeWindow *window)
{
	return window ? 1 /* WINDOW_FORMAT_RGBA_8888 */ : -1;
}

int32_t ANativeWindow_setBuffersGeometry(ANativeWindow *window,
                                         int32_t width, int32_t height, int32_t format)
{
	if (!window)
		return -1;
	/* 0x0 means "whatever the window system picked", which is what the layer
	 * already sized the wl_egl_window to */
	if (width > 0 && height > 0) {
		window->width = width;
		window->height = height;
		if (window->egl_window)
			wl_egl_window_resize((struct wl_egl_window *)window->egl_window, width, height, 0, 0);
	}
	return 0;
}

typedef void ARect;

int32_t ANativeWindow_lock(ANativeWindow *window, ANativeWindow_Buffer *outBuffer,
                           ARect *inOutDirtyBounds)
{
	return -1;
}

int32_t ANativeWindow_unlockAndPost(ANativeWindow *window)
{
	return -1;
}

int32_t ANativeWindow_setFrameRate(ANativeWindow *window, float frameRate, int8_t compatibility)
{
	return 0; // success
}

int32_t ANativeWindow_setBuffersTransform(ANativeWindow *window, int32_t transform)
{
	return -1;
}

/*
 * The window behind an android.view.Surface. The SurfaceView built it when its
 * layer came up and cached it in the Surface's own field, so the several
 * fromSurface() calls an app makes about one surface are about one window -
 * which is also AOSP's identity, where the Surface *is* the ANativeWindow.
 */
ANativeWindow *ANativeWindow_fromSurface(JNIEnv *env, jobject surface)
{
	struct ANativeWindow *window;
	jclass class;
	jfieldID field;

	if (!env || !surface)
		return NULL;
	class = (*env)->GetObjectClass(env, surface);
	field = class ? (*env)->GetFieldID(env, class, "nativeWindow", "J") : NULL;
	if (!field) {
		(*env)->ExceptionClear(env);
		fprintf(stderr, "ANativeWindow_fromSurface: android.view.Surface has no window field\n");
		return NULL;
	}

	/* an app asks about one surface from more than one thread; the field is
	 * written on the main thread while the layer is being created */
	if ((*env)->MonitorEnter(env, surface) != JNI_OK)
		return NULL;
	window = (struct ANativeWindow *)(intptr_t)(*env)->GetLongField(env, surface, field);
	ANativeWindow_acquire(window);
	(*env)->MonitorExit(env, surface);

	if (!window)
		fprintf(stderr, "ANativeWindow_fromSurface: this surface has no layer behind it "
		                "(no wl_subcompositor, or the view is not attached yet)\n");
	return window;
}

ANativeWindow *ANativeWindow_fromSurfaceTexture(JNIEnv *env, jobject surfaceTexture)
{
	return NULL;
}

// FIXME 1.5: this most likely belongs elsewhere

VkResult bionic_vkCreateAndroidSurfaceKHR(VkInstance instance, const VkAndroidSurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	if (!pCreateInfo->window->wayland_display) {
		fprintf(stderr, "bionic_vkCreateAndroidSurfaceKHR: no wayland surface on the native window\n");
		return VK_ERROR_UNKNOWN;
	}

	VkWaylandSurfaceCreateInfoKHR wayland_create_info = {
		.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
		.display = pCreateInfo->window->wayland_display,
		.surface = pCreateInfo->window->wayland_surface,
	};

	return vkCreateWaylandSurfaceKHR(instance, &wayland_create_info, pAllocator, pSurface);
}

VkResult bionic_vkCreateInstance(VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
	int original_extension_count = pCreateInfo->enabledExtensionCount;
	int new_extension_count = original_extension_count + 1;
	const char **enabled_exts = malloc(new_extension_count * sizeof(char *));
	memcpy(enabled_exts, pCreateInfo->ppEnabledExtensionNames, original_extension_count * sizeof(char *));
	enabled_exts[original_extension_count] = "VK_KHR_wayland_surface";

	pCreateInfo->enabledExtensionCount = new_extension_count;
	pCreateInfo->ppEnabledExtensionNames = enabled_exts;
	return vkCreateInstance(pCreateInfo, pAllocator, pInstance);
}

PFN_vkVoidFunction bionic_vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
	if (__unlikely__(!strcmp(pName, "vkCreateInstance")))
		return (PFN_vkVoidFunction)bionic_vkCreateInstance;

	return vkGetInstanceProcAddr(instance, pName);
}

// FIXME 2: this BLATANTLY belongs elsewhere

typedef XrResult (*xr_func)(...);

/* avoid hard dependency on libopenxr_loader for the three functions that we only ever call when running a VR app */
/* NOTE: our use of __builtin_va_arg_pack means this only works as long as we don't need to call a function
 * that takes an integer type shorter than int or a floating point type shorter than double */
static void *openxr_loader_handle = NULL;
static inline __attribute__((__always_inline__)) XrResult xr_lazy_call(char *func_name, ...)
{
	if (!openxr_loader_handle) {
		openxr_loader_handle = dlopen("libopenxr_loader.so.1", RTLD_LAZY);
	}

	xr_func func = dlsym(openxr_loader_handle, func_name);
	return func(__builtin_va_arg_pack());
}

static XrResult bionic_xrInitializeLoaderKHR(void *loaderInitInfo)
{
	fprintf(stderr, "STUB: xrInitializeLoaderKHR noop called\n");
	return XR_SUCCESS;
}

struct XrGraphicsBindingOpenGLESAndroidKHR {
	XrStructureType type;
	const void *next;
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
};

XrResult bionic_xrCreateSession(XrInstance instance, XrSessionCreateInfo *createInfo, XrSession *session)
{
	struct XrGraphicsBindingOpenGLESAndroidKHR *android_bind = (struct XrGraphicsBindingOpenGLESAndroidKHR *)createInfo->next;
	XrGraphicsBindingEGLMNDX egl_bind = {XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};

	if (android_bind->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR) {
		egl_bind.getProcAddress = eglGetProcAddress;
		egl_bind.display = android_bind->display;
		egl_bind.config = android_bind->config;
		egl_bind.context = android_bind->context;
		createInfo->next = &egl_bind;
	} else {
		fprintf(stderr, "xrCreateSession: The graphics binding type = %d\n", android_bind->type);
	}

	return xr_lazy_call("xrCreateSession", instance, createInfo, session);
}

/*
 * Intercept XrInstanceProperties and notify the user of our meta-layer.
 */
XrResult bionic_xrGetInstanceProperties(XrInstance instance, XrInstanceProperties *instanceProperties)
{
	XrResult ret = xr_lazy_call("xrGetInstanceProperties", instance, instanceProperties);

	strncat(instanceProperties->runtimeName, " (With ATL meta-layer)",
	        XR_MAX_RUNTIME_NAME_SIZE - 1 - strlen(instanceProperties->runtimeName));

	return ret;
}

XrResult bionic_xrCreateInstance(XrInstanceCreateInfo *createInfo, XrInstance *instance)
{
	/* so that we can use simpler (and faster) code, we replace extensions which we
	 * want to remove with this extension rather than delete them and copy the following
	 * extensions over
	 */
	const char *harmless_extension = "XR_KHR_opengl_es_enable";

	const char *extra_exts[] = {
		"XR_MNDX_egl_enable",
		"XR_EXT_local_floor",
	};

	const char *const *old_names = createInfo->enabledExtensionNames;
	const char **new_names;
	int new_count = createInfo->enabledExtensionCount + ARRAY_SIZE(extra_exts);

	//FIXME: Leak?
	new_names = malloc(sizeof(*new_names) * new_count);
	memcpy(new_names, old_names, createInfo->enabledExtensionCount * sizeof(*old_names));

	for (int i = 0; i < createInfo->enabledExtensionCount; i++) {
		if (!strcmp(new_names[i], "XR_KHR_android_create_instance"))
			new_names[i] = harmless_extension;
	}

	for (int i = 0; i < ARRAY_SIZE(extra_exts); i++)
		new_names[createInfo->enabledExtensionCount + i] = extra_exts[i];

	createInfo->enabledExtensionCount = new_count;
	createInfo->enabledExtensionNames = new_names;

	fprintf(stderr, "## xrCreateInstance: Enabled extensions:\n");
	for (int i = 0; i < createInfo->enabledExtensionCount; ++i)
		fprintf(stderr, "## ---- %s\n", createInfo->enabledExtensionNames[i]);

	return xr_lazy_call("xrCreateInstance", createInfo, instance);
}

XrResult bionic_xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo *createInfo, XrSpace *space)
{
	fprintf(stderr, "xrCreateReferenceSpace(s=0x%w64x, info={rs_type=%d})\n", (uint64_t)session, createInfo->referenceSpaceType);

	//FIXME: this is sad for oculus refspace extension it assumes we have...
	if (createInfo->referenceSpaceType > 100)
		*(int *)(&createInfo->referenceSpaceType) = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;

	return xr_lazy_call("xrCreateReferenceSpace", session, createInfo, space);
}

/*
 * NOTE: Here we implement a NIH OpenXR API layer.
 *
 * We should make sure all our overrides are available via
 * "xrGetInstanceProcAddr" so the table below should contain
 * all our special functions.
 *
 * Maybe we should implement a proper OpenXR layer lib and inject
 * it in xrCreateInstance or extend our XR runtime in a way where
 * we won't need to help it.
 */

/*
 * HACK: The name prop here is deliberately an in-struct
 * string so we can do bsearch with plain strcmp.
 * So it must always be first.
 */
struct xr_proc_override {
	char name[64];
	PFN_xrVoidFunction *func;
};

#define XR_PROC_BIONIC(name) {#name, (void (**)(void))bionic_##name}

/* Please keep the alphabetical order */
static const struct xr_proc_override xr_proc_override_tbl[] = {
	XR_PROC_BIONIC(xrCreateInstance),
	XR_PROC_BIONIC(xrCreateReferenceSpace),
	XR_PROC_BIONIC(xrCreateSession),
	XR_PROC_BIONIC(xrGetInstanceProperties),
	XR_PROC_BIONIC(xrInitializeLoaderKHR),
};

XrResult bionic_xrGetInstanceProcAddr(XrInstance instance, const char *name, PFN_xrVoidFunction *func)
{
	printf("xrGetInstanceProcAddr(%s)\n", name);

	struct xr_proc_override *match = bsearch(name, xr_proc_override_tbl,
	                                         ARRAY_SIZE(xr_proc_override_tbl),
	                                         sizeof(xr_proc_override_tbl[0]),
	                                         (int (*)(const void *, const void *))strcmp);

	if (match) {
		*func = (PFN_xrVoidFunction)match->func;
		return XR_SUCCESS;
	}

	return xr_lazy_call("xrGetInstanceProcAddr", instance, name, func);
}

typedef void *ANativeActivity;

void ANativeActivity_setWindowFormat(ANativeActivity *activity, int32_t format)
{
	printf("STUB: %s called\n", __func__);
}
