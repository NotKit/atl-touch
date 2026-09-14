/*
 * camera2 NDK backend: ATL's cameras served by the device's own Android stack.
 *
 * libcamera2ndk.so and libmediandk.so are Android libraries, so they are loaded
 * with libhybris' android_dlopen()/android_dlsym() and not by the host loader.
 * There is no build-time hybris dependency: on a desktop nothing exports
 * android_dlopen, the backend reports itself unavailable and the gst backend
 * takes over. The vendored NDK headers under third_party/android-headers are the
 * API this talks to.
 *
 * One open camera is one ACameraDevice with one of two sessions on it. The
 * Camera1 session is a YUV stream (and a JPEG one once a still is asked for):
 * its preview frames arrive in an AImageReader and go two ways, straight into
 * the app's GL texture as an EGLImage over the frame's own gralloc buffer (the
 * zero-copy path, see "the zero-copy preview path" below), or repacked into
 * the NV21 every other ATL frame consumer expects. The camera2 session (see
 * "the stream session") is the app's own set of streams, one AImageReader per
 * output in the app's format, whose images are handed out as they are.
 * Static characteristics and per-frame results are the HAL's own metadata,
 * converted entry by entry into ATL's bags, vendor tags included, and the
 * Camera1 capability strings are derived from the same characteristics.
 */

#include <dlfcn.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <glib.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include <third_party/android-headers/android_compat.h>

#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraMetadataTags.h>
#include <camera/NdkCaptureRequest.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>

#include "camera2_metadata.h"
#include "camera_backend.h"
#include "camera_frame.h"

/* enough buffers that the HAL is never waiting on us, few enough that a stalled
 * consumer does not hold the whole pipeline */
#define PREVIEW_IMAGES 4
#define STILL_IMAGES   2

/* AHardwareBuffer usage bits (android/hardware_buffer.h values, spelled out so
 * this does not depend on the enum being visible): the frames are sampled by
 * the GPU on the zero-copy path and read by the CPU on the fallback one */
#define USAGE_CPU_READ_OFTEN     3ULL
#define USAGE_GPU_SAMPLED_IMAGE  (1ULL << 8)

/* EGL_ANDROID_image_native_buffer / GL_OES_EGL_image_external; the host EGL
 * headers are the Linux ones and know neither */
#ifndef EGL_NATIVE_BUFFER_ANDROID
#define EGL_NATIVE_BUFFER_ANDROID 0x3140
#endif
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

/*
 * The Halium libcamera2ndk reprocessing extension (NdkCameraReprocess.h),
 * declared here because the NDK headers ATL builds against never had it. The
 * symbols are platform-only and a device's library may predate them, so all
 * of them are optional and a missing one means "no reprocessing".
 */
typedef struct ACameraInputConfiguration {
	int32_t width;
	int32_t height;
	int32_t format;
	bool isMultiResolution;
	int32_t maxImages;
} ACameraInputConfiguration;

typedef void (*ACameraCaptureSession_inputBufferReleased)(void *context, AHardwareBuffer *buffer);

/* how many results are kept so a reprocess request can be built from the one
 * the app names, and how many input buffers may be in the camera at once */
#define RESULT_RING     16
#define MAX_INPUT_QUEUE 8

/* the NDK entry points, all resolved with android_dlsym */
static struct {
	void *camera2ndk;
	void *mediandk;

	ACameraManager *(*ACameraManager_create)(void);
	void (*ACameraManager_delete)(ACameraManager *);
	camera_status_t (*ACameraManager_getCameraIdList)(ACameraManager *, ACameraIdList **);
	void (*ACameraManager_deleteCameraIdList)(ACameraIdList *);
	camera_status_t (*ACameraManager_getCameraCharacteristics)(ACameraManager *, const char *,
	                                                           ACameraMetadata **);
	camera_status_t (*ACameraManager_openCamera)(ACameraManager *, const char *,
	                                            ACameraDevice_StateCallbacks *, ACameraDevice **);

	camera_status_t (*ACameraMetadata_getConstEntry)(const ACameraMetadata *, uint32_t,
	                                                 ACameraMetadata_const_entry *);
	camera_status_t (*ACameraMetadata_getAllTags)(const ACameraMetadata *, int32_t *, const uint32_t **);
	void (*ACameraMetadata_free)(ACameraMetadata *);

	camera_status_t (*ACameraDevice_close)(ACameraDevice *);
	camera_status_t (*ACameraDevice_createCaptureSession)(ACameraDevice *,
	                                                     const ACaptureSessionOutputContainer *,
	                                                     const ACameraCaptureSession_stateCallbacks *,
	                                                     ACameraCaptureSession **);
	camera_status_t (*ACameraDevice_createCaptureRequest)(ACameraDevice *,
	                                                      ACameraDevice_request_template,
	                                                      ACaptureRequest **);

	camera_status_t (*ACaptureSessionOutputContainer_create)(ACaptureSessionOutputContainer **);
	void (*ACaptureSessionOutputContainer_free)(ACaptureSessionOutputContainer *);
	camera_status_t (*ACaptureSessionOutputContainer_add)(ACaptureSessionOutputContainer *,
	                                                      const ACaptureSessionOutput *);
	camera_status_t (*ACaptureSessionOutput_create)(ANativeWindow *, ACaptureSessionOutput **);
	void (*ACaptureSessionOutput_free)(ACaptureSessionOutput *);

	camera_status_t (*ACameraOutputTarget_create)(ANativeWindow *, ACameraOutputTarget **);
	void (*ACameraOutputTarget_free)(ACameraOutputTarget *);
	camera_status_t (*ACaptureRequest_addTarget)(ACaptureRequest *, const ACameraOutputTarget *);
	camera_status_t (*ACaptureRequest_getConstEntry)(const ACaptureRequest *, uint32_t,
	                                                ACameraMetadata_const_entry *);
	camera_status_t (*ACaptureRequest_setEntry_u8)(ACaptureRequest *, uint32_t, uint32_t, const uint8_t *);
	camera_status_t (*ACaptureRequest_setEntry_i32)(ACaptureRequest *, uint32_t, uint32_t, const int32_t *);
	camera_status_t (*ACaptureRequest_setEntry_float)(ACaptureRequest *, uint32_t, uint32_t, const float *);
	camera_status_t (*ACaptureRequest_setEntry_i64)(ACaptureRequest *, uint32_t, uint32_t, const int64_t *);
	camera_status_t (*ACaptureRequest_setEntry_double)(ACaptureRequest *, uint32_t, uint32_t, const double *);
	camera_status_t (*ACaptureRequest_setEntry_rational)(ACaptureRequest *, uint32_t, uint32_t,
	                                                     const ACameraMetadata_rational *);
	void (*ACaptureRequest_free)(ACaptureRequest *);

	camera_status_t (*ACameraCaptureSession_setRepeatingRequest)(ACameraCaptureSession *,
	                                                             ACameraCaptureSession_captureCallbacks *,
	                                                             int, ACaptureRequest **, int *);
	camera_status_t (*ACameraCaptureSession_capture)(ACameraCaptureSession *,
	                                                ACameraCaptureSession_captureCallbacks *,
	                                                int, ACaptureRequest **, int *);
	camera_status_t (*ACameraCaptureSession_stopRepeating)(ACameraCaptureSession *);
	void (*ACameraCaptureSession_close)(ACameraCaptureSession *);

	/* the camera2 stream session; the API 28/29/33 ones are optional and
	 * NULL where the device's libcamera2ndk predates them */
	camera_status_t (*ACaptureSessionPhysicalOutput_create)(ANativeWindow *, const char *,
	                                                        ACaptureSessionOutput **);
	camera_status_t (*ACaptureRequest_setUserContext)(ACaptureRequest *, void *);
	camera_status_t (*ACaptureRequest_getUserContext)(const ACaptureRequest *, void **);
	camera_status_t (*ACameraCaptureSession_setRepeatingRequestV2)(ACameraCaptureSession *,
	                                                               ACameraCaptureSession_captureCallbacksV2 *,
	                                                               int, ACaptureRequest **, int *);
	camera_status_t (*ACameraCaptureSession_captureV2)(ACameraCaptureSession *,
	                                                  ACameraCaptureSession_captureCallbacksV2 *,
	                                                  int, ACaptureRequest **, int *);
	camera_status_t (*ACameraCaptureSession_abortCaptures)(ACameraCaptureSession *);
	media_status_t (*AImageReader_acquireNextImage)(AImageReader *, AImage **);

	/* the reprocessing extension; NULL on a stock libcamera2ndk */
	ACameraMetadata *(*ACameraMetadata_copy)(const ACameraMetadata *);
	camera_status_t (*ACameraDevice_createReprocessableCaptureSession)(
	    ACameraDevice *, const ACameraInputConfiguration *, const ACaptureSessionOutputContainer *,
	    const ACaptureRequest *, const ACameraCaptureSession_stateCallbacks *,
	    ACameraCaptureSession **);
	camera_status_t (*ACameraCaptureSession_queueInputBuffer)(ACameraCaptureSession *, AHardwareBuffer *,
	                                                          int64_t,
	                                                          ACameraCaptureSession_inputBufferReleased,
	                                                          void *);
	camera_status_t (*ACameraDevice_createReprocessCaptureRequest)(ACameraDevice *,
	                                                               const ACameraMetadata *,
	                                                               ACaptureRequest **);
	bool (*ACaptureRequest_isReprocess)(const ACaptureRequest *);

	media_status_t (*AImageReader_new)(int32_t, int32_t, int32_t, int32_t, AImageReader **);
	media_status_t (*AImageReader_newWithUsage)(int32_t, int32_t, int32_t, uint64_t, int32_t,
	                                           AImageReader **);
	void (*AImageReader_delete)(AImageReader *);
	media_status_t (*AImageReader_getWindow)(AImageReader *, ANativeWindow **);
	media_status_t (*AImageReader_setImageListener)(AImageReader *, AImageReader_ImageListener *);
	media_status_t (*AImageReader_acquireLatestImage)(AImageReader *, AImage **);

	void (*AImage_delete)(AImage *);
	media_status_t (*AImage_getWidth)(const AImage *, int32_t *);
	media_status_t (*AImage_getHeight)(const AImage *, int32_t *);
	media_status_t (*AImage_getNumberOfPlanes)(const AImage *, int32_t *);
	media_status_t (*AImage_getPlaneData)(const AImage *, int32_t, uint8_t **, int *);
	media_status_t (*AImage_getPlaneRowStride)(const AImage *, int32_t, int32_t *);
	media_status_t (*AImage_getPlanePixelStride)(const AImage *, int32_t, int32_t *);
	media_status_t (*AImage_getTimestamp)(const AImage *, int64_t *);
	media_status_t (*AImage_getHardwareBuffer)(const AImage *, AHardwareBuffer **);
} ndk;

/* the host EGL's Android extensions, resolved on first use; only an Android EGL
 * (which hybris EGL is) has them, so the desktop never leaves the CPU path */
static struct {
	EGLClientBuffer (*eglGetNativeClientBufferANDROID)(const AHardwareBuffer *);
	EGLImageKHR (*eglCreateImageKHR)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer,
	                                 const EGLint *);
	EGLBoolean (*eglDestroyImageKHR)(EGLDisplay, EGLImageKHR);
	void (*glEGLImageTargetTexture2DOES)(GLenum, void *);
} egl;

/* the characteristics of one camera, kept for as long as the process lives:
 * every ATL layer asks for them repeatedly and the HAL round trip is a binder
 * call */
struct camera_static {
	char *id;
	struct atl_camera_metadata *md;
	/* ATL_CAMERA2_KEYS_*: the HAL's own key lists, vendor tags appended to the
	 * characteristics one */
	uint32_t *keys[3];
	int n_keys[3];
};

static GMutex ndk_lock; /* the loader, the manager and the caches below */
static ACameraManager *manager;
static char **camera_ids;
static int n_camera_ids;
static GPtrArray *statics; /* struct camera_static * */

struct atl_camera {
	char *id;
	ACameraDevice *device;
	ACameraCaptureSession *session;
	ACaptureSessionOutputContainer *outputs;

	AImageReader *preview_reader;
	ANativeWindow *preview_window;
	ACaptureSessionOutput *preview_output;
	ACameraOutputTarget *preview_target;
	ACaptureRequest *preview_request;

	AImageReader *still_reader;
	ANativeWindow *still_window;
	ACaptureSessionOutput *still_output;
	ACameraOutputTarget *still_target;
	ACaptureRequest *still_request;

	/* what the session was built for, so start_preview knows when to rebuild */
	int session_width;
	int session_height;
	int session_still_width;
	int session_still_height;

	int width;  /* requested preview size, snapped to a supported one */
	int height;
	int fps_min; /* scaled by 1000, like Camera.Parameters */
	int fps_max;
	int display_orientation;
	bool previewing;

	GMutex lock;
	atl_camera_frame_cb frame_cb;
	void *frame_user;
	atl_camera_error_cb error_cb;
	void *error_user;
	atl_camera_autofocus_cb autofocus_cb;
	void *autofocus_user;
	atl_camera_jpeg_cb jpeg_cb;
	void *jpeg_user;

	/* the latest result the HAL sent, as a bag; get_result_metadata copies it */
	struct atl_camera_metadata *result;
	/* the latest request the app submitted: it usually arrives before there is
	 * a session to put it on, so it is kept and replayed onto every new one */
	struct atl_camera_metadata *request;

	uint8_t *nv21; /* the repacking buffer the frame callback is handed */
	size_t nv21_size;
	/* the largest frame the consumers of the moment need, under lock; 0 means
	 * nothing is known and the whole stream is repacked */
	int need_width;
	int need_height;
	int packed_step;        /* the step the last frame used, to log a change once */
	int64_t repack_micros;  /* what that repacking cost, since the last report */

	/* the session is being torn down: no listener may take another image, and
	 * the ones still running have to be waited for (see session_destroy) */
	bool closing;
	int callbacks_running;
	GCond idle;

	/* the zero-copy preview path; everything but pixels_needed is under lock */
	atl_camera_texture_cb texture_cb;
	void *texture_user;
	bool zero_copy;         /* a texture consumer is attached and the EGL side loaded */
	bool zero_copy_engaged; /* a frame has really reached the texture */
	bool pixels_needed;     /* something other than that texture wants the NV21 */
	AImage *tex_pending;    /* the newest frame, waiting for the GL thread */
	AImage *tex_current;    /* the frame the texture points at, held until replaced */
	EGLImageKHR tex_image;  /* its EGLImage */
	EGLDisplay tex_display;
	unsigned tex_name;

	/* caps, owned here and alive until close() */
	const struct camera_static *statics;
	GArray *preview_sizes;
	GArray *picture_sizes;
	GArray *fps_ranges;
	GString *focus_modes;
	GString *flash_modes;
	GString *scene_modes;
	GString *white_balance_modes;
	GString *color_effects;
	GString *antibanding_modes;
	struct atl_camera_caps caps;

	uint64_t frame_count;
	uint64_t cpu_frame_count; /* of those, the ones copied out to the CPU */
	char *dump_dir;

	/* the camera2 stream session (see "the stream session" below); a camera
	 * runs either this or the Camera1 session above, never both */
	ACameraCaptureSession *s_session;
	ACaptureSessionOutputContainer *s_outputs;
	struct ndk_stream *streams[ATL_CAMERA_MAX_STREAMS];
	int n_streams;
	struct atl_camera_stream_callbacks s_callbacks;
	void *s_user;
	ACameraCaptureSession_captureCallbacks s_cb;
	ACameraCaptureSession_captureCallbacksV2 s_cb_v2;
	bool s_v2; /* the HAL's own frame numbers, from the API 33 callbacks */
	int s_repeating_id;
	/* frame numbers: the HAL's where it tells them, this counter where it
	 * does not, and the started frames kept by timestamp so a result can be
	 * given its frame number and request back */
	int64_t s_next_frame;
	struct {
		int64_t timestamp;
		int64_t frame_number;
		int request_id;
	} s_started[64];
	int s_started_next;
	uint64_t s_buffers;
	int64_t s_rate_since;

	/* the reprocessing input this session was configured with, if any, and
	 * the real results of recent frames: a reprocess request is built from
	 * the one whose sensor timestamp the app names, which is why the bag the
	 * result callback hands upwards is not enough */
	bool s_has_input;
	struct atl_camera_stream_input s_input;
	struct {
		int64_t timestamp;
		ACameraMetadata *md;
	} s_results[RESULT_RING];
	int s_results_next;
	uint64_t s_reprocessed;
};

/* one buffer queued on the input stream, waiting for the camera to release it.
 * These live in a registry of their own rather than on the camera, so a
 * release that arrives after the session went does not touch freed memory. */
struct ndk_input {
	uint64_t token; /* what the release callback carries, never the pointer */
	struct atl_camera *camera;
	struct atl_camera_buffer *buffer;
	atl_camera_input_released_cb released;
	void *user;
};

static GMutex input_lock;
static GPtrArray *input_pending; /* struct ndk_input *, the live ones */
static uint64_t input_next_token = 1;

/* every entry point of the extension, or none of it */
static bool reprocess_available(void);
static const char *stream_format_name(int format);

/* one HAL output stream: an AImageReader and the session output built on it,
 * alive for as long as the session or any of its buffers is */
struct ndk_stream {
	gint refcount;
	struct atl_camera *camera;
	int index;
	struct atl_camera_stream config;
	char *physical_id;
	AImageReader *reader;
	ANativeWindow *window;
	ACaptureSessionOutput *output;
	ACameraOutputTarget *target;
	uint64_t delivered;
	uint64_t dropped;      /* the HAL said it would not fill this buffer */
	uint64_t unacquired;   /* the reader had an image ready and would not give it */
	int last_acquire_error;
};

/* one of its buffers out in the world */
struct ndk_buffer {
	struct atl_camera_buffer pub;
	AImage *image;
	struct ndk_stream *stream;
};

/* --- loading the Android libraries through libhybris --------------------- */

static void *(*android_dlopen_fn)(const char *, int);
static void *(*android_dlsym_fn)(void *, const char *);
static const char *(*android_dlerror_fn)(void);

static const char *ndk_dlerror(void)
{
	const char *error = android_dlerror_fn ? android_dlerror_fn() : NULL;

	return error ? error : "no error reported";
}

/*
 * libhybris' android loader. On a UT device libhybris-common is already in the
 * process (the session preloads it), so a plain symbol lookup finds it;
 * dlopen'ing it here would fail on its initial-exec TLS anyway.
 */
static bool hybris_loader_load(void)
{
	static const char *const sonames[] = {"libhybris-common.so.1", "libhybris-common.so"};

	android_dlopen_fn = dlsym(RTLD_DEFAULT, "android_dlopen");
	for (unsigned i = 0; !android_dlopen_fn && i < G_N_ELEMENTS(sonames); i++) {
		void *handle = dlopen(sonames[i], RTLD_NOW | RTLD_GLOBAL);

		if (!handle)
			continue;
		android_dlopen_fn = dlsym(handle, "android_dlopen");
	}
	if (!android_dlopen_fn) {
		fprintf(stderr, "Camera camera2ndk: no libhybris android loader in this process "
		                "(%s), so the Android camera2 libraries cannot be loaded\n", dlerror());
		return false;
	}
	android_dlsym_fn = dlsym(RTLD_DEFAULT, "android_dlsym");
	android_dlerror_fn = dlsym(RTLD_DEFAULT, "android_dlerror");
	return android_dlsym_fn != NULL;
}

/*
 * The binder thread pool has to be running before the camera is opened: the HAL
 * calls back into this process to dequeue the AImageReader's buffers, and
 * without a thread to serve those calls it times out on every frame
 * (requestStreamBuffer err:-110) and the preview never starts. The plain
 * libcamera2ndk.so does not start the pool itself, unlike its vendor variant.
 */
static bool binder_pool_start(void)
{
	void *lib = android_dlopen_fn("libbinder_ndk.so", RTLD_LAZY);
	void (*set_max)(uint32_t);
	void (*start)(void);

	if (!lib) {
		fprintf(stderr, "Camera camera2ndk: android_dlopen(libbinder_ndk.so) failed: %s\n",
		        ndk_dlerror());
		return false;
	}
	set_max = android_dlsym_fn(lib, "ABinderProcess_setThreadPoolMaxThreadCount");
	start = android_dlsym_fn(lib, "ABinderProcess_startThreadPool");
	if (!start) {
		fprintf(stderr, "Camera camera2ndk: libbinder_ndk.so has no "
		                "ABinderProcess_startThreadPool: %s\n", ndk_dlerror());
		return false;
	}
	if (set_max)
		set_max(4);
	start();
	return true;
}

static bool ndk_load(void)
{
	static bool tried;
	static bool ok;

	if (tried)
		return ok;
	tried = true;

	if (!hybris_loader_load())
		return false;

	/* libcamera2ndk.so pulls in libandroid_runtime.so, whose constructors abort
	 * in a host process on caiman: the process dies inside this dlopen with no
	 * message of its own, so say where we are and what is meant to prevent it
	 * (an empty libandroid_runtime.so on the Android linker's search path -
	 * usr/lib/hybris-stubs, see atl-env.sh) */
	fprintf(stderr, "Camera camera2ndk: loading the Android camera2 libraries, "
	                "HYBRIS_LD_LIBRARY_PATH=%s\n", getenv("HYBRIS_LD_LIBRARY_PATH") ?: "<unset>");
	ndk.camera2ndk = android_dlopen_fn("libcamera2ndk.so", RTLD_LAZY);
	if (!ndk.camera2ndk) {
		fprintf(stderr, "Camera camera2ndk: android_dlopen(libcamera2ndk.so) failed: %s\n",
		        ndk_dlerror());
		return false;
	}
	ndk.mediandk = android_dlopen_fn("libmediandk.so", RTLD_LAZY);
	if (!ndk.mediandk) {
		fprintf(stderr, "Camera camera2ndk: android_dlopen(libmediandk.so) failed: %s\n",
		        ndk_dlerror());
		return false;
	}

	bool complete = true;
#define SYM(lib, field)                                                       \
	do {                                                                      \
		ndk.field = android_dlsym_fn(ndk.lib, #field);                        \
		if (!ndk.field) {                                                     \
			fprintf(stderr, "Camera camera2ndk: %s is missing " #field "\n",  \
			        #lib);                                                    \
			complete = false;                                                 \
		}                                                                     \
	} while (0)
	SYM(camera2ndk, ACameraManager_create);
	SYM(camera2ndk, ACameraManager_delete);
	SYM(camera2ndk, ACameraManager_getCameraIdList);
	SYM(camera2ndk, ACameraManager_deleteCameraIdList);
	SYM(camera2ndk, ACameraManager_getCameraCharacteristics);
	SYM(camera2ndk, ACameraManager_openCamera);
	SYM(camera2ndk, ACameraMetadata_getConstEntry);
	SYM(camera2ndk, ACameraMetadata_getAllTags);
	SYM(camera2ndk, ACameraMetadata_free);
	SYM(camera2ndk, ACameraDevice_close);
	SYM(camera2ndk, ACameraDevice_createCaptureSession);
	SYM(camera2ndk, ACameraDevice_createCaptureRequest);
	SYM(camera2ndk, ACaptureSessionOutputContainer_create);
	SYM(camera2ndk, ACaptureSessionOutputContainer_free);
	SYM(camera2ndk, ACaptureSessionOutputContainer_add);
	SYM(camera2ndk, ACaptureSessionOutput_create);
	SYM(camera2ndk, ACaptureSessionOutput_free);
	SYM(camera2ndk, ACameraOutputTarget_create);
	SYM(camera2ndk, ACameraOutputTarget_free);
	SYM(camera2ndk, ACaptureRequest_addTarget);
	SYM(camera2ndk, ACaptureRequest_setEntry_u8);
	SYM(camera2ndk, ACaptureRequest_setEntry_i32);
	SYM(camera2ndk, ACaptureRequest_setEntry_float);
	SYM(camera2ndk, ACaptureRequest_setEntry_i64);
	SYM(camera2ndk, ACaptureRequest_setEntry_double);
	SYM(camera2ndk, ACaptureRequest_setEntry_rational);
	SYM(camera2ndk, ACaptureRequest_free);
	SYM(camera2ndk, ACameraCaptureSession_setRepeatingRequest);
	SYM(camera2ndk, ACameraCaptureSession_capture);
	SYM(camera2ndk, ACameraCaptureSession_stopRepeating);
	SYM(camera2ndk, ACameraCaptureSession_close);
	SYM(mediandk, AImageReader_new);
	SYM(mediandk, AImageReader_newWithUsage);
	SYM(mediandk, AImageReader_delete);
	SYM(mediandk, AImageReader_getWindow);
	SYM(mediandk, AImageReader_setImageListener);
	SYM(mediandk, AImageReader_acquireLatestImage);
	SYM(mediandk, AImage_delete);
	SYM(mediandk, AImage_getWidth);
	SYM(mediandk, AImage_getHeight);
	SYM(mediandk, AImage_getNumberOfPlanes);
	SYM(mediandk, AImage_getPlaneData);
	SYM(mediandk, AImage_getPlaneRowStride);
	SYM(mediandk, AImage_getPlanePixelStride);
	SYM(mediandk, AImage_getTimestamp);
	SYM(mediandk, AImage_getHardwareBuffer);
	SYM(mediandk, AImageReader_acquireNextImage);
#undef SYM
	if (!complete)
		return false;
#define SYM_OPT(lib, field) ndk.field = android_dlsym_fn(ndk.lib, #field)
	SYM_OPT(camera2ndk, ACaptureSessionPhysicalOutput_create);
	SYM_OPT(camera2ndk, ACaptureRequest_getConstEntry);
	SYM_OPT(camera2ndk, ACaptureRequest_setUserContext);
	SYM_OPT(camera2ndk, ACaptureRequest_getUserContext);
	SYM_OPT(camera2ndk, ACameraCaptureSession_setRepeatingRequestV2);
	SYM_OPT(camera2ndk, ACameraCaptureSession_captureV2);
	SYM_OPT(camera2ndk, ACameraCaptureSession_abortCaptures);
	SYM_OPT(camera2ndk, ACameraMetadata_copy);
	SYM_OPT(camera2ndk, ACameraDevice_createReprocessableCaptureSession);
	SYM_OPT(camera2ndk, ACameraCaptureSession_queueInputBuffer);
	SYM_OPT(camera2ndk, ACameraDevice_createReprocessCaptureRequest);
	SYM_OPT(camera2ndk, ACaptureRequest_isReprocess);
#undef SYM_OPT
	if (!reprocess_available())
		fprintf(stderr, "Camera camera2ndk: this libcamera2ndk has no reprocessing extension; "
		                "a session with an input cannot be configured\n");

	if (!binder_pool_start())
		return false;

	fprintf(stderr, "Camera camera2ndk: loaded libcamera2ndk.so and libmediandk.so\n");
	ok = true;
	return true;
}

/* the process-wide ACameraManager; call with ndk_lock held */
static ACameraManager *manager_get(void)
{
	if (!manager && ndk_load())
		manager = ndk.ACameraManager_create();
	return manager;
}

/* --- the HAL's metadata as ATL bags -------------------------------------- */

/* CAMERA_METADATA_INVALID_VENDOR_ID. With the plain global vendor tag
 * descriptor installed the lookups below fall through to it for any id; only
 * the per-provider descriptor *cache* needs the real one. */
#define VENDOR_ID_INVALID (~0ULL)

/* the vendor tag name lookups of the device's own libcamera_metadata.so; they
 * answer out of whatever global vendor tag descriptor libcamera2ndk installed
 * when its manager connected to the camera service */
static struct {
	bool tried;
	const char *(*section_name)(uint32_t tag, uint64_t id);
	const char *(*tag_name)(uint32_t tag, uint64_t id);
	int (*tag_type)(uint32_t tag, uint64_t id);
	uint64_t (*vendor_id_of)(const void *meta);
} vtag;

/* The id the per-provider descriptor cache files this HAL's tags under.
 * Invalid falls through to the plain global descriptor, which on caiman
 * names only a small subset (15 of 116); the cache has the rest. */
static uint64_t vendor_id = VENDOR_ID_INVALID;

static bool vendor_names_load(void)
{
	void *lib;

	if (vtag.tried)
		return vtag.section_name && vtag.tag_name;
	vtag.tried = true;

	lib = android_dlopen_fn("libcamera_metadata.so", RTLD_LAZY);
	if (!lib) {
		fprintf(stderr, "Camera camera2ndk: android_dlopen(libcamera_metadata.so) "
		                "failed: %s\n", ndk_dlerror());
		return false;
	}
	vtag.section_name = android_dlsym_fn(lib, "get_local_camera_metadata_section_name_vendor_id");
	vtag.tag_name = android_dlsym_fn(lib, "get_local_camera_metadata_tag_name_vendor_id");
	vtag.tag_type = android_dlsym_fn(lib, "get_local_camera_metadata_tag_type_vendor_id");
	vtag.vendor_id_of = android_dlsym_fn(lib, "get_camera_metadata_vendor_id");
	if (!vtag.section_name || !vtag.tag_name)
		fprintf(stderr, "Camera camera2ndk: libcamera_metadata.so has no vendor tag "
		                "name lookups, vendor tags stay readable by id only\n");
	return vtag.section_name && vtag.tag_name;
}

/* every page of [start, start + len) is mapped */
static bool span_mapped(uintptr_t start, size_t len, long page)
{
	unsigned char vec[4];
	uintptr_t first = start & ~(uintptr_t)(page - 1);
	size_t pages = (start + len - first + (size_t)page - 1) / (size_t)page;

	return pages <= sizeof(vec) && mincore((void *)first, pages * (size_t)page, vec) == 0;
}

/*
 * The descriptor cache is keyed by the metadata's vendor id, and the NDK never
 * says it. But every ACameraMetadata_getConstEntry data pointer lands inside
 * the raw camera_metadata_t, whose header carries the id: walk backwards to a
 * word that looks like the header (version 1, at offset 8 - or 4, should the
 * size fields ever be 32-bit) and let the device's own
 * get_camera_metadata_vendor_id read it. A wrong candidate cannot pass the
 * check that its id actually names the probe tag, which the plain descriptor
 * could not (get_local_..._vendor_id only consults the cache for a real id).
 */
static void vendor_id_discover(const ACameraMetadata *src, uint32_t probe_tag)
{
	ACameraMetadata_const_entry entry = {0};
	long page = sysconf(_SC_PAGESIZE);
	uintptr_t addr, base_page;
	unsigned char resident;

	if (!vtag.vendor_id_of || !vtag.tag_name || page <= 0)
		return;
	if (ndk.ACameraMetadata_getConstEntry(src, probe_tag, &entry) != ACAMERA_OK ||
	    !entry.data.u8)
		return;

	addr = (uintptr_t)entry.data.u8 & ~(uintptr_t)7;
	base_page = addr & ~(uintptr_t)(page - 1);
	for (size_t off = 0; off < (1u << 20); off += 8, addr -= 8) {
		if (addr < base_page) {
			base_page -= (uintptr_t)page;
			if (mincore((void *)base_page, (size_t)page, &resident) != 0)
				break; /* walked out of the mapping: no header found */
		}
		const uint32_t *words = (const uint32_t *)addr;

		if (words[1] != 1 && words[2] != 1)
			continue;
		if (!span_mapped(addr, 80, page)) /* the id is 72 bytes in */
			continue;
		uint64_t id = vtag.vendor_id_of((const void *)addr);

		if (id && id != VENDOR_ID_INVALID && vtag.tag_name(probe_tag, id)) {
			vendor_id = id;
			fprintf(stderr, "Camera camera2ndk: vendor tag id %llu, from the raw "
			                "metadata %zu bytes below entry data\n",
			        (unsigned long long)id, off);
			return;
		}
	}
	fprintf(stderr, "Camera camera2ndk: no raw metadata header found behind tag 0x%x, "
	                "vendor tags keep only the plain descriptor's names\n", probe_tag);
}

/* registers the tag's "com.vendor.section.name" (and type) so the app's
 * by-name Key lookups and writes resolve it; type < 0 asks the descriptor */
static bool vendor_tag_register(uint32_t tag, int type)
{
	const char *section, *name;
	char *full;

	if (atl_camera2_vendor_name(tag) &&
	    (type < 0 || atl_camera2_vendor_type(tag) >= 0))
		return true;
	if (!vendor_names_load())
		return false;
	section = vtag.section_name(tag, vendor_id);
	name = vtag.tag_name(tag, vendor_id);
	if (!section || !name)
		return false;
	if (type < 0 && vtag.tag_type)
		type = vtag.tag_type(tag, vendor_id);
	full = g_strdup_printf("%s.%s", section, name);
	atl_camera2_vendor_register(tag, full, type);
	g_free(full);
	return true;
}

/* one ACameraMetadata entry into the bag; the NDK type numbering is ATL's */
static void md_add_entry(struct atl_camera_metadata *md, const ACameraMetadata_const_entry *entry)
{
	atl_camera_metadata_add(md, entry->tag, entry->type, entry->data.u8, (int)entry->count);
}

/*
 * Every tag the HAL has, converted. A vendor tag (the Pixel HAL has hundreds)
 * is not in the generated table; its name and type go to the vendor registry,
 * where the by-name lookups find them, and one the descriptor cannot name
 * stays in the bag as the opaque-tag case, read by id.
 */
static struct atl_camera_metadata *md_from_acamera(const ACameraMetadata *src, int *n_vendor)
{
	struct atl_camera_metadata *md = atl_camera_metadata_new();
	const uint32_t *tags = NULL;
	int32_t n_tags = 0;

	if (!md)
		return NULL;
	if (ndk.ACameraMetadata_getAllTags(src, &n_tags, &tags) != ACAMERA_OK || !tags) {
		atl_camera_metadata_free(md);
		return NULL;
	}

	for (int32_t i = 0; i < n_tags; i++) {
		ACameraMetadata_const_entry entry = {0};

		if (ndk.ACameraMetadata_getConstEntry(src, tags[i], &entry) != ACAMERA_OK)
			continue;
		md_add_entry(md, &entry);
		if (!atl_camera2_tag_name(tags[i])) {
			vendor_tag_register(tags[i], entry.type);
			if (n_vendor)
				(*n_vendor)++;
		}
	}
	return md;
}

/*
 * Whether a vendor tag's name is one the hide list drops. A '!' pattern keeps
 * the tag whatever else matches, so a family can be hidden except for the few
 * tags an app genuinely needs.
 */
static bool vendor_name_matches(const char *name, char *const *patterns)
{
	bool hidden = false;

	if (!name || !patterns)
		return false;
	for (int i = 0; patterns[i]; i++) {
		const char *pattern = patterns[i];
		bool keep = *pattern == '!';

		if (keep)
			pattern++;
		if (!*pattern || !strstr(name, pattern))
			continue;
		if (keep)
			return false;
		hidden = true;
	}
	return hidden;
}

/* the tags of one of the HAL's key lists, plus the bag's own vendor tags for
 * the characteristics: an app that asks for the key list gets what it can read */
static uint32_t *keys_from_metadata(const struct atl_camera_metadata *md, uint32_t list_tag,
                                   bool with_vendor, int *count)
{
	const struct atl_camera_metadata_entry *entry = atl_camera_metadata_find(md, list_tag);
	const int32_t *tags = entry && entry->type == ATL_CAMERA2_TYPE_INT32 ? entry->data : NULL;
	int n_listed = tags ? entry->count : 0;
	int n_entries = atl_camera_metadata_n_entries(md);
	uint32_t *keys = g_new(uint32_t, (size_t)n_listed + (with_vendor ? (size_t)n_entries : 0) + 1);
	int n = 0;

	for (int i = 0; i < n_listed; i++)
		keys[n++] = (uint32_t)tags[i];

	for (int i = 0; with_vendor && i < n_entries; i++) {
		const struct atl_camera_metadata_entry *have = atl_camera_metadata_entry_at(md, i);
		bool listed = false;

		if (atl_camera2_tag_name(have->tag))
			continue; /* a known tag the HAL did not list is not ours to add */
		for (int j = 0; j < n && !listed; j++)
			listed = keys[j] == have->tag;
		if (!listed)
			keys[n++] = have->tag;
	}

	*count = n;
	return keys;
}

/*
 * ATL_CAMERA2_HIDE_PHYSICAL_IDS: comma-separated physical camera ids an app
 * never sees. The lever for a sub-camera the port cannot drive; caiman's
 * ultrawide was one until the Pixel 9 kernel fix.
 */
static bool physical_id_hidden(const char *id)
{
	const char *hide = g_getenv("ATL_CAMERA2_HIDE_PHYSICAL_IDS");
	char **ids;
	bool hidden = false;

	if (!hide || !*hide || !id)
		return false;
	ids = g_strsplit(hide, ",", -1);
	for (int i = 0; ids[i] && !hidden; i++)
		hidden = !strcmp(ids[i], id);
	g_strfreev(ids);
	return hidden;
}

/* the logical camera's physical id list less the hidden ones; the HAL writes
 * it as NUL-separated strings in one byte array */
static void physical_ids_hide(struct atl_camera_metadata *md, const char *id)
{
	const struct atl_camera_metadata_entry *entry =
	    atl_camera_metadata_find(md, ACAMERA_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS);
	GByteArray *kept;
	int dropped = 0;

	if (!entry || entry->type != ATL_CAMERA2_TYPE_BYTE)
		return;
	kept = g_byte_array_new();
	for (int at = 0; at < entry->count;) {
		const char *one = (const char *)entry->data + at;
		int len = (int)strnlen(one, (size_t)(entry->count - at));

		if (physical_id_hidden(one))
			dropped++;
		else
			g_byte_array_append(kept, (const guint8 *)one, (guint)len + 1);
		at += len + 1;
	}
	if (dropped) {
		fprintf(stderr, "Camera camera2ndk: camera '%s' hides %d of its physical camera id(s)\n",
		        id, dropped);
		if (kept->len)
			atl_camera_metadata_add(md, ACAMERA_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS,
			                        ATL_CAMERA2_TYPE_BYTE, kept->data, (int)kept->len);
		else
			atl_camera_metadata_remove(md, ACAMERA_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS);
	}
	g_byte_array_free(kept, TRUE);
}

/* the cached characteristics of one camera id, NULL when the HAL has no such
 * camera; call with ndk_lock held */
static const struct camera_static *camera_static_get(const char *id)
{
	struct camera_static *entry;
	ACameraMetadata *chars = NULL;
	ACameraManager *mgr = manager_get();
	int n_vendor = 0;

	if (!mgr || !id || physical_id_hidden(id))
		return NULL;
	if (!statics)
		statics = g_ptr_array_new();
	for (guint i = 0; i < statics->len; i++) {
		entry = g_ptr_array_index(statics, i);
		if (!strcmp(entry->id, id))
			return entry;
	}

	if (ndk.ACameraManager_getCameraCharacteristics(mgr, id, &chars) != ACAMERA_OK || !chars) {
		fprintf(stderr, "Camera camera2ndk: no characteristics for camera '%s'\n", id);
		return NULL;
	}

	entry = g_new0(struct camera_static, 1);
	entry->id = g_strdup(id);
	entry->md = md_from_acamera(chars, &n_vendor);

	/* most of a Pixel's vendor tags name only through the descriptor cache,
	 * keyed by a vendor id the NDK never says: dig it out of the raw buffer
	 * behind the first tag the plain descriptor could not name, then name
	 * the entries that first pass had to skip */
	if (entry->md && vendor_id == VENDOR_ID_INVALID) {
		for (int i = 0; i < atl_camera_metadata_n_entries(entry->md); i++) {
			const struct atl_camera_metadata_entry *have =
			    atl_camera_metadata_entry_at(entry->md, i);

			if (atl_camera2_tag_name(have->tag) || atl_camera2_vendor_name(have->tag))
				continue;
			vendor_id_discover(chars, have->tag);
			break;
		}
		if (vendor_id != VENDOR_ID_INVALID)
			for (int i = 0; i < atl_camera_metadata_n_entries(entry->md); i++) {
				const struct atl_camera_metadata_entry *have =
				    atl_camera_metadata_entry_at(entry->md, i);

				if (!atl_camera2_tag_name(have->tag))
					vendor_tag_register(have->tag, have->type);
			}
	}
	ndk.ACameraMetadata_free(chars);
	if (!entry->md) {
		g_free(entry->id);
		g_free(entry);
		return NULL;
	}

	/* ATL_CAMERA2_HIDE_VENDOR_TAGS: comma-separated substrings; a vendor tag
	 * whose full name contains one is dropped from the characteristics.
	 * The lever for app pipelines a vendor tag switches on that ATL cannot
	 * serve — Google Camera reads its Pixel tags and then builds a RAW
	 * reprocessing session, which the NDK has no API for.
	 *
	 * A substring prefixed with '!' is kept instead, whatever else matches:
	 * some of the tags in a family an app must not see carry the sensor's own
	 * calibration, and hiding those costs colour (Google Camera falls back to
	 * default tuning and a magenta cast). */
	const char *hide = g_getenv("ATL_CAMERA2_HIDE_VENDOR_TAGS");
	char **hide_patterns = hide && *hide ? g_strsplit(hide, ",", -1) : NULL;
	int n_hidden = 0;

	if (hide_patterns) {
		for (int i = atl_camera_metadata_n_entries(entry->md) - 1; i >= 0; i--) {
			const struct atl_camera_metadata_entry *have =
			    atl_camera_metadata_entry_at(entry->md, i);
			const char *name = have ? atl_camera2_vendor_name(have->tag) : NULL;

			if (vendor_name_matches(name, hide_patterns)) {
				atl_camera_metadata_remove(entry->md, have->tag);
				n_hidden++;
			}
		}
	}

	physical_ids_hide(entry->md, id);

	/* The HAL's characteristics describe the HAL, not ATL's camera2. Cutting
	 * them down to the formats ATL delivers is off by default because it
	 * makes Google Camera worse, not better: libgcam builds its own camera
	 * list out of the RAW configurations and answers "Create:
	 * static_metadata_list can not be empty" without them, so the app never
	 * opens a camera at all (measured on caiman). */
	if (!g_strcmp0(g_getenv("ATL_CAMERA2_NARROW_STREAMS"), "1")) {
		int dropped = atl_camera_metadata_narrow_streams(entry->md);

		if (dropped)
			fprintf(stderr, "Camera camera2ndk: camera '%s' narrowed to the formats "
			                "ATL delivers, %d stream configuration(s) dropped\n",
			        id, dropped);
	}

	entry->keys[ATL_CAMERA2_KEYS_CHARACTERISTICS] =
	    keys_from_metadata(entry->md, ACAMERA_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS, true,
	                       &entry->n_keys[ATL_CAMERA2_KEYS_CHARACTERISTICS]);
	entry->keys[ATL_CAMERA2_KEYS_REQUEST] =
	    keys_from_metadata(entry->md, ACAMERA_REQUEST_AVAILABLE_REQUEST_KEYS, false,
	                       &entry->n_keys[ATL_CAMERA2_KEYS_REQUEST]);
	entry->keys[ATL_CAMERA2_KEYS_RESULT] =
	    keys_from_metadata(entry->md, ACAMERA_REQUEST_AVAILABLE_RESULT_KEYS, false,
	                       &entry->n_keys[ATL_CAMERA2_KEYS_RESULT]);

	/* the request and result key lists carry vendor tags the characteristics
	 * have no entry for (a request key is not a characteristic); an app can
	 * only set one by name, so name them all — and drop the hidden ones, or
	 * the app's getAvailable*Keys() would still show them */
	int n_named = 0, n_listed_vendor = 0;
	for (int which = 0; which < 3; which++) {
		int kept = 0;

		for (int i = 0; i < entry->n_keys[which]; i++) {
			uint32_t tag = entry->keys[which][i];
			bool hidden = false;

			if (!atl_camera2_tag_name(tag)) {
				n_listed_vendor++;
				if (vendor_tag_register(tag, -1))
					n_named++;
				if (vendor_name_matches(atl_camera2_vendor_name(tag), hide_patterns)) {
					hidden = true;
					n_hidden++;
				}
			}
			if (!hidden)
				entry->keys[which][kept++] = tag;
		}
		entry->n_keys[which] = kept;
	}
	g_strfreev(hide_patterns);

	fprintf(stderr, "Camera camera2ndk: camera '%s' has %d metadata entries "
	                "(%d vendor tags), %d request and %d result keys, "
	                "%d of %d listed vendor tags named, %d hidden\n",
	        id, atl_camera_metadata_n_entries(entry->md), n_vendor,
	        entry->n_keys[ATL_CAMERA2_KEYS_REQUEST], entry->n_keys[ATL_CAMERA2_KEYS_RESULT],
	        n_named, n_listed_vendor, n_hidden);

	g_ptr_array_add(statics, entry);
	return entry;
}

/* --- Camera1 capabilities out of the characteristics --------------------- */

static const uint8_t *md_u8(const struct atl_camera_metadata *md, uint32_t tag, int *count)
{
	const struct atl_camera_metadata_entry *entry = atl_camera_metadata_find(md, tag);

	if (!entry || entry->type != ATL_CAMERA2_TYPE_BYTE)
		return NULL;
	if (count)
		*count = entry->count;
	return entry->data;
}

static const int32_t *md_i32(const struct atl_camera_metadata *md, uint32_t tag, int *count)
{
	const struct atl_camera_metadata_entry *entry = atl_camera_metadata_find(md, tag);

	if (!entry || entry->type != ATL_CAMERA2_TYPE_INT32)
		return NULL;
	if (count)
		*count = entry->count;
	return entry->data;
}

static const float *md_f32(const struct atl_camera_metadata *md, uint32_t tag, int *count)
{
	const struct atl_camera_metadata_entry *entry = atl_camera_metadata_find(md, tag);

	if (!entry || entry->type != ATL_CAMERA2_TYPE_FLOAT)
		return NULL;
	if (count)
		*count = entry->count;
	return entry->data;
}

/* append to a comma-joined Camera.Parameters value list, skipping duplicates */
static void append_mode(GString *s, const char *mode)
{
	for (const char *p = s->str; *p; p = strchr(p, ',') + 1) {
		size_t len = strcspn(p, ",");

		if (len == strlen(mode) && !strncmp(p, mode, len))
			return;
		if (!p[len])
			break;
	}
	if (s->len)
		g_string_append_c(s, ',');
	g_string_append(s, mode);
}

/* one HAL enum list -> the Camera1 strings for the values that have one */
static void append_mapped_modes(GString *s, const struct atl_camera_metadata *md, uint32_t tag,
                                const char *const *names, int n_names)
{
	int count = 0;
	const uint8_t *values = md_u8(md, tag, &count);

	for (int i = 0; values && i < count; i++)
		if (values[i] < n_names && names[values[i]])
			append_mode(s, names[values[i]]);
}

static gint size_cmp(gconstpointer a, gconstpointer b)
{
	const struct atl_camera_size *x = a, *y = b;

	return x->width * x->height - y->width * y->height;
}

/* the output sizes the HAL advertises for one format, smallest first (the same
 * order the gst backend uses, and preview_sizes[0] is a default) */
static void collect_stream_sizes(const struct atl_camera_metadata *md, int32_t format, GArray *sizes)
{
	int count = 0;
	const int32_t *configs = md_i32(md, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &count);

	for (int i = 0; configs && i + 3 < count; i += 4) {
		struct atl_camera_size size = {configs[i + 1], configs[i + 2]};

		if (configs[i] != format ||
		    configs[i + 3] != ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT)
			continue;
		g_array_append_val(sizes, size);
	}
	g_array_sort(sizes, size_cmp);
}

static void query_caps(struct atl_camera *camera)
{
	const struct atl_camera_metadata *md = camera->statics->md;
	const float *focal, *physical, *max_zoom;
	const int32_t *range;
	int count = 0;

	camera->preview_sizes = g_array_new(FALSE, FALSE, sizeof(struct atl_camera_size));
	camera->picture_sizes = g_array_new(FALSE, FALSE, sizeof(struct atl_camera_size));
	camera->fps_ranges = g_array_new(FALSE, FALSE, sizeof(struct atl_camera_fps_range));
	camera->focus_modes = g_string_new(NULL);
	camera->flash_modes = g_string_new(NULL);
	camera->scene_modes = g_string_new(NULL);
	camera->white_balance_modes = g_string_new(NULL);
	camera->color_effects = g_string_new(NULL);
	camera->antibanding_modes = g_string_new(NULL);

	collect_stream_sizes(md, AIMAGE_FORMAT_YUV_420_888, camera->preview_sizes);
	collect_stream_sizes(md, AIMAGE_FORMAT_JPEG, camera->picture_sizes);
	if (!camera->preview_sizes->len) {
		struct atl_camera_size fallback = {640, 480};

		g_array_append_val(camera->preview_sizes, fallback);
	}
	if (!camera->picture_sizes->len)
		g_array_append_vals(camera->picture_sizes, camera->preview_sizes->data,
		                    camera->preview_sizes->len);

	range = md_i32(md, ACAMERA_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, &count);
	for (int i = 0; range && i + 1 < count; i += 2) {
		struct atl_camera_fps_range fps = {range[i] * 1000, range[i + 1] * 1000};

		g_array_append_val(camera->fps_ranges, fps);
	}
	if (!camera->fps_ranges->len) {
		struct atl_camera_fps_range fps = {30000, 30000};

		g_array_append_val(camera->fps_ranges, fps);
	}
	camera->fps_min = g_array_index(camera->fps_ranges, struct atl_camera_fps_range,
	                                camera->fps_ranges->len - 1).min;
	camera->fps_max = g_array_index(camera->fps_ranges, struct atl_camera_fps_range,
	                                camera->fps_ranges->len - 1).max;

	static const char *const af_names[] = {
		[ACAMERA_CONTROL_AF_MODE_OFF] = "fixed",
		[ACAMERA_CONTROL_AF_MODE_AUTO] = "auto",
		[ACAMERA_CONTROL_AF_MODE_MACRO] = "macro",
		[ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO] = "continuous-video",
		[ACAMERA_CONTROL_AF_MODE_CONTINUOUS_PICTURE] = "continuous-picture",
		[ACAMERA_CONTROL_AF_MODE_EDOF] = "edof",
	};
	append_mapped_modes(camera->focus_modes, md, ACAMERA_CONTROL_AF_AVAILABLE_MODES, af_names,
	                    (int)G_N_ELEMENTS(af_names));

	static const char *const awb_names[] = {
		[ACAMERA_CONTROL_AWB_MODE_OFF] = NULL,
		[ACAMERA_CONTROL_AWB_MODE_AUTO] = "auto",
		[ACAMERA_CONTROL_AWB_MODE_INCANDESCENT] = "incandescent",
		[ACAMERA_CONTROL_AWB_MODE_FLUORESCENT] = "fluorescent",
		[ACAMERA_CONTROL_AWB_MODE_WARM_FLUORESCENT] = "warm-fluorescent",
		[ACAMERA_CONTROL_AWB_MODE_DAYLIGHT] = "daylight",
		[ACAMERA_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT] = "cloudy-daylight",
		[ACAMERA_CONTROL_AWB_MODE_TWILIGHT] = "twilight",
		[ACAMERA_CONTROL_AWB_MODE_SHADE] = "shade",
	};
	append_mapped_modes(camera->white_balance_modes, md, ACAMERA_CONTROL_AWB_AVAILABLE_MODES,
	                    awb_names, (int)G_N_ELEMENTS(awb_names));

	static const char *const effect_names[] = {
		[ACAMERA_CONTROL_EFFECT_MODE_OFF] = "none",
		[ACAMERA_CONTROL_EFFECT_MODE_MONO] = "mono",
		[ACAMERA_CONTROL_EFFECT_MODE_NEGATIVE] = "negative",
		[ACAMERA_CONTROL_EFFECT_MODE_SOLARIZE] = "solarize",
		[ACAMERA_CONTROL_EFFECT_MODE_SEPIA] = "sepia",
		[ACAMERA_CONTROL_EFFECT_MODE_POSTERIZE] = "posterize",
		[ACAMERA_CONTROL_EFFECT_MODE_WHITEBOARD] = "whiteboard",
		[ACAMERA_CONTROL_EFFECT_MODE_BLACKBOARD] = "blackboard",
		[ACAMERA_CONTROL_EFFECT_MODE_AQUA] = "aqua",
	};
	append_mapped_modes(camera->color_effects, md, ACAMERA_CONTROL_AVAILABLE_EFFECTS,
	                    effect_names, (int)G_N_ELEMENTS(effect_names));

	static const char *const scene_names[] = {
		[ACAMERA_CONTROL_SCENE_MODE_DISABLED] = "auto",
		[ACAMERA_CONTROL_SCENE_MODE_ACTION] = "action",
		[ACAMERA_CONTROL_SCENE_MODE_PORTRAIT] = "portrait",
		[ACAMERA_CONTROL_SCENE_MODE_LANDSCAPE] = "landscape",
		[ACAMERA_CONTROL_SCENE_MODE_NIGHT] = "night",
		[ACAMERA_CONTROL_SCENE_MODE_NIGHT_PORTRAIT] = "night-portrait",
		[ACAMERA_CONTROL_SCENE_MODE_THEATRE] = "theatre",
		[ACAMERA_CONTROL_SCENE_MODE_BEACH] = "beach",
		[ACAMERA_CONTROL_SCENE_MODE_SNOW] = "snow",
		[ACAMERA_CONTROL_SCENE_MODE_SUNSET] = "sunset",
		[ACAMERA_CONTROL_SCENE_MODE_STEADYPHOTO] = "steadyphoto",
		[ACAMERA_CONTROL_SCENE_MODE_FIREWORKS] = "fireworks",
		[ACAMERA_CONTROL_SCENE_MODE_SPORTS] = "sports",
		[ACAMERA_CONTROL_SCENE_MODE_PARTY] = "party",
		[ACAMERA_CONTROL_SCENE_MODE_CANDLELIGHT] = "candlelight",
		[ACAMERA_CONTROL_SCENE_MODE_BARCODE] = "barcode",
		[ACAMERA_CONTROL_SCENE_MODE_HDR] = "hdr",
	};
	append_mapped_modes(camera->scene_modes, md, ACAMERA_CONTROL_AVAILABLE_SCENE_MODES,
	                    scene_names, (int)G_N_ELEMENTS(scene_names));

	static const char *const antibanding_names[] = {
		[ACAMERA_CONTROL_AE_ANTIBANDING_MODE_OFF] = "off",
		[ACAMERA_CONTROL_AE_ANTIBANDING_MODE_50HZ] = "50hz",
		[ACAMERA_CONTROL_AE_ANTIBANDING_MODE_60HZ] = "60hz",
		[ACAMERA_CONTROL_AE_ANTIBANDING_MODE_AUTO] = "auto",
	};
	append_mapped_modes(camera->antibanding_modes, md,
	                    ACAMERA_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES, antibanding_names,
	                    (int)G_N_ELEMENTS(antibanding_names));

	const uint8_t *has_flash = md_u8(md, ACAMERA_FLASH_INFO_AVAILABLE, NULL);
	if (has_flash && has_flash[0]) {
		/* the AE modes say which of these the HAL will actually do, but every
		 * camera with a flash can be driven through FLASH_MODE */
		append_mode(camera->flash_modes, "off");
		append_mode(camera->flash_modes, "auto");
		append_mode(camera->flash_modes, "on");
		append_mode(camera->flash_modes, "torch");
	}

	/* horizontal/vertical field of view from the lens and the sensor it covers */
	float h_angle = 0.0f, v_angle = 0.0f;
	focal = md_f32(md, ACAMERA_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &count);
	physical = md_f32(md, ACAMERA_SENSOR_INFO_PHYSICAL_SIZE, NULL);
	if (focal && count >= 1 && physical && focal[0] > 0.0f) {
		h_angle = (float)(2.0 * atan2(physical[0] / 2.0, focal[0]) * 180.0 / G_PI);
		v_angle = (float)(2.0 * atan2(physical[1] / 2.0, focal[0]) * 180.0 / G_PI);
	}

	max_zoom = md_f32(md, ACAMERA_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, NULL);
	/* Camera1 zoom is an index into 1.0x + 0.1x steps (see the zoom-ratios list
	 * the parameter flattener builds) */
	int zoom_steps = max_zoom && max_zoom[0] > 1.0f ? (int)((max_zoom[0] - 1.0f) * 10.0f) : 0;

	const int32_t *ev_range = md_i32(md, ACAMERA_CONTROL_AE_COMPENSATION_RANGE, &count);
	const struct atl_camera_metadata_entry *ev_step =
	    atl_camera_metadata_find(md, ACAMERA_CONTROL_AE_COMPENSATION_STEP);
	float step = 0.0f;
	if (ev_step && ev_step->type == ATL_CAMERA2_TYPE_RATIONAL && ev_step->count >= 1) {
		const int32_t *rational = ev_step->data;

		if (rational[1])
			step = (float)rational[0] / (float)rational[1];
	}

	/* android.control.maxRegions is one int32[3], in (AE, AWB, AF) order */
	int n_regions = 0;
	const int32_t *regions = md_i32(md, ACAMERA_CONTROL_MAX_REGIONS, &n_regions);
	const int32_t *max_faces = md_i32(md, ACAMERA_STATISTICS_INFO_MAX_FACE_COUNT, NULL);

	bool stabilization = false;
	int n_stab = 0;
	const uint8_t *stab_modes = md_u8(md, ACAMERA_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES, &n_stab);
	for (int i = 0; stab_modes && i < n_stab; i++)
		stabilization |= stab_modes[i] == ACAMERA_CONTROL_VIDEO_STABILIZATION_MODE_ON;

	camera->caps = (struct atl_camera_caps){
		.preview_sizes = (const struct atl_camera_size *)camera->preview_sizes->data,
		.n_preview_sizes = camera->preview_sizes->len,
		.picture_sizes = (const struct atl_camera_size *)camera->picture_sizes->data,
		.n_picture_sizes = camera->picture_sizes->len,
		.fps_ranges = (const struct atl_camera_fps_range *)camera->fps_ranges->data,
		.n_fps_ranges = camera->fps_ranges->len,
		.focus_modes = camera->focus_modes->len ? camera->focus_modes->str : NULL,
		.flash_modes = camera->flash_modes->len ? camera->flash_modes->str : NULL,
		.scene_modes = camera->scene_modes->len ? camera->scene_modes->str : NULL,
		.white_balance_modes = camera->white_balance_modes->len ? camera->white_balance_modes->str : NULL,
		.color_effects = camera->color_effects->len ? camera->color_effects->str : NULL,
		.antibanding_modes = camera->antibanding_modes->len ? camera->antibanding_modes->str : NULL,
		.zoom_supported = zoom_steps > 0,
		.max_zoom = zoom_steps,
		.horizontal_view_angle = h_angle,
		.vertical_view_angle = v_angle,
		.min_exposure_compensation = ev_range && count >= 2 ? ev_range[0] : 0,
		.max_exposure_compensation = ev_range && count >= 2 ? ev_range[1] : 0,
		.exposure_compensation_step = step,
		.max_num_focus_areas = regions && n_regions >= 3 ? regions[2] : 0,
		.max_num_metering_areas = regions && n_regions >= 1 ? regions[0] : 0,
		.max_num_detected_faces = max_faces ? max_faces[0] : 0,
		.video_snapshot_supported = true,
		.video_stabilization_supported = stabilization,
	};
}

/* --- frames -------------------------------------------------------------- */

/*
 * One AImage of YUV_420_888 into the contiguous NV21 (Y plane, then interleaved
 * VU) the frame consumers expect - at the size the consumers actually asked
 * for, not the size of the stream. The two are far apart on a phone: Google
 * Camera's viewfinder reads 640x480 off a 4080x3072 sensor stream, and
 * repacking all 12.5 megapixels of every frame for it cost more than the
 * capture did.
 */
static bool image_to_nv21(struct atl_camera *camera, AImage *image, int *out_width, int *out_height)
{
	struct atl_camera_yuv420 src = {0};
	int32_t width = 0, height = 0, planes = 0;
	int32_t y_stride = 0, u_stride = 0, v_stride = 0, u_pixel = 0, v_pixel = 0;
	uint8_t *y = NULL, *u = NULL, *v = NULL;
	int y_len = 0, u_len = 0, v_len = 0;
	int need_width, need_height, step, packed_width, packed_height;
	size_t needed;

	if (ndk.AImage_getWidth(image, &width) != AMEDIA_OK ||
	    ndk.AImage_getHeight(image, &height) != AMEDIA_OK ||
	    ndk.AImage_getNumberOfPlanes(image, &planes) != AMEDIA_OK)
		return false;
	if (width < 2 || height < 2 || planes < 3)
		return false;

	if (ndk.AImage_getPlaneData(image, 0, &y, &y_len) != AMEDIA_OK ||
	    ndk.AImage_getPlaneData(image, 1, &u, &u_len) != AMEDIA_OK ||
	    ndk.AImage_getPlaneData(image, 2, &v, &v_len) != AMEDIA_OK)
		return false;
	if (ndk.AImage_getPlaneRowStride(image, 0, &y_stride) != AMEDIA_OK ||
	    ndk.AImage_getPlaneRowStride(image, 1, &u_stride) != AMEDIA_OK ||
	    ndk.AImage_getPlaneRowStride(image, 2, &v_stride) != AMEDIA_OK ||
	    ndk.AImage_getPlanePixelStride(image, 1, &u_pixel) != AMEDIA_OK ||
	    ndk.AImage_getPlanePixelStride(image, 2, &v_pixel) != AMEDIA_OK)
		return false;

	width &= ~1;
	height &= ~1;

	g_mutex_lock(&camera->lock);
	need_width = camera->need_width;
	need_height = camera->need_height;
	g_mutex_unlock(&camera->lock);

	step = atl_camera_yuv420_step(width, height, need_width, need_height);
	atl_camera_yuv420_size(width, height, step, &packed_width, &packed_height);

	needed = (size_t)packed_width * packed_height * 3 / 2;
	if (camera->nv21_size < needed) {
		camera->nv21 = g_realloc(camera->nv21, needed);
		camera->nv21_size = needed;
	}

	src = (struct atl_camera_yuv420){
		.y = y, .u = u, .v = v,
		.y_stride = y_stride, .u_stride = u_stride, .v_stride = v_stride,
		.u_pixel = u_pixel, .v_pixel = v_pixel,
		.y_len = y_len, .u_len = u_len, .v_len = v_len,
	};
	atl_camera_yuv420_to_nv21(camera->nv21, packed_width, packed_height, &src, step);

	if (camera->packed_step != step) {
		camera->packed_step = step;
		fprintf(stderr, "Camera camera2ndk: repacking the %dx%d stream at %dx%d "
		                "(step %d) for a %dx%d consumer\n",
		        width, height, packed_width, packed_height, step, need_width, need_height);
	}

	*out_width = packed_width;
	*out_height = packed_height;
	return true;
}

/*
 * An AImageReader listener is running. Its images belong to a reader the app
 * thread may be about to delete, and the NDK does not wait for us, so every
 * callback announces itself and session_destroy waits for the ones in flight.
 */
static bool listener_enter(struct atl_camera *camera)
{
	bool run;

	g_mutex_lock(&camera->lock);
	run = !camera->closing;
	if (run)
		camera->callbacks_running++;
	g_mutex_unlock(&camera->lock);
	return run;
}

static void listener_leave(struct atl_camera *camera)
{
	g_mutex_lock(&camera->lock);
	if (--camera->callbacks_running == 0)
		g_cond_broadcast(&camera->idle);
	g_mutex_unlock(&camera->lock);
}

static void stream_session_destroy(struct atl_camera *camera);

/* --- the zero-copy preview path ------------------------------------------ */

/*
 * The frame never leaves the gralloc buffer the HAL wrote it into: the AImage's
 * AHardwareBuffer becomes an EGLImage, which is bound to the app's texture as
 * GL_TEXTURE_EXTERNAL_OES on its own GL thread. Only an Android EGL has the two
 * extensions that takes, and hybris EGL is one - on a desktop they are missing
 * and the preview simply stays on the NV21 path.
 *
 * The reader thread parks the newest frame in tex_pending; the GL thread takes
 * it in update_preview_texture() and holds it (as tex_current) until the frame
 * after it is bound, so the HAL never overwrites the buffer being sampled.
 */
static bool egl_load(void)
{
	static bool tried;
	static bool ok;
	const char *disabled = getenv("ATL_CAMERA_ZERO_COPY");

	if (tried)
		return ok;
	tried = true;

	if (disabled && !atoi(disabled)) {
		fprintf(stderr, "Camera camera2ndk: zero-copy preview disabled by "
		                "ATL_CAMERA_ZERO_COPY, frames go through the CPU\n");
		return false;
	}

	egl.eglGetNativeClientBufferANDROID = (void *)eglGetProcAddress("eglGetNativeClientBufferANDROID");
	egl.eglCreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
	egl.eglDestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
	egl.glEGLImageTargetTexture2DOES = (void *)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	ok = egl.eglGetNativeClientBufferANDROID && egl.eglCreateImageKHR &&
	     egl.eglDestroyImageKHR && egl.glEGLImageTargetTexture2DOES;
	if (!ok)
		fprintf(stderr, "Camera camera2ndk: this EGL has no "
		                "EGL_ANDROID_get_native_client_buffer/GL_OES_EGL_image_external, "
		                "so the preview goes through the CPU\n");
	return ok;
}

/* drop whatever the fast path is holding; safe from any thread */
static void texture_release(struct atl_camera *camera)
{
	AImage *pending, *current;
	EGLImageKHR image;
	EGLDisplay display;

	g_mutex_lock(&camera->lock);
	pending = camera->tex_pending;
	current = camera->tex_current;
	image = camera->tex_image;
	display = camera->tex_display;
	camera->tex_pending = camera->tex_current = NULL;
	camera->tex_image = NULL;
	camera->zero_copy_engaged = false;
	camera->tex_name = 0;
	g_mutex_unlock(&camera->lock);

	/* an EGLImage belongs to the display, not to a context, so it can go from
	 * here; the texture keeps its own reference to what it was given */
	if (image && display)
		egl.eglDestroyImageKHR(display, image);
	if (pending)
		ndk.AImage_delete(pending);
	if (current)
		ndk.AImage_delete(current);
}

/* the app's GL thread, from SurfaceTexture.updateTexImage() */
static bool camera2ndk_update_preview_texture(struct atl_camera *camera, unsigned tex_name)
{
	AImage *image, *previous = NULL;
	AHardwareBuffer *buffer = NULL;
	EGLImageKHR egl_image, previous_image = NULL;
	EGLClientBuffer client;
	EGLDisplay display;
	GLenum error;

	if (!egl_load() || !tex_name)
		return false;

	g_mutex_lock(&camera->lock);
	image = camera->tex_pending;
	camera->tex_pending = NULL;
	g_mutex_unlock(&camera->lock);

	/* no new frame: the texture still holds the last one, as AOSP's does */
	if (!image)
		return true;

	display = eglGetCurrentDisplay();
	if (display == EGL_NO_DISPLAY) {
		fprintf(stderr, "Camera camera2ndk: updateTexImage without a current EGL context\n");
		goto fail;
	}
	if (ndk.AImage_getHardwareBuffer(image, &buffer) != AMEDIA_OK || !buffer) {
		fprintf(stderr, "Camera camera2ndk: the preview image has no AHardwareBuffer\n");
		goto fail;
	}
	client = egl.eglGetNativeClientBufferANDROID(buffer);
	egl_image = client ? egl.eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
	                                           client, NULL)
	                   : NULL;
	if (!egl_image) {
		fprintf(stderr, "Camera camera2ndk: no EGLImage for the camera buffer (EGL error 0x%x)\n",
		        eglGetError());
		goto fail;
	}

	glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex_name);
	egl.glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image);
	error = glGetError();
	if (error != GL_NO_ERROR) {
		fprintf(stderr, "Camera camera2ndk: GL error 0x%x binding the camera buffer to "
		                "texture %u\n", error, tex_name);
		egl.eglDestroyImageKHR(display, egl_image);
		goto fail;
	}

	g_mutex_lock(&camera->lock);
	if (camera->tex_name != tex_name) {
		glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		camera->tex_name = tex_name;
	}
	previous = camera->tex_current;
	previous_image = camera->tex_image;
	camera->tex_current = image;
	camera->tex_image = egl_image;
	camera->tex_display = display;
	if (!camera->zero_copy_engaged) {
		camera->zero_copy_engaged = true;
		fprintf(stderr, "Camera camera2ndk: zero-copy preview fast path on GL texture %u "
		                "(%dx%d, no CPU copy)\n", tex_name, camera->width, camera->height);
	}
	g_mutex_unlock(&camera->lock);

	/* the frame before it can go back to the HAL now */
	if (previous_image)
		egl.eglDestroyImageKHR(display, previous_image);
	if (previous)
		ndk.AImage_delete(previous);
	return true;

fail:
	ndk.AImage_delete(image);
	/* the SurfaceTexture drops the fast path for good on a false; put the NV21
	 * frames back for the consumers that are about to need them again */
	g_mutex_lock(&camera->lock);
	camera->zero_copy = false;
	camera->zero_copy_engaged = false;
	g_mutex_unlock(&camera->lock);
	atl_camera_set_external_textures(false);
	texture_release(camera);
	return false;
}

static void camera2ndk_set_texture_callback(struct atl_camera *camera, atl_camera_texture_cb cb,
                                            void *user)
{
	bool zero_copy = cb && egl_load();

	g_mutex_lock(&camera->lock);
	camera->texture_cb = cb;
	camera->texture_user = user;
	camera->zero_copy = zero_copy;
	g_mutex_unlock(&camera->lock);

	if (!cb)
		texture_release(camera);
	/* the app binds its SurfaceTexture name as GL_TEXTURE_EXTERNAL_OES, and
	 * with this path running that is what it has to reach */
	atl_camera_set_external_textures(zero_copy);
}

static void camera2ndk_set_frame_pixels_needed(struct atl_camera *camera, bool needed)
{
	g_mutex_lock(&camera->lock);
	camera->pixels_needed = needed;
	g_mutex_unlock(&camera->lock);
}

static void camera2ndk_set_frame_size_needed(struct atl_camera *camera, int width, int height)
{
	g_mutex_lock(&camera->lock);
	camera->need_width = width;
	camera->need_height = height;
	g_mutex_unlock(&camera->lock);
}

/* the AImageReader's own thread: the newest frame to the texture, the CPU, or
 * both */
static void on_preview_image(void *context, AImageReader *reader)
{
	struct atl_camera *camera = context;
	atl_camera_texture_cb texture_cb;
	atl_camera_frame_cb cb;
	void *texture_user, *cb_user;
	AImage *image = NULL, *stale = NULL;
	int width = 0, height = 0;
	bool to_texture, pixels;

	if (!listener_enter(camera))
		return;
	if (ndk.AImageReader_acquireLatestImage(reader, &image) != AMEDIA_OK || !image) {
		listener_leave(camera);
		return;
	}

	g_mutex_lock(&camera->lock);
	cb = camera->frame_cb;
	cb_user = camera->frame_user;
	texture_cb = camera->texture_cb;
	texture_user = camera->texture_user;
	to_texture = camera->zero_copy && texture_cb;
	/* until the GL thread has really bound a frame the CPU path is all there
	 * is, and a dump can only be written from it */
	pixels = !to_texture || !camera->zero_copy_engaged || camera->pixels_needed ||
	         camera->dump_dir != NULL;
	g_mutex_unlock(&camera->lock);

	camera->frame_count++;
	if (pixels) {
		int64_t entered = g_get_monotonic_time();

		camera->cpu_frame_count++;
		bool packed = image_to_nv21(camera, image, &width, &height);

		camera->repack_micros += g_get_monotonic_time() - entered;
		if (packed) {
			if (camera->frame_count == 1)
				fprintf(stderr, "Camera camera2ndk: first frame (%dx%d)\n", width, height);
			if (cb)
				cb(camera->nv21, width, height, width, cb_user);
			atl_camera_dump_frame(camera->dump_dir, camera->frame_count, camera->nv21,
			                      width, height, width);
		}
	}
	/* what the HAL is actually delivering and what the repack off it costs, to
	 * compare against what the app ends up presenting: a viewfinder at half
	 * rate is either the camera or everything after it, and the two want
	 * opposite fixes */
	if (camera->dump_dir || getenv("ATL_DEBUG_PRESENT")) {
		static int64_t marked;
		int64_t now = g_get_monotonic_time();

		if (!marked)
			marked = now;
		else if (camera->frame_count % 120 == 0) {
			double since = (now - marked) / 1e6;

			if (since > 0)
				fprintf(stderr, "Camera camera2ndk: %" G_GUINT64_FORMAT " frames from the HAL, "
				                "last 120 in %.1fs (%.1f/s), %" G_GUINT64_FORMAT " copied to the CPU "
				                "at %.1f ms each\n",
				        camera->frame_count, since, 120 / since, camera->cpu_frame_count,
				        camera->repack_micros / 1000.0 / 120);
			marked = now;
			camera->repack_micros = 0;
		}
	}
	if (!pixels) {
		if (camera->frame_count == 1)
			fprintf(stderr, "Camera camera2ndk: first frame (%dx%d)\n",
			        camera->width, camera->height);
		/* the pixels are the texture's; the frame is still one the session has
		 * to report */
		if (cb)
			cb(NULL, camera->width, camera->height, 0, cb_user);
	}

	if (to_texture) {
		g_mutex_lock(&camera->lock);
		stale = camera->tex_pending; /* the GL thread is behind: newest wins */
		camera->tex_pending = image;
		g_mutex_unlock(&camera->lock);
		if (stale)
			ndk.AImage_delete(stale);
		texture_cb(texture_user);
	} else {
		ndk.AImage_delete(image);
	}
	listener_leave(camera);
}

/* the still stream: the HAL's own JPEG, one plane of encoded bytes */
static void on_still_image(void *context, AImageReader *reader)
{
	struct atl_camera *camera = context;
	AImage *image = NULL;
	uint8_t *data = NULL;
	int size = 0;

	if (!listener_enter(camera))
		return;
	if (ndk.AImageReader_acquireLatestImage(reader, &image) != AMEDIA_OK || !image) {
		listener_leave(camera);
		return;
	}

	if (ndk.AImage_getPlaneData(image, 0, &data, &size) == AMEDIA_OK && data && size > 0) {
		g_mutex_lock(&camera->lock);
		atl_camera_jpeg_cb cb = camera->jpeg_cb;
		void *cb_user = camera->jpeg_user;
		camera->jpeg_cb = NULL;
		/* AOSP Camera1: takePicture stops the preview, startPreview resumes it */
		camera->previewing = false;
		g_mutex_unlock(&camera->lock);

		fprintf(stderr, "Camera camera2ndk: captured a picture, %d bytes of JPEG\n", size);
		if (cb)
			cb(data, (size_t)size, cb_user);
	}
	ndk.AImage_delete(image);
	listener_leave(camera);
}

/* --- device and session callbacks ---------------------------------------- */

static void report_error(struct atl_camera *camera)
{
	g_mutex_lock(&camera->lock);
	atl_camera_error_cb cb = camera->error_cb;
	void *cb_user = camera->error_user;
	g_mutex_unlock(&camera->lock);

	if (cb)
		cb(ATL_CAMERA_ERROR_SERVER_DIED, cb_user);
}

static void on_device_disconnected(void *context, ACameraDevice *device)
{
	(void)device;
	fprintf(stderr, "Camera camera2ndk: the camera was disconnected\n");
	report_error(context);
}

static void on_device_error(void *context, ACameraDevice *device, int error)
{
	(void)device;
	fprintf(stderr, "Camera camera2ndk: camera device error %d\n", error);
	report_error(context);
}

static void on_session_closed(void *context, ACameraCaptureSession *session)
{
	(void)context;
	(void)session;
}

static void on_session_ready(void *context, ACameraCaptureSession *session)
{
	(void)context;
	(void)session;
}

static void on_session_active(void *context, ACameraCaptureSession *session)
{
	(void)context;
	(void)session;
}

/* the HAL's result for one frame; the newest one is what get_result_metadata
 * reports, and the autofocus callback watches AF_STATE go past its trigger */
static void on_capture_completed(void *context, ACameraCaptureSession *session,
                                 ACaptureRequest *request, const ACameraMetadata *result)
{
	struct atl_camera *camera = context;
	struct atl_camera_metadata *md = md_from_acamera(result, NULL);
	const struct atl_camera_metadata_entry *af_state;
	atl_camera_autofocus_cb af_cb = NULL;
	void *af_user = NULL;
	bool focused = false;

	(void)session;
	(void)request;
	if (!md)
		return;

	af_state = atl_camera_metadata_find(md, ACAMERA_CONTROL_AF_STATE);

	g_mutex_lock(&camera->lock);
	atl_camera_metadata_free(camera->result);
	camera->result = md;
	if (camera->autofocus_cb && af_state && af_state->type == ATL_CAMERA2_TYPE_BYTE &&
	    af_state->count >= 1) {
		uint8_t state = ((const uint8_t *)af_state->data)[0];

		if (state == ACAMERA_CONTROL_AF_STATE_FOCUSED_LOCKED ||
		    state == ACAMERA_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED) {
			af_cb = camera->autofocus_cb;
			af_user = camera->autofocus_user;
			camera->autofocus_cb = NULL;
			focused = state == ACAMERA_CONTROL_AF_STATE_FOCUSED_LOCKED;
		}
	}
	g_mutex_unlock(&camera->lock);

	if (af_cb)
		af_cb(focused, af_user);
}

static void on_capture_failed(void *context, ACameraCaptureSession *session,
                              ACaptureRequest *request, ACameraCaptureFailure *failure)
{
	(void)context;
	(void)session;
	(void)request;
	fprintf(stderr, "Camera camera2ndk: capture of frame %" G_GINT64_FORMAT " failed\n",
	        failure ? (int64_t)failure->frameNumber : -1);
}

static void on_capture_started(void *context, ACameraCaptureSession *session,
                               const ACaptureRequest *request, int64_t timestamp)
{
	(void)context;
	(void)session;
	(void)request;
	(void)timestamp;
}

static void on_capture_progressed(void *context, ACameraCaptureSession *session,
                                  ACaptureRequest *request, const ACameraMetadata *result)
{
	(void)context;
	(void)session;
	(void)request;
	(void)result;
}

static void on_sequence_completed(void *context, ACameraCaptureSession *session, int sequence_id,
                                  int64_t frame_number)
{
	(void)context;
	(void)session;
	(void)sequence_id;
	(void)frame_number;
}

static void on_sequence_aborted(void *context, ACameraCaptureSession *session, int sequence_id)
{
	(void)context;
	(void)session;
	(void)sequence_id;
}

static void on_buffer_lost(void *context, ACameraCaptureSession *session, ACaptureRequest *request,
                           ANativeWindow *window, int64_t frame_number)
{
	(void)context;
	(void)session;
	(void)request;
	(void)window;
	(void)frame_number;
}

/* --- the session --------------------------------------------------------- */

static void request_apply_entry(ACaptureRequest *request,
                                const struct atl_camera_metadata_entry *entry)
{
	/* the NDK refuses tags that are not request keys; that is not an error
	 * here, the bag is whatever the app put in its CaptureRequest */
	switch (entry->type) {
	case ATL_CAMERA2_TYPE_BYTE:
		ndk.ACaptureRequest_setEntry_u8(request, entry->tag, entry->count, entry->data);
		break;
	case ATL_CAMERA2_TYPE_INT32:
		ndk.ACaptureRequest_setEntry_i32(request, entry->tag, entry->count, entry->data);
		break;
	case ATL_CAMERA2_TYPE_FLOAT:
		ndk.ACaptureRequest_setEntry_float(request, entry->tag, entry->count, entry->data);
		break;
	case ATL_CAMERA2_TYPE_INT64:
		ndk.ACaptureRequest_setEntry_i64(request, entry->tag, entry->count, entry->data);
		break;
	case ATL_CAMERA2_TYPE_DOUBLE:
		ndk.ACaptureRequest_setEntry_double(request, entry->tag, entry->count, entry->data);
		break;
	case ATL_CAMERA2_TYPE_RATIONAL:
		ndk.ACaptureRequest_setEntry_rational(request, entry->tag, entry->count, entry->data);
		break;
	}
}

/*
 * ATL_CAMERA2_DROP_REQUEST_TAGS: comma-separated substrings; an app's request
 * entry whose tag name contains one never reaches the HAL. Google Camera's
 * com.google.multicam.ST3AOverwriteSwitch is what asks the Pixel HAL for the
 * ultrawide as lead lens (UltraWideMacro, Wide) - see physical_id_hidden.
 */
static bool request_tag_dropped(uint32_t tag)
{
	static gsize once;
	static char **patterns;
	static uint32_t reported[8];
	const char *name;

	if (g_once_init_enter(&once)) {
		const char *s = g_getenv("ATL_CAMERA2_DROP_REQUEST_TAGS");

		patterns = s && *s ? g_strsplit(s, ",", -1) : NULL;
		g_once_init_leave(&once, 1);
	}
	if (!patterns)
		return false;
	name = atl_camera2_tag_name(tag);
	if (!name)
		name = atl_camera2_vendor_name(tag);
	if (!vendor_name_matches(name, patterns))
		return false;
	for (guint i = 0; i < G_N_ELEMENTS(reported); i++) {
		if (reported[i] == tag)
			break;
		if (!reported[i]) {
			reported[i] = tag;
			fprintf(stderr, "Camera camera2ndk: dropping %s from every request\n", name);
			break;
		}
	}
	return true;
}

/* the app's settings onto a request, less the dropped tags */
static void request_apply_settings(ACaptureRequest *request,
                                   const struct atl_camera_metadata *settings)
{
	int n_entries = atl_camera_metadata_n_entries(settings);

	for (int i = 0; i < n_entries; i++) {
		const struct atl_camera_metadata_entry *entry = atl_camera_metadata_entry_at(settings, i);

		if (!request_tag_dropped(entry->tag))
			request_apply_entry(request, entry);
	}
}

static void request_set_u8(ACaptureRequest *request, uint32_t tag, uint8_t value)
{
	if (request)
		ndk.ACaptureRequest_setEntry_u8(request, tag, 1, &value);
}

/* the fps range the caller asked for, on both requests */
static void request_apply_fps(struct atl_camera *camera)
{
	int32_t range[2] = {camera->fps_min / 1000, camera->fps_max / 1000};

	if (range[1] < 1)
		return;
	if (range[0] < 1)
		range[0] = range[1];
	if (camera->preview_request)
		ndk.ACaptureRequest_setEntry_i32(camera->preview_request,
		                                 ACAMERA_CONTROL_AE_TARGET_FPS_RANGE, 2, range);
}

/* the app's last request plus the fps range, onto the current preview request */
static void request_apply_all(struct atl_camera *camera)
{
	int n_entries = atl_camera_metadata_n_entries(camera->request);

	if (!camera->preview_request)
		return;
	for (int i = 0; i < n_entries; i++)
		request_apply_entry(camera->preview_request,
		                    atl_camera_metadata_entry_at(camera->request, i));
	request_apply_fps(camera);
}

static ACameraCaptureSession_captureCallbacks *capture_callbacks(struct atl_camera *camera)
{
	static ACameraCaptureSession_captureCallbacks callbacks;

	callbacks = (ACameraCaptureSession_captureCallbacks){
		.context = camera,
		.onCaptureStarted = on_capture_started,
		.onCaptureProgressed = on_capture_progressed,
		.onCaptureCompleted = on_capture_completed,
		.onCaptureFailed = on_capture_failed,
		.onCaptureSequenceCompleted = on_sequence_completed,
		.onCaptureSequenceAborted = on_sequence_aborted,
		.onCaptureBufferLost = on_buffer_lost,
	};
	return &callbacks;
}

/* one AImageReader plus the session output and request target built on it */
static bool stream_create(struct atl_camera *camera, int width, int height, int format,
                          int max_images, uint64_t usage, AImageReader_ImageCallback listener,
                          AImageReader **reader_out, ANativeWindow **window_out,
                          ACaptureSessionOutput **output_out, ACameraOutputTarget **target_out)
{
	AImageReader *reader = NULL;
	ANativeWindow *window = NULL;
	media_status_t status;

	if (usage)
		status = ndk.AImageReader_newWithUsage(width, height, format, usage, max_images, &reader);
	else
		status = ndk.AImageReader_new(width, height, format, max_images, &reader);
	if (status != AMEDIA_OK || !reader) {
		fprintf(stderr, "Camera camera2ndk: no %dx%d image reader for format 0x%x (%d)\n",
		        width, height, format, status);
		return false;
	}
	if (ndk.AImageReader_getWindow(reader, &window) != AMEDIA_OK || !window) {
		fprintf(stderr, "Camera camera2ndk: the %dx%d image reader has no window\n", width, height);
		ndk.AImageReader_delete(reader);
		return false;
	}

	AImageReader_ImageListener image_listener = {camera, listener};
	ndk.AImageReader_setImageListener(reader, &image_listener);

	if (ndk.ACaptureSessionOutput_create(window, output_out) != ACAMERA_OK ||
	    ndk.ACameraOutputTarget_create(window, target_out) != ACAMERA_OK) {
		fprintf(stderr, "Camera camera2ndk: could not make a session output of the "
		                "%dx%d stream\n", width, height);
		ndk.AImageReader_delete(reader);
		return false;
	}

	*reader_out = reader;
	*window_out = window;
	return true;
}

static void session_destroy(struct atl_camera *camera)
{
	/*
	 * Shut the listeners down first and wait for the one that may be running:
	 * its AImage belongs to a reader this function deletes, and deleting the
	 * reader underneath it wedges AImageReader_delete for good - which is how
	 * a preview into a SurfaceTexture used to leave the process stuck in
	 * CameraDevice.close(), whenever the app thread won that race.
	 */
	g_mutex_lock(&camera->lock);
	camera->closing = true;
	while (camera->callbacks_running)
		g_cond_wait(&camera->idle, &camera->lock);
	g_mutex_unlock(&camera->lock);
	if (camera->preview_reader)
		ndk.AImageReader_setImageListener(camera->preview_reader, NULL);
	if (camera->still_reader)
		ndk.AImageReader_setImageListener(camera->still_reader, NULL);

	/* the images the fast path holds belong to the reader that is about to go */
	texture_release(camera);

	if (camera->session) {
		ndk.ACameraCaptureSession_stopRepeating(camera->session);
		ndk.ACameraCaptureSession_close(camera->session);
		camera->session = NULL;
	}
	if (camera->preview_request) {
		ndk.ACaptureRequest_free(camera->preview_request);
		camera->preview_request = NULL;
	}
	if (camera->still_request) {
		ndk.ACaptureRequest_free(camera->still_request);
		camera->still_request = NULL;
	}
	if (camera->preview_target) {
		ndk.ACameraOutputTarget_free(camera->preview_target);
		camera->preview_target = NULL;
	}
	if (camera->still_target) {
		ndk.ACameraOutputTarget_free(camera->still_target);
		camera->still_target = NULL;
	}
	if (camera->preview_output) {
		ndk.ACaptureSessionOutput_free(camera->preview_output);
		camera->preview_output = NULL;
	}
	if (camera->still_output) {
		ndk.ACaptureSessionOutput_free(camera->still_output);
		camera->still_output = NULL;
	}
	if (camera->outputs) {
		ndk.ACaptureSessionOutputContainer_free(camera->outputs);
		camera->outputs = NULL;
	}
	/* the readers own their windows, so they go last */
	if (camera->preview_reader) {
		ndk.AImageReader_delete(camera->preview_reader);
		camera->preview_reader = NULL;
		camera->preview_window = NULL;
	}
	if (camera->still_reader) {
		ndk.AImageReader_delete(camera->still_reader);
		camera->still_reader = NULL;
		camera->still_window = NULL;
	}
	camera->session_width = camera->session_height = 0;
	camera->session_still_width = camera->session_still_height = 0;
	camera->previewing = false;
	camera->closing = false;
}

/*
 * (Re)build the capture session: a YUV stream at the preview size, plus a JPEG
 * stream when a still capture wants one. The NDK fixes a session's outputs when
 * it is created, so a new size means a new session. Called from the app thread
 * only, which is where every session call in this backend comes from.
 */
static bool session_create(struct atl_camera *camera, int still_width, int still_height)
{
	ACameraCaptureSession_stateCallbacks state = {
		.context = camera,
		.onClosed = on_session_closed,
		.onReady = on_session_ready,
		.onActive = on_session_active,
	};
	camera_status_t status;

	session_destroy(camera);
	stream_session_destroy(camera);

	if (ndk.ACaptureSessionOutputContainer_create(&camera->outputs) != ACAMERA_OK)
		return false;

	if (!stream_create(camera, camera->width, camera->height, AIMAGE_FORMAT_YUV_420_888,
	                   PREVIEW_IMAGES, USAGE_CPU_READ_OFTEN | USAGE_GPU_SAMPLED_IMAGE,
	                   on_preview_image, &camera->preview_reader, &camera->preview_window,
	                   &camera->preview_output, &camera->preview_target))
		goto fail;
	ndk.ACaptureSessionOutputContainer_add(camera->outputs, camera->preview_output);

	if (still_width > 0 && still_height > 0) {
		if (!stream_create(camera, still_width, still_height, AIMAGE_FORMAT_JPEG, STILL_IMAGES,
		                   0, on_still_image, &camera->still_reader, &camera->still_window,
		                   &camera->still_output, &camera->still_target))
			goto fail;
		ndk.ACaptureSessionOutputContainer_add(camera->outputs, camera->still_output);
	}

	status = ndk.ACameraDevice_createCaptureSession(camera->device, camera->outputs, &state,
	                                                &camera->session);
	if (status != ACAMERA_OK || !camera->session) {
		fprintf(stderr, "Camera camera2ndk: createCaptureSession failed (%d)\n", status);
		goto fail;
	}

	status = ndk.ACameraDevice_createCaptureRequest(camera->device, TEMPLATE_PREVIEW,
	                                                &camera->preview_request);
	if (status != ACAMERA_OK || !camera->preview_request) {
		fprintf(stderr, "Camera camera2ndk: createCaptureRequest failed (%d)\n", status);
		goto fail;
	}
	ndk.ACaptureRequest_addTarget(camera->preview_request, camera->preview_target);
	request_apply_all(camera);

	if (camera->still_target) {
		status = ndk.ACameraDevice_createCaptureRequest(camera->device, TEMPLATE_STILL_CAPTURE,
		                                                &camera->still_request);
		if (status != ACAMERA_OK || !camera->still_request)
			goto fail;
		ndk.ACaptureRequest_addTarget(camera->still_request, camera->still_target);
	}

	camera->session_width = camera->width;
	camera->session_height = camera->height;
	camera->session_still_width = still_width;
	camera->session_still_height = still_height;
	fprintf(stderr, "Camera camera2ndk: session with a %dx%d YUV stream", camera->width,
	        camera->height);
	if (camera->still_request)
		fprintf(stderr, " and a %dx%d JPEG stream", still_width, still_height);
	fprintf(stderr, "\n");
	return true;

fail:
	session_destroy(camera);
	return false;
}

static bool repeating_start(struct atl_camera *camera)
{
	int sequence = 0;
	camera_status_t status;

	status = ndk.ACameraCaptureSession_setRepeatingRequest(camera->session,
	                                                       capture_callbacks(camera), 1,
	                                                       &camera->preview_request, &sequence);
	if (status != ACAMERA_OK) {
		fprintf(stderr, "Camera camera2ndk: setRepeatingRequest failed (%d)\n", status);
		return false;
	}
	camera->previewing = true;
	return true;
}

/* --- the stream session --------------------------------------------------- */

/*
 * The camera2 half of this backend, in the NDK's own shape: every app output
 * is an AImageReader of the app's format and size, a session output (a
 * physical camera's where the app said so) and a request target; a request is
 * an ACaptureRequest with the app's settings and the targets of the streams it
 * names, tagged with the app's request id. The images are handed out as they
 * are - a RAW10 reader gets the sensor's RAW10, a PRIVATE one the gralloc
 * buffer the viewfinder samples - and each holds its stream alive until the
 * consumer releases it, so a reader is never deleted under an image.
 */

static void stream_unref(struct ndk_stream *stream)
{
	if (!g_atomic_int_dec_and_test(&stream->refcount))
		return;
	if (stream->target)
		ndk.ACameraOutputTarget_free(stream->target);
	if (stream->output)
		ndk.ACaptureSessionOutput_free(stream->output);
	/* the reader owns its window, so it goes last */
	if (stream->reader)
		ndk.AImageReader_delete(stream->reader);
	g_free(stream->physical_id);
	g_free(stream);
}

static void stream_buffer_release(struct atl_camera_buffer *pub)
{
	struct ndk_buffer *buffer = (struct ndk_buffer *)pub;

	ndk.AImage_delete(buffer->image);
	stream_unref(buffer->stream);
	free(buffer);
}

/* the AImageReader's own thread: the image, as it is, to the consumer */
static void on_stream_image(void *context, AImageReader *reader)
{
	struct ndk_stream *stream = context;
	struct atl_camera *camera = stream->camera;
	struct ndk_buffer *buffer;
	AImage *image = NULL;
	AHardwareBuffer *hardware = NULL;
	int32_t width = 0, height = 0, planes = 0;
	media_status_t status;

	if (!listener_enter(camera))
		return;
	status = ndk.AImageReader_acquireNextImage(reader, &image);
	if (status != AMEDIA_OK || !image) {
		/* separate from a HAL buffer lost: the callback fired, so the image
		 * was there, and the reader refused it - -30002 is every one of this
		 * reader's images still held by somebody */
		stream->unacquired++;
		if (status != stream->last_acquire_error) {
			stream->last_acquire_error = status;
			fprintf(stderr, "Camera camera2ndk: stream %d (%dx%d format 0x%x) could not "
			                "acquire its image (%d), %" G_GUINT64_FORMAT " so far\n",
			        stream->index, stream->config.width, stream->config.height,
			        stream->config.format, status, stream->unacquired);
		}
		listener_leave(camera);
		return;
	}
	stream->last_acquire_error = AMEDIA_OK;

	buffer = calloc(1, sizeof(*buffer));
	buffer->image = image;
	buffer->stream = stream;
	g_atomic_int_inc(&stream->refcount);
	buffer->pub.stream = stream->index;
	buffer->pub.format = stream->config.format;
	buffer->pub.width = stream->config.width;
	buffer->pub.height = stream->config.height;
	if (ndk.AImage_getWidth(image, &width) == AMEDIA_OK && width > 0)
		buffer->pub.width = width;
	if (ndk.AImage_getHeight(image, &height) == AMEDIA_OK && height > 0)
		buffer->pub.height = height;
	ndk.AImage_getTimestamp(image, &buffer->pub.timestamp);
	/* an opaque image has no planes a CPU may read; the rest are mapped by
	 * the first plane query and stay mapped until the image goes */
	if (stream->config.format != ATL_CAMERA_FORMAT_PRIVATE &&
	    ndk.AImage_getNumberOfPlanes(image, &planes) == AMEDIA_OK) {
		for (int i = 0; i < planes && i < ATL_CAMERA_MAX_PLANES; i++) {
			struct atl_camera_plane *plane = &buffer->pub.planes[i];
			uint8_t *data = NULL;
			int len = 0;
			int32_t row_stride = 0, pixel_stride = 0;

			if (ndk.AImage_getPlaneData(image, i, &data, &len) != AMEDIA_OK || !data)
				break;
			/* a blob and a packed raw plane have no strides in the NDK's
			 * eyes: a JPEG's bytes are the whole image, a RAW10 row has no
			 * pixel stride */
			if (ndk.AImage_getPlaneRowStride(image, i, &row_stride) != AMEDIA_OK)
				row_stride = 0;
			if (ndk.AImage_getPlanePixelStride(image, i, &pixel_stride) != AMEDIA_OK)
				pixel_stride = stream->config.format == ATL_CAMERA_FORMAT_JPEG ? 1 : 0;
			plane->data = data;
			plane->len = len;
			plane->row_stride = row_stride;
			plane->pixel_stride = pixel_stride;
			buffer->pub.n_planes = i + 1;
		}
	}
	if (ndk.AImage_getHardwareBuffer(image, &hardware) == AMEDIA_OK)
		buffer->pub.native = hardware;
	buffer->pub.release = stream_buffer_release;
	buffer->pub.owner = camera;

	stream->delivered++;
	camera->s_buffers++;
	if (stream->delivered == 1)
		fprintf(stderr, "Camera camera2ndk: first buffer on stream %d (%dx%d format 0x%x, "
		                "%d plane(s), %s)\n", stream->index, buffer->pub.width,
		        buffer->pub.height, buffer->pub.format, buffer->pub.n_planes,
		        buffer->pub.native ? "gralloc" : "no gralloc handle");
	/* ATL_CAMERA_DUMP_FRAMES: the 30th buffer of every stream with planes,
	 * once, as its bare plane 0 - a RAW10 stream's Bayer data can then be
	 * demosaiced on a desktop, which is the proof that it is sensor data */
	if (camera->dump_dir && stream->delivered == 30 && buffer->pub.n_planes > 0) {
		char *path = g_strdup_printf("%s/stream%d-fmt0x%x-%dx%d-stride%d.plane0", camera->dump_dir,
		                             stream->index, buffer->pub.format, buffer->pub.width,
		                             buffer->pub.height, buffer->pub.planes[0].row_stride);
		FILE *f = fopen(path, "wb");

		if (f) {
			fwrite(buffer->pub.planes[0].data, 1, buffer->pub.planes[0].len, f);
			fclose(f);
			fprintf(stderr, "Camera camera2ndk: wrote %s (%d bytes)\n", path,
			        buffer->pub.planes[0].len);
		}
		g_free(path);
	}
	/* ATL_DEBUG_PRESENT: the rate the HAL fills stream 0 at, every 120 of its
	 * buffers, to sit beside what the app then presents */
	if (stream->index == 0 && stream->delivered % 120 == 0 && getenv("ATL_DEBUG_PRESENT")) {
		int64_t now = g_get_monotonic_time();

		if (camera->s_rate_since)
			fprintf(stderr, "Camera camera2ndk: stream 0 filled 120 times in %.1fs (%.1f/s), "
			                "%" G_GUINT64_FORMAT " buffers on all streams\n",
			        (now - camera->s_rate_since) / 1e6, 120e6 / (now - camera->s_rate_since),
			        camera->s_buffers);
		camera->s_rate_since = now;
	}
	camera->s_callbacks.buffer(camera->s_user, &buffer->pub);
	listener_leave(camera);
}

/* the app's request id a callback's request carries */
static int request_id_of(struct atl_camera *camera, const ACaptureRequest *request)
{
	void *context = NULL;

	if (request && ndk.ACaptureRequest_getUserContext &&
	    ndk.ACaptureRequest_getUserContext(request, &context) == ACAMERA_OK)
		return (int)(intptr_t)context;
	return camera->s_repeating_id;
}

/* call with the lock held */
static void started_record_locked(struct atl_camera *camera, int64_t timestamp,
                                  int64_t frame_number, int request_id)
{
	int i = camera->s_started_next++ % G_N_ELEMENTS(camera->s_started);

	camera->s_started[i].timestamp = timestamp;
	camera->s_started[i].frame_number = frame_number;
	camera->s_started[i].request_id = request_id;
}

static bool started_find_locked(struct atl_camera *camera, int64_t timestamp,
                                int64_t *frame_number, int *request_id)
{
	if (!timestamp)
		return false;
	for (unsigned i = 0; i < G_N_ELEMENTS(camera->s_started); i++) {
		if (camera->s_started[i].timestamp == timestamp) {
			*frame_number = camera->s_started[i].frame_number;
			*request_id = camera->s_started[i].request_id;
			return true;
		}
	}
	return false;
}

/*
 * A reprocess capture carries the sensor timestamp of the frame it is
 * reprocessing, not one of its own, so it must stay out of the timestamp
 * pairing below: the frame it names already has a started event of the
 * original capture's, and matching against that would report the reprocess
 * result as the repeating request's.
 */
static bool request_is_reprocess(const ACaptureRequest *request)
{
	return request && ndk.ACaptureRequest_isReprocess && ndk.ACaptureRequest_isReprocess(request);
}

static void stream_started(struct atl_camera *camera, const ACaptureRequest *request,
                           int64_t timestamp, int64_t frame_number)
{
	int request_id = request_id_of(camera, request);

	g_mutex_lock(&camera->lock);
	if (frame_number < 0)
		frame_number = camera->s_next_frame++;
	else if (frame_number >= camera->s_next_frame)
		camera->s_next_frame = frame_number + 1;
	if (!request_is_reprocess(request))
		started_record_locked(camera, timestamp, frame_number, request_id);
	g_mutex_unlock(&camera->lock);
	camera->s_callbacks.started(camera->s_user, request_id, frame_number, timestamp);
}

static void on_stream_started(void *context, ACameraCaptureSession *session,
                              const ACaptureRequest *request, int64_t timestamp)
{
	(void)session;
	stream_started(context, request, timestamp, -1);
}

static void on_stream_started_v2(void *context, ACameraCaptureSession *session,
                                 const ACaptureRequest *request, int64_t timestamp,
                                 int64_t frame_number)
{
	(void)session;
	stream_started(context, request, timestamp, frame_number);
}

static bool reprocess_available(void)
{
	return ndk.ACameraDevice_createReprocessableCaptureSession &&
	       ndk.ACameraCaptureSession_queueInputBuffer &&
	       ndk.ACameraDevice_createReprocessCaptureRequest && ndk.ACameraMetadata_copy;
}

/*
 * The result ring: a copy of the HAL's own metadata for each of the last
 * RESULT_RING frames. A reprocess request has to be built from the real
 * ACameraMetadata of the frame being reprocessed - the bag ATL passes up to
 * Java has lost the vendor sections the HAL needs back. Call with the lock.
 */
static void result_keep_locked(struct atl_camera *camera, int64_t timestamp,
                               const ACameraMetadata *result)
{
	int slot = camera->s_results_next % RESULT_RING;

	if (camera->s_results[slot].md)
		ndk.ACameraMetadata_free(camera->s_results[slot].md);
	camera->s_results[slot].md = ndk.ACameraMetadata_copy(result);
	camera->s_results[slot].timestamp = camera->s_results[slot].md ? timestamp : 0;
	camera->s_results_next++;
}

/* call with the lock held; the metadata stays the ring's */
static const ACameraMetadata *result_find_locked(struct atl_camera *camera, int64_t timestamp)
{
	for (int i = 0; i < RESULT_RING; i++)
		if (camera->s_results[i].md && camera->s_results[i].timestamp == timestamp)
			return camera->s_results[i].md;
	return NULL;
}

static void results_clear(struct atl_camera *camera)
{
	g_mutex_lock(&camera->lock);
	for (int i = 0; i < RESULT_RING; i++) {
		if (camera->s_results[i].md)
			ndk.ACameraMetadata_free(camera->s_results[i].md);
		camera->s_results[i].md = NULL;
		camera->s_results[i].timestamp = 0;
	}
	camera->s_results_next = 0;
	g_mutex_unlock(&camera->lock);
}

/*
 * The pending-input registry: takes an entry out if it is still live, so
 * exactly one of the release callback and the teardown gets each buffer. The
 * callback carries a token rather than the pointer, because a release that
 * arrives after teardown would otherwise match a later entry that malloc
 * happened to put at the same address.
 */
static struct ndk_input *input_claim(uint64_t token)
{
	struct ndk_input *claimed = NULL;

	g_mutex_lock(&input_lock);
	for (guint i = 0; input_pending && i < input_pending->len; i++) {
		struct ndk_input *in = g_ptr_array_index(input_pending, i);

		if (in->token != token)
			continue;
		claimed = in;
		g_ptr_array_remove_index_fast(input_pending, i);
		break;
	}
	g_mutex_unlock(&input_lock);
	return claimed;
}

static void input_done(struct ndk_input *in)
{
	if (in->released)
		in->released(in->user, in->buffer);
	else if (in->buffer && in->buffer->release)
		in->buffer->release(in->buffer);
	g_free(in);
}

/* a binder thread: the camera is done with the buffer the app queued */
static void on_input_released(void *context, AHardwareBuffer *hardware)
{
	struct ndk_input *in = input_claim((uint64_t)(uintptr_t)context);

	(void)hardware;
	if (in) /* else the session was torn down and gave the buffer back already */
		input_done(in);
}

/* every buffer of this camera still in the input queue, given back */
static void input_drain(struct atl_camera *camera)
{
	GPtrArray *taken = g_ptr_array_new();

	g_mutex_lock(&input_lock);
	for (guint i = 0; input_pending && i < input_pending->len;) {
		struct ndk_input *in = g_ptr_array_index(input_pending, i);

		if (in->camera != camera) {
			i++;
			continue;
		}
		g_ptr_array_add(taken, in);
		g_ptr_array_remove_index_fast(input_pending, i);
	}
	g_mutex_unlock(&input_lock);

	for (guint i = 0; i < taken->len; i++)
		input_done(g_ptr_array_index(taken, i));
	g_ptr_array_free(taken, TRUE);
}

/* the HAL's result for one frame, with the frame number its started event
 * was given, found by the sensor timestamp the two share */
static void on_stream_completed(void *context, ACameraCaptureSession *session,
                                ACaptureRequest *request, const ACameraMetadata *result)
{
	struct atl_camera *camera = context;
	struct atl_camera_metadata *md = md_from_acamera(result, NULL);
	const struct atl_camera_metadata_entry *entry;
	int64_t timestamp = 0, frame_number;
	int request_id;
	bool reprocess;

	(void)session;
	if (!md)
		return;
	entry = atl_camera_metadata_find(md, ACAMERA_SENSOR_TIMESTAMP);
	if (entry && entry->type == ATL_CAMERA2_TYPE_INT64 && entry->count >= 1)
		timestamp = ((const int64_t *)entry->data)[0];

	reprocess = request_is_reprocess(request);
	g_mutex_lock(&camera->lock);
	if (reprocess || !started_find_locked(camera, timestamp, &frame_number, &request_id)) {
		frame_number = camera->s_next_frame++;
		request_id = request_id_of(camera, request);
	}
	/* the ring keeps the camera's own frames: a reprocess result would
	 * replace the original under the same timestamp, and an app may
	 * reprocess one frame more than once */
	if (camera->s_has_input && timestamp && !reprocess)
		result_keep_locked(camera, timestamp, result);
	g_mutex_unlock(&camera->lock);
	camera->s_callbacks.result(camera->s_user, request_id, frame_number, md);
}

static void on_stream_failed(void *context, ACameraCaptureSession *session,
                             ACaptureRequest *request, ACameraCaptureFailure *failure)
{
	struct atl_camera *camera = context;
	int request_id = request_id_of(camera, request);
	int64_t frame_number = failure ? failure->frameNumber : -1;

	(void)session;
	fprintf(stderr, "Camera camera2ndk: capture of frame %" G_GINT64_FORMAT " (request %d) "
	                "failed, reason %d\n", frame_number, request_id, failure ? failure->reason : -1);
	camera->s_callbacks.failed(camera->s_user, request_id, frame_number);
}

/* the HAL will not fill one stream's buffer of this frame; the app hears
 * which, as it does on Android, and does not wait for the image */
static void on_stream_buffer_lost(void *context, ACameraCaptureSession *session,
                                  ACaptureRequest *request, ANativeWindow *window,
                                  int64_t frame_number)
{
	struct atl_camera *camera = context;
	int request_id = request_id_of(camera, request);
	int index = -1;

	(void)session;
	for (int i = 0; i < camera->n_streams; i++)
		if (camera->streams[i]->window == window)
			index = i;
	if (index >= 0)
		camera->streams[index]->dropped++;
	/* the first, then every 300th: a stream the HAL gives up on every frame is
	 * the shape of the loss, and the close line comes far too late to see it */
	if (index >= 0 && (camera->streams[index]->dropped == 1 ||
	                   camera->streams[index]->dropped % 300 == 0))
		fprintf(stderr, "Camera camera2ndk: stream %d lost its buffer of frame %" G_GINT64_FORMAT
		                " (%" G_GUINT64_FORMAT " lost, %" G_GUINT64_FORMAT " delivered, %"
		                G_GUINT64_FORMAT " unacquired)\n", index, frame_number,
		        camera->streams[index]->dropped, camera->streams[index]->delivered,
		        camera->streams[index]->unacquired);
	if (camera->s_callbacks.buffer_lost)
		camera->s_callbacks.buffer_lost(camera->s_user, request_id, frame_number, index);
}

/* the session's listeners off and waited for, then everything freed that no
 * buffer out in the world still needs */
static void stream_session_destroy(struct atl_camera *camera)
{
	if (!camera->s_session && !camera->n_streams)
		return;

	g_mutex_lock(&camera->lock);
	camera->closing = true;
	while (camera->callbacks_running)
		g_cond_wait(&camera->idle, &camera->lock);
	g_mutex_unlock(&camera->lock);
	for (int i = 0; i < camera->n_streams; i++)
		if (camera->streams[i]->reader)
			ndk.AImageReader_setImageListener(camera->streams[i]->reader, NULL);

	if (camera->s_session) {
		ndk.ACameraCaptureSession_stopRepeating(camera->s_session);
		ndk.ACameraCaptureSession_close(camera->s_session);
		camera->s_session = NULL;
	}
	if (camera->s_outputs) {
		ndk.ACaptureSessionOutputContainer_free(camera->s_outputs);
		camera->s_outputs = NULL;
	}
	for (int i = 0; i < camera->n_streams; i++) {
		struct ndk_stream *stream = camera->streams[i];

		if (stream->delivered || stream->dropped || stream->unacquired)
			fprintf(stderr, "Camera camera2ndk: stream %d (%dx%d format 0x%x) delivered %"
			        G_GUINT64_FORMAT " buffers, the HAL lost %" G_GUINT64_FORMAT ", the reader "
			        "would not give %" G_GUINT64_FORMAT "\n", i,
			        stream->config.width, stream->config.height, stream->config.format,
			        stream->delivered, stream->dropped, stream->unacquired);
		camera->streams[i] = NULL;
		stream_unref(stream);
	}
	camera->n_streams = 0;
	camera->s_repeating_id = -1;
	camera->closing = false;
	if (camera->s_reprocessed)
		fprintf(stderr, "Camera camera2ndk: %" G_GUINT64_FORMAT " reprocess capture(s) on the "
		                "%dx%d %s input\n", camera->s_reprocessed, camera->s_input.width,
		        camera->s_input.height, stream_format_name(camera->s_input.format));
	camera->s_reprocessed = 0;
	g_mutex_lock(&camera->lock);
	camera->s_has_input = false;
	memset(&camera->s_input, 0, sizeof(camera->s_input));
	g_mutex_unlock(&camera->lock);
	/* the app's buffers first: a released callback may still be on its way in */
	input_drain(camera);
	results_clear(camera);
}

static const char *stream_format_name(int format)
{
	switch (format) {
	case ATL_CAMERA_FORMAT_RAW_SENSOR:  return "RAW_SENSOR";
	case ATL_CAMERA_FORMAT_PRIVATE:     return "PRIVATE";
	case ATL_CAMERA_FORMAT_YUV_420_888: return "YUV_420_888";
	case ATL_CAMERA_FORMAT_RAW10:       return "RAW10";
	case ATL_CAMERA_FORMAT_RAW12:       return "RAW12";
	case ATL_CAMERA_FORMAT_JPEG:        return "JPEG";
	default:                            return "format";
	}
}

static struct ndk_stream *stream_new(struct atl_camera *camera, int index,
                                     const struct atl_camera_stream *config)
{
	struct ndk_stream *stream = g_new0(struct ndk_stream, 1);
	/* android.graphics.ImageFormat and AIMAGE_FORMAT agree on every value
	 * an ImageReader can be made with; RAW_SENSOR is the NDK's RAW16 */
	int format = config->format;
	uint64_t usage = 0;
	AImageReader_ImageListener listener = {stream, on_stream_image};
	media_status_t status;
	camera_status_t cstatus;

	stream->refcount = 1;
	stream->camera = camera;
	stream->index = index;
	stream->config = *config;
	stream->physical_id = config->physical_id ? g_strdup(config->physical_id) : NULL;
	stream->config.physical_id = stream->physical_id;

	if (config->usage & ATL_CAMERA_USAGE_CPU_READ_OFTEN)
		usage |= USAGE_CPU_READ_OFTEN;
	if (config->usage & ATL_CAMERA_USAGE_GPU_SAMPLED_IMAGE)
		usage |= USAGE_GPU_SAMPLED_IMAGE;
	/* an opaque stream is sampled by the GPU, whatever else was asked */
	if (format == ATL_CAMERA_FORMAT_PRIVATE)
		usage |= USAGE_GPU_SAMPLED_IMAGE;

	/* one more than the consumer may hold, so a reader full of images is what
	 * drops the next one and counts it, not the HAL's queue silently */
	if (usage)
		status = ndk.AImageReader_newWithUsage(config->width, config->height, format, usage,
		                                       config->max_buffers + 1, &stream->reader);
	else
		status = ndk.AImageReader_new(config->width, config->height, format,
		                              config->max_buffers + 1, &stream->reader);
	if (status != AMEDIA_OK || !stream->reader) {
		fprintf(stderr, "Camera camera2ndk: no %dx%d image reader for %s (0x%x), usage 0x%llx "
		                "(%d)\n", config->width, config->height, stream_format_name(format),
		        format, (unsigned long long)usage, status);
		goto fail;
	}
	if (ndk.AImageReader_getWindow(stream->reader, &stream->window) != AMEDIA_OK ||
	    !stream->window) {
		fprintf(stderr, "Camera camera2ndk: the %dx%d %s reader has no window\n",
		        config->width, config->height, stream_format_name(format));
		goto fail;
	}
	ndk.AImageReader_setImageListener(stream->reader, &listener);

	if (stream->physical_id) {
		if (!ndk.ACaptureSessionPhysicalOutput_create) {
			fprintf(stderr, "Camera camera2ndk: this libcamera2ndk has no physical camera "
			                "outputs, stream %d cannot be camera %s's\n", index, stream->physical_id);
			goto fail;
		}
		cstatus = ndk.ACaptureSessionPhysicalOutput_create(stream->window, stream->physical_id,
		                                                   &stream->output);
	} else {
		cstatus = ndk.ACaptureSessionOutput_create(stream->window, &stream->output);
	}
	if (cstatus != ACAMERA_OK || !stream->output ||
	    ndk.ACameraOutputTarget_create(stream->window, &stream->target) != ACAMERA_OK) {
		fprintf(stderr, "Camera camera2ndk: could not make a session output of the %dx%d %s "
		                "stream\n", config->width, config->height, stream_format_name(format));
		goto fail;
	}
	return stream;

fail:
	stream_unref(stream);
	return NULL;
}

static bool camera2ndk_configure_streams(struct atl_camera *camera,
                                         const struct atl_camera_stream *configs, int n_streams,
                                         const struct atl_camera_stream_input *input,
                                         const struct atl_camera_stream_callbacks *callbacks,
                                         void *user)
{
	ACameraInputConfiguration in_config;
	ACameraCaptureSession_stateCallbacks state = {
		.context = camera,
		.onClosed = on_session_closed,
		.onReady = on_session_ready,
		.onActive = on_session_active,
	};
	camera_status_t status;

	/* whichever session ran before: the Camera1 one, or the last of these */
	session_destroy(camera);
	stream_session_destroy(camera);
	if (n_streams <= 0)
		return true;
	if (n_streams > ATL_CAMERA_MAX_STREAMS || !callbacks)
		return false;
	if (input && !reprocess_available()) {
		fprintf(stderr, "Camera camera2ndk: no reprocessing in this libcamera2ndk, the %dx%d "
		                "%s input cannot be configured\n", input->width, input->height,
		        stream_format_name(input->format));
		return false;
	}

	camera->s_callbacks = *callbacks;
	camera->s_user = user;
	camera->s_v2 = ndk.ACameraCaptureSession_setRepeatingRequestV2 &&
	               ndk.ACameraCaptureSession_captureV2;
	camera->s_cb = (ACameraCaptureSession_captureCallbacks){
		.context = camera,
		.onCaptureStarted = on_stream_started,
		.onCaptureCompleted = on_stream_completed,
		.onCaptureFailed = on_stream_failed,
		.onCaptureBufferLost = on_stream_buffer_lost,
	};
	camera->s_cb_v2 = (ACameraCaptureSession_captureCallbacksV2){
		.context = camera,
		.onCaptureStarted = on_stream_started_v2,
		.onCaptureCompleted = on_stream_completed,
		.onCaptureFailed = on_stream_failed,
		.onCaptureBufferLost = on_stream_buffer_lost,
	};

	if (ndk.ACaptureSessionOutputContainer_create(&camera->s_outputs) != ACAMERA_OK)
		return false;
	for (int i = 0; i < n_streams; i++) {
		struct ndk_stream *stream = stream_new(camera, i, &configs[i]);

		if (!stream)
			goto fail;
		camera->streams[i] = stream;
		camera->n_streams = i + 1;
		ndk.ACaptureSessionOutputContainer_add(camera->s_outputs, stream->output);
	}

	if (input) {
		/* AOSP builds the input stream inside the same configuration as the
		 * outputs, which is why it cannot be added to a session after the
		 * fact; a HAL that will not take it refuses the whole configuration */
		in_config = (ACameraInputConfiguration){
			.width = input->width,
			.height = input->height,
			.format = input->format,
			.isMultiResolution = input->multi_resolution,
			.maxImages = input->max_buffers > 0 ? input->max_buffers : MAX_INPUT_QUEUE,
		};
		status = ndk.ACameraDevice_createReprocessableCaptureSession(camera->device, &in_config,
		                                                             camera->s_outputs, NULL,
		                                                             &state, &camera->s_session);
	} else {
		status = ndk.ACameraDevice_createCaptureSession(camera->device, camera->s_outputs, &state,
		                                                &camera->s_session);
	}
	if (status != ACAMERA_OK || !camera->s_session) {
		fprintf(stderr, "Camera camera2ndk: createCaptureSession refused the %d stream(s)%s (%d)\n",
		        n_streams, input ? " and the input" : "", status);
		goto fail;
	}
	if (input) {
		g_mutex_lock(&camera->lock);
		camera->s_has_input = true;
		camera->s_input = *input;
		g_mutex_unlock(&camera->lock);
	}

	fprintf(stderr, "Camera camera2ndk: session with %d HAL stream(s)%s:\n", n_streams,
	        camera->s_v2 ? "" : " (no frame numbers from this libcamera2ndk)");
	if (input)
		fprintf(stderr, "Camera camera2ndk:   input: %dx%d %s%s\n", input->width, input->height,
		        stream_format_name(input->format),
		        input->multi_resolution ? ", multi-resolution" : "");
	for (int i = 0; i < n_streams; i++)
		fprintf(stderr, "Camera camera2ndk:   stream %d: %dx%d %s x%d%s%s\n", i,
		        configs[i].width, configs[i].height, stream_format_name(configs[i].format),
		        configs[i].max_buffers, configs[i].physical_id ? ", physical camera " : "",
		        configs[i].physical_id ? configs[i].physical_id : "");
	return true;

fail:
	stream_session_destroy(camera);
	return false;
}

/*
 * The metering regions and the crop a request carries, once per distinct value.
 * caiman's HAL runs its AE off them and rejects a degenerate rectangle
 * ("Invalid input crop [x0=0, y0=0, x1=0, y1=0]" out of libgcam), so what is in
 * the request - the driver template's, or the app's on top - is the question.
 */
static void request_report_regions(const ACaptureRequest *request, const char *when)
{
	static const struct { uint32_t tag; const char *name; } wanted[] = {
		{ACAMERA_CONTROL_AE_REGIONS, "aeRegions"},
		{ACAMERA_CONTROL_AF_REGIONS, "afRegions"},
		{ACAMERA_CONTROL_AWB_REGIONS, "awbRegions"},
		{ACAMERA_SCALER_CROP_REGION, "cropRegion"},
	};
	static char last[G_N_ELEMENTS(wanted)][2][160];

	if (!getenv("ATL_CAMERA_DUMP_METADATA") || !ndk.ACaptureRequest_getConstEntry)
		return;
	for (guint i = 0; i < G_N_ELEMENTS(wanted); i++) {
		ACameraMetadata_const_entry entry = {0};
		char text[160];
		int at = 0, slot = g_strcmp0(when, "template") ? 1 : 0;

		if (ndk.ACaptureRequest_getConstEntry(request, wanted[i].tag, &entry) != ACAMERA_OK)
			g_strlcpy(text, "(absent)", sizeof(text));
		else
			for (uint32_t j = 0; j < entry.count && at < (int)sizeof(text) - 12; j++)
				at += snprintf(text + at, sizeof(text) - at, "%s%d", j ? " " : "",
				               entry.data.i32[j]);
		if (!g_strcmp0(text, last[i][slot]))
			continue;
		g_strlcpy(last[i][slot], text, sizeof(last[i][slot]));
		fprintf(stderr, "Camera camera2ndk: %s %s = %s\n", when, wanted[i].name, text);
	}
}

static bool camera2ndk_submit_request(struct atl_camera *camera, int request_id,
                                      const struct atl_camera_metadata *settings,
                                      uint32_t streams, bool repeating)
{
	const struct atl_camera_metadata_entry *intent;
	ACameraDevice_request_template template = TEMPLATE_PREVIEW;
	ACaptureRequest *request = NULL;
	int n_targets = 0, sequence = 0;
	camera_status_t status;

	if (!camera->s_session)
		return false;

	/* the app's capture intent names the template it built the request from;
	 * the two enumerations agree */
	intent = atl_camera_metadata_find(settings, ACAMERA_CONTROL_CAPTURE_INTENT);
	if (intent && intent->type == ATL_CAMERA2_TYPE_BYTE && intent->count >= 1) {
		uint8_t value = ((const uint8_t *)intent->data)[0];

		if (value >= TEMPLATE_PREVIEW && value <= TEMPLATE_MANUAL)
			template = (ACameraDevice_request_template)value;
	}
	status = ndk.ACameraDevice_createCaptureRequest(camera->device, template, &request);
	if (status != ACAMERA_OK || !request) {
		fprintf(stderr, "Camera camera2ndk: createCaptureRequest(template %d) failed (%d)\n",
		        template, status);
		return false;
	}
	for (int i = 0; i < camera->n_streams; i++) {
		if (!(streams & (1u << i)))
			continue;
		ndk.ACaptureRequest_addTarget(request, camera->streams[i]->target);
		n_targets++;
	}
	if (!n_targets) {
		fprintf(stderr, "Camera camera2ndk: request %d targets no stream\n", request_id);
		ndk.ACaptureRequest_free(request);
		return false;
	}
	request_apply_settings(request, settings);
	if (ndk.ACaptureRequest_setUserContext)
		ndk.ACaptureRequest_setUserContext(request, (void *)(intptr_t)request_id);
	/* ATL_CAMERA_DUMP_METADATA: the regions the driver's own template carries,
	 * before the app's settings land on top of them */
	request_report_regions(request, "template");
	/* ATL_CAMERA_DUMP_METADATA: what the app actually asked for, per request */
	{
		char *label = g_strdup_printf("%s request %d (template %d, streams 0x%x)",
		                              repeating ? "repeating" : "one-shot", request_id,
		                              template, streams);

		atl_camera_metadata_dump(settings, label);
		g_free(label);
	}
	request_report_regions(request, "submitted");

	if (repeating) {
		camera->s_repeating_id = request_id;
		status = camera->s_v2
		             ? ndk.ACameraCaptureSession_setRepeatingRequestV2(camera->s_session,
		                                                               &camera->s_cb_v2, 1,
		                                                               &request, &sequence)
		             : ndk.ACameraCaptureSession_setRepeatingRequest(camera->s_session,
		                                                             &camera->s_cb, 1, &request,
		                                                             &sequence);
	} else {
		status = camera->s_v2
		             ? ndk.ACameraCaptureSession_captureV2(camera->s_session, &camera->s_cb_v2, 1,
		                                                   &request, &sequence)
		             : ndk.ACameraCaptureSession_capture(camera->s_session, &camera->s_cb, 1,
		                                                 &request, &sequence);
	}
	/* the session keeps its own copy */
	ndk.ACaptureRequest_free(request);
	if (status != ACAMERA_OK) {
		fprintf(stderr, "Camera camera2ndk: %s request %d (template %d, %d target(s)) failed (%d)\n",
		        repeating ? "repeating" : "one-shot", request_id, template, n_targets, status);
		return false;
	}
	if (repeating || getenv("ATL_DEBUG_IMAGEREADER"))
		fprintf(stderr, "Camera camera2ndk: %s request %d: template %d, %d target(s), streams 0x%x, "
		                "sequence %d\n", repeating ? "repeating" : "one-shot", request_id, template,
		        n_targets, streams, sequence);
	return true;
}

/*
 * The app hands the camera back one of its own buffers, which is what
 * ImageWriter.queueInputImage does on Android: the gralloc buffer is attached
 * to the input queue and queued with the frame's sensor timestamp. Nothing is
 * copied, so the buffer stays the app's until the camera releases it.
 */
static bool camera2ndk_queue_input(struct atl_camera *camera, struct atl_camera_buffer *buffer,
                                   atl_camera_input_released_cb released, void *user)
{
	struct ndk_input *in;
	camera_status_t status;
	uint64_t token;
	int queued = 0;

	if (!camera->s_session || !camera->s_has_input || !buffer)
		return false;
	if (!buffer->native) {
		fprintf(stderr, "Camera camera2ndk: an input buffer with no gralloc handle cannot be "
		                "reprocessed (stream %d, format 0x%x)\n", buffer->stream, buffer->format);
		return false;
	}

	in = g_new0(struct ndk_input, 1);
	in->camera = camera;
	in->buffer = buffer;
	in->released = released;
	in->user = user;

	g_mutex_lock(&input_lock);
	if (!input_pending)
		input_pending = g_ptr_array_new();
	/* this camera's own budget: another open camera's queued buffers are not
	 * this input stream's depth */
	for (guint i = 0; i < input_pending->len; i++)
		if (((struct ndk_input *)g_ptr_array_index(input_pending, i))->camera == camera)
			queued++;
	if (queued >= MAX_INPUT_QUEUE) {
		g_mutex_unlock(&input_lock);
		fprintf(stderr, "Camera camera2ndk: the reprocessing input already holds %d buffer(s)\n",
		        queued);
		g_free(in);
		return false;
	}
	token = in->token = input_next_token++;
	g_ptr_array_add(input_pending, in);
	g_mutex_unlock(&input_lock);

	status = ndk.ACameraCaptureSession_queueInputBuffer(camera->s_session, buffer->native,
	                                                    buffer->timestamp, on_input_released,
	                                                    (void *)(uintptr_t)token);
	if (status != ACAMERA_OK) {
		fprintf(stderr, "Camera camera2ndk: queueInputBuffer(%dx%d, %" G_GINT64_FORMAT ") failed "
		                "(%d)\n", buffer->width, buffer->height, buffer->timestamp, status);
		/* the callback will not come; take the entry back if it has not */
		in = input_claim(token);
		g_free(in);
		return false;
	}
	return true;
}

/*
 * A reprocess capture: its settings are the whole result of the frame being
 * reprocessed, which is why the ring is kept. The app's own bag goes on top -
 * that is where the JPEG quality and orientation it wants live.
 */
static bool camera2ndk_submit_reprocess(struct atl_camera *camera, int request_id,
                                        const struct atl_camera_metadata *settings,
                                        uint32_t streams, int64_t input_timestamp)
{
	int n_targets = 0, sequence = 0;
	const ACameraMetadata *result;
	ACameraMetadata *taken = NULL;
	ACaptureRequest *request = NULL;
	camera_status_t status;

	if (!camera->s_session || !camera->s_has_input)
		return false;

	/* the ring's copy is copied again and the lock dropped before the NDK is
	 * called: createReprocessCaptureRequest takes the device's own lock, and
	 * the callback threads take that one before this one */
	g_mutex_lock(&camera->lock);
	result = result_find_locked(camera, input_timestamp);
	if (result)
		taken = ndk.ACameraMetadata_copy(result);
	g_mutex_unlock(&camera->lock);
	if (!taken) {
		fprintf(stderr, "Camera camera2ndk: no kept result for the frame at %" G_GINT64_FORMAT
		                ", the last %d cannot be reprocessed\n", input_timestamp, RESULT_RING);
		return false;
	}

	status = ndk.ACameraDevice_createReprocessCaptureRequest(camera->device, taken, &request);
	ndk.ACameraMetadata_free(taken);
	if (status != ACAMERA_OK || !request) {
		fprintf(stderr, "Camera camera2ndk: createReprocessCaptureRequest failed (%d)\n", status);
		return false;
	}

	for (int i = 0; i < camera->n_streams; i++) {
		if (!(streams & (1u << i)))
			continue;
		ndk.ACaptureRequest_addTarget(request, camera->streams[i]->target);
		n_targets++;
	}
	if (!n_targets) {
		fprintf(stderr, "Camera camera2ndk: reprocess request %d targets no stream\n", request_id);
		ndk.ACaptureRequest_free(request);
		return false;
	}
	request_apply_settings(request, settings);
	if (ndk.ACaptureRequest_setUserContext)
		ndk.ACaptureRequest_setUserContext(request, (void *)(intptr_t)request_id);

	/* never repeating: a reprocess consumes one queued input buffer */
	status = camera->s_v2
	             ? ndk.ACameraCaptureSession_captureV2(camera->s_session, &camera->s_cb_v2, 1,
	                                                   &request, &sequence)
	             : ndk.ACameraCaptureSession_capture(camera->s_session, &camera->s_cb, 1, &request,
	                                                 &sequence);
	ndk.ACaptureRequest_free(request);
	if (status != ACAMERA_OK) {
		fprintf(stderr, "Camera camera2ndk: reprocess request %d (%d target(s)) failed (%d)\n",
		        request_id, n_targets, status);
		return false;
	}
	camera->s_reprocessed++;
	if (camera->s_reprocessed == 1 || getenv("ATL_DEBUG_IMAGEREADER"))
		fprintf(stderr, "Camera camera2ndk: reprocess request %d: %d target(s), streams 0x%x, "
		                "input frame %" G_GINT64_FORMAT ", sequence %d\n", request_id, n_targets,
		        streams, input_timestamp, sequence);
	return true;
}

static void camera2ndk_cancel_repeating(struct atl_camera *camera)
{
	if (!camera->s_session)
		return;
	ndk.ACameraCaptureSession_stopRepeating(camera->s_session);
	camera->s_repeating_id = -1;
}

static void camera2ndk_flush_requests(struct atl_camera *camera)
{
	if (!camera->s_session || !ndk.ACameraCaptureSession_abortCaptures)
		return;
	ndk.ACameraCaptureSession_abortCaptures(camera->s_session);
}

/* --- backend vtable ------------------------------------------------------ */

/* the ids the HAL enumerates, cached; call with ndk_lock held */
static bool camera_ids_load(void)
{
	ACameraManager *mgr = manager_get();
	ACameraIdList *list = NULL;

	if (camera_ids)
		return true;
	if (!mgr)
		return false;
	if (ndk.ACameraManager_getCameraIdList(mgr, &list) != ACAMERA_OK || !list) {
		fprintf(stderr, "Camera camera2ndk: getCameraIdList failed\n");
		return false;
	}

	camera_ids = g_new0(char *, list->numCameras + 1);
	for (int i = 0; i < list->numCameras; i++)
		camera_ids[i] = g_strdup(list->cameraIds[i]);
	n_camera_ids = list->numCameras;
	ndk.ACameraManager_deleteCameraIdList(list);

	fprintf(stderr, "Camera camera2ndk: %d camera(s)\n", n_camera_ids);
	return true;
}

static int camera2ndk_get_count(void)
{
	int count = 0;

	g_mutex_lock(&ndk_lock);
	if (camera_ids_load())
		count = n_camera_ids;
	g_mutex_unlock(&ndk_lock);
	return count;
}

/* the id at a Camera1-style index; NULL when there is none */
static const char *camera_id_at(int index)
{
	if (!camera_ids_load() || index < 0 || index >= n_camera_ids)
		return NULL;
	return camera_ids[index];
}

static bool camera2ndk_get_info(int index, int *facing, int *orientation)
{
	const struct camera_static *statics;
	const uint8_t *lens_facing;
	const int32_t *sensor_orientation;
	const char *id;

	g_mutex_lock(&ndk_lock);
	id = camera_id_at(index);
	statics = id ? camera_static_get(id) : NULL;
	if (!statics) {
		g_mutex_unlock(&ndk_lock);
		return false;
	}

	lens_facing = md_u8(statics->md, ACAMERA_LENS_FACING, NULL);
	sensor_orientation = md_i32(statics->md, ACAMERA_SENSOR_ORIENTATION, NULL);
	*facing = lens_facing && lens_facing[0] == ACAMERA_LENS_FACING_FRONT ? ATL_CAMERA_FACING_FRONT
	                                                                    : ATL_CAMERA_FACING_BACK;
	*orientation = sensor_orientation ? sensor_orientation[0] : 0;
	g_mutex_unlock(&ndk_lock);
	return true;
}

static struct atl_camera *camera2ndk_open(int index)
{
	ACameraDevice_StateCallbacks callbacks = {0};
	const struct camera_static *statics;
	struct atl_camera *camera;
	ACameraManager *mgr;
	const char *id;
	camera_status_t status;

	g_mutex_lock(&ndk_lock);
	mgr = manager_get();
	id = mgr ? camera_id_at(index) : NULL;
	statics = id ? camera_static_get(id) : NULL;
	g_mutex_unlock(&ndk_lock);
	if (!statics) {
		fprintf(stderr, "Camera camera2ndk: no camera at index %d\n", index);
		return NULL;
	}

	camera = g_new0(struct atl_camera, 1);
	camera->id = g_strdup(id);
	camera->statics = statics;
	g_cond_init(&camera->idle);
	/* the CPU frames stay on until a caller says its texture is the only
	 * consumer left (Camera1's preview callbacks never stop needing them) */
	camera->pixels_needed = true;
	g_mutex_init(&camera->lock);
	camera->dump_dir = g_strdup(getenv("ATL_CAMERA_DUMP_FRAMES"));
	camera->s_repeating_id = -1;
	query_caps(camera);
	camera->width = camera->caps.preview_sizes[0].width;
	camera->height = camera->caps.preview_sizes[0].height;

	callbacks.context = camera;
	callbacks.onDisconnected = on_device_disconnected;
	callbacks.onError = on_device_error;

	status = ndk.ACameraManager_openCamera(mgr, id, &callbacks, &camera->device);
	if (status != ACAMERA_OK || !camera->device) {
		fprintf(stderr, "Camera camera2ndk: openCamera('%s') failed (%d)\n", id, status);
		g_free(camera->id);
		g_free(camera);
		return NULL;
	}

	fprintf(stderr, "Camera camera2ndk: opened camera '%s' (%d preview / %d picture sizes)\n",
	        id, camera->caps.n_preview_sizes, camera->caps.n_picture_sizes);
	return camera;
}

static void camera2ndk_close(struct atl_camera *camera)
{
	if (!camera)
		return;

	session_destroy(camera);
	stream_session_destroy(camera);
	if (camera->device)
		ndk.ACameraDevice_close(camera->device);
	if (camera->s_buffers)
		fprintf(stderr, "Camera camera2ndk: closed after %" G_GUINT64_FORMAT " stream buffers\n",
		        camera->s_buffers);
	if (camera->frame_count)
		fprintf(stderr, "Camera camera2ndk: closed after %" G_GUINT64_FORMAT " frames, %"
		        G_GUINT64_FORMAT " of them copied to the CPU\n", camera->frame_count,
		        camera->cpu_frame_count);

	atl_camera_metadata_free(camera->result);
	atl_camera_metadata_free(camera->request);
	g_array_free(camera->preview_sizes, TRUE);
	g_array_free(camera->picture_sizes, TRUE);
	g_array_free(camera->fps_ranges, TRUE);
	g_string_free(camera->focus_modes, TRUE);
	g_string_free(camera->flash_modes, TRUE);
	g_string_free(camera->scene_modes, TRUE);
	g_string_free(camera->white_balance_modes, TRUE);
	g_string_free(camera->color_effects, TRUE);
	g_string_free(camera->antibanding_modes, TRUE);
	g_mutex_clear(&camera->lock);
	g_cond_clear(&camera->idle);
	g_free(camera->nv21);
	g_free(camera->dump_dir);
	g_free(camera->id);
	g_free(camera);
}

static const struct atl_camera_caps *camera2ndk_get_caps(struct atl_camera *camera)
{
	return &camera->caps;
}

/* the supported size closest to the requested one: an app that picked from our
 * stream configurations hits it exactly, a Camera1 app may not */
static bool camera2ndk_set_preview_size(struct atl_camera *camera, int width, int height)
{
	const struct atl_camera_size *best = NULL;
	long best_distance = 0;

	if (width < 2 || height < 2)
		return false;

	for (int i = 0; i < camera->caps.n_preview_sizes; i++) {
		const struct atl_camera_size *size = &camera->caps.preview_sizes[i];
		long distance = labs((long)size->width * size->height - (long)width * height);

		if (size->width == width && size->height == height) {
			best = size;
			best_distance = 0;
			break;
		}
		if (!best || distance < best_distance) {
			best = size;
			best_distance = distance;
		}
	}
	if (!best)
		return false;
	if (best_distance)
		fprintf(stderr, "Camera camera2ndk: %dx%d is not a stream size, using %dx%d\n",
		        width, height, best->width, best->height);

	camera->width = best->width;
	camera->height = best->height;
	return true;
}

static bool camera2ndk_set_preview_format(struct atl_camera *camera, int format)
{
	(void)camera;
	/* the YUV stream is repacked into NV21, which is all ATL asks for */
	return format == ATL_CAMERA_FORMAT_NV21;
}

static bool camera2ndk_set_fps_range(struct atl_camera *camera, int min, int max)
{
	camera->fps_min = min;
	camera->fps_max = max;
	request_apply_fps(camera);
	if (camera->previewing)
		repeating_start(camera);
	return true;
}

static void camera2ndk_set_frame_callback(struct atl_camera *camera, atl_camera_frame_cb cb, void *user)
{
	g_mutex_lock(&camera->lock);
	camera->frame_cb = cb;
	camera->frame_user = user;
	g_mutex_unlock(&camera->lock);
}

static void camera2ndk_set_error_callback(struct atl_camera *camera, atl_camera_error_cb cb, void *user)
{
	g_mutex_lock(&camera->lock);
	camera->error_cb = cb;
	camera->error_user = user;
	g_mutex_unlock(&camera->lock);
}

static bool camera2ndk_start_preview(struct atl_camera *camera)
{
	if (camera->previewing)
		return true;
	if (!camera->session || camera->session_width != camera->width ||
	    camera->session_height != camera->height) {
		if (!session_create(camera, camera->session_still_width, camera->session_still_height))
			return false;
	}
	if (!repeating_start(camera))
		return false;
	fprintf(stderr, "Camera camera2ndk: preview started %dx%d\n", camera->width, camera->height);
	return true;
}

static void camera2ndk_stop_preview(struct atl_camera *camera)
{
	if (!camera->session || !camera->previewing)
		return;
	ndk.ACameraCaptureSession_stopRepeating(camera->session);
	camera->previewing = false;
}

/*
 * One still through the HAL's own JPEG encoder. The session's outputs are fixed,
 * so a picture size the current session has no stream for means rebuilding it -
 * and the preview has to run while the still is taken, because AE and AF settle
 * on the repeating request.
 */
static bool camera2ndk_take_picture(struct atl_camera *camera, int width, int height,
                                    int jpeg_quality, atl_camera_jpeg_cb cb, void *user)
{
	uint8_t quality = (uint8_t)CLAMP(jpeg_quality, 1, 100);
	int32_t orientation = camera->display_orientation;
	bool was_previewing = camera->previewing;
	int sequence = 0;
	camera_status_t status;

	if (width < 2 || height < 2)
		return false;

	if (!camera->session || camera->session_still_width != width ||
	    camera->session_still_height != height) {
		if (!session_create(camera, width, height))
			return false;
		was_previewing = true; /* a fresh session has to be driven to get frames */
	}
	if (!camera->still_request)
		return false;

	ndk.ACaptureRequest_setEntry_u8(camera->still_request, ACAMERA_JPEG_QUALITY, 1, &quality);
	ndk.ACaptureRequest_setEntry_i32(camera->still_request, ACAMERA_JPEG_ORIENTATION, 1,
	                                 &orientation);

	g_mutex_lock(&camera->lock);
	camera->jpeg_cb = cb;
	camera->jpeg_user = user;
	g_mutex_unlock(&camera->lock);

	if (was_previewing && !camera->previewing && !repeating_start(camera))
		return false;

	status = ndk.ACameraCaptureSession_capture(camera->session, capture_callbacks(camera), 1,
	                                           &camera->still_request, &sequence);
	if (status != ACAMERA_OK) {
		fprintf(stderr, "Camera camera2ndk: still capture failed (%d)\n", status);
		g_mutex_lock(&camera->lock);
		camera->jpeg_cb = NULL;
		g_mutex_unlock(&camera->lock);
		return false;
	}
	return true;
}

static void camera2ndk_autofocus(struct atl_camera *camera, atl_camera_autofocus_cb cb, void *user)
{
	int sequence = 0;

	g_mutex_lock(&camera->lock);
	camera->autofocus_cb = cb;
	camera->autofocus_user = user;
	g_mutex_unlock(&camera->lock);

	if (!camera->session || !camera->preview_request) {
		g_mutex_lock(&camera->lock);
		camera->autofocus_cb = NULL;
		g_mutex_unlock(&camera->lock);
		if (cb)
			cb(false, user);
		return;
	}

	/* the trigger belongs to one request, so it is sent once and then cleared
	 * on the repeating one; the result's AF_STATE says how it ended */
	request_set_u8(camera->preview_request, ACAMERA_CONTROL_AF_TRIGGER,
	               ACAMERA_CONTROL_AF_TRIGGER_START);
	ndk.ACameraCaptureSession_capture(camera->session, capture_callbacks(camera), 1,
	                                  &camera->preview_request, &sequence);
	request_set_u8(camera->preview_request, ACAMERA_CONTROL_AF_TRIGGER,
	               ACAMERA_CONTROL_AF_TRIGGER_IDLE);
	if (camera->previewing)
		repeating_start(camera);
}

static void camera2ndk_cancel_autofocus(struct atl_camera *camera)
{
	int sequence = 0;

	g_mutex_lock(&camera->lock);
	camera->autofocus_cb = NULL;
	g_mutex_unlock(&camera->lock);

	if (!camera->session || !camera->preview_request)
		return;
	request_set_u8(camera->preview_request, ACAMERA_CONTROL_AF_TRIGGER,
	               ACAMERA_CONTROL_AF_TRIGGER_CANCEL);
	ndk.ACameraCaptureSession_capture(camera->session, capture_callbacks(camera), 1,
	                                  &camera->preview_request, &sequence);
	request_set_u8(camera->preview_request, ACAMERA_CONTROL_AF_TRIGGER,
	               ACAMERA_CONTROL_AF_TRIGGER_IDLE);
	if (camera->previewing)
		repeating_start(camera);
}

static void camera2ndk_set_display_orientation(struct atl_camera *camera, int degrees)
{
	/* only the JPEG the HAL encodes is affected; the preview frames are
	 * rotated by whoever draws them */
	camera->display_orientation = degrees;
}

/* Camera1 zoom index -> the zoom ratio the parameter flattener advertises */
static void camera2ndk_set_zoom(struct atl_camera *camera, int zoom)
{
	float ratio = 1.0f + 0.1f * (float)CLAMP(zoom, 0, camera->caps.max_zoom);

	if (!camera->preview_request)
		return;
	ndk.ACaptureRequest_setEntry_float(camera->preview_request, ACAMERA_CONTROL_ZOOM_RATIO, 1,
	                                   &ratio);
	if (camera->previewing)
		repeating_start(camera);
}

static void camera2ndk_set_focus_mode(struct atl_camera *camera, const char *mode)
{
	uint8_t af = ACAMERA_CONTROL_AF_MODE_AUTO;

	if (!mode || !camera->preview_request)
		return;
	if (!strcmp(mode, "fixed") || !strcmp(mode, "infinity"))
		af = ACAMERA_CONTROL_AF_MODE_OFF;
	else if (!strcmp(mode, "macro"))
		af = ACAMERA_CONTROL_AF_MODE_MACRO;
	else if (!strcmp(mode, "continuous-video"))
		af = ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
	else if (!strcmp(mode, "continuous-picture"))
		af = ACAMERA_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
	else if (!strcmp(mode, "edof"))
		af = ACAMERA_CONTROL_AF_MODE_EDOF;

	request_set_u8(camera->preview_request, ACAMERA_CONTROL_AF_MODE, af);
	if (camera->previewing)
		repeating_start(camera);
}

/*
 * Camera1 flash: "torch" is FLASH_MODE, the rest are AE modes, because the HAL
 * decides when to fire a flash it is told to use automatically.
 */
static void camera2ndk_set_flash_mode(struct atl_camera *camera, const char *mode)
{
	uint8_t ae = ACAMERA_CONTROL_AE_MODE_ON;
	uint8_t flash = ACAMERA_FLASH_MODE_OFF;

	if (!mode || !camera->preview_request)
		return;
	if (!strcmp(mode, "torch"))
		flash = ACAMERA_FLASH_MODE_TORCH;
	else if (!strcmp(mode, "on"))
		ae = ACAMERA_CONTROL_AE_MODE_ON_ALWAYS_FLASH;
	else if (!strcmp(mode, "auto"))
		ae = ACAMERA_CONTROL_AE_MODE_ON_AUTO_FLASH;
	else if (!strcmp(mode, "red-eye"))
		ae = ACAMERA_CONTROL_AE_MODE_ON_AUTO_FLASH_REDEYE;

	request_set_u8(camera->preview_request, ACAMERA_CONTROL_AE_MODE, ae);
	request_set_u8(camera->preview_request, ACAMERA_FLASH_MODE, flash);
	if (camera->previewing)
		repeating_start(camera);
}

/* --- camera2 ------------------------------------------------------------- */

static const char *const *camera2ndk_get_id_list(int *count)
{
	const char *const *ids = NULL;

	g_mutex_lock(&ndk_lock);
	if (camera_ids_load()) {
		ids = (const char *const *)camera_ids;
		*count = n_camera_ids;
	}
	g_mutex_unlock(&ndk_lock);
	return ids;
}

static struct atl_camera_metadata *camera2ndk_get_static_metadata(const char *id)
{
	const struct camera_static *statics;
	struct atl_camera_metadata *copy;

	g_mutex_lock(&ndk_lock);
	statics = camera_static_get(id);
	copy = statics ? atl_camera_metadata_copy(statics->md) : NULL;
	g_mutex_unlock(&ndk_lock);
	return copy;
}

static const uint32_t *camera2ndk_get_available_keys(const char *id, int which, int *count)
{
	const struct camera_static *statics;
	const uint32_t *keys = NULL;

	if (which < 0 || which > ATL_CAMERA2_KEYS_RESULT)
		return NULL;

	g_mutex_lock(&ndk_lock);
	statics = camera_static_get(id);
	if (statics) {
		keys = statics->keys[which];
		*count = statics->n_keys[which];
	}
	g_mutex_unlock(&ndk_lock);
	return keys;
}

/*
 * The app's request, applied as it is: this backend is a camera2 HAL, so every
 * key it knows is one the HAL may know too. ATL's camera2 runs one stream, so
 * the settings hold from here until the next request replaces them.
 */
static void camera2ndk_set_request_metadata(struct atl_camera *camera,
                                            const struct atl_camera_metadata *request)
{
	if (!atl_camera_metadata_n_entries(request))
		return;

	/* the first request of a session arrives before start_preview builds one,
	 * so keep it: session_create replays it onto the request it makes */
	atl_camera_metadata_free(camera->request);
	camera->request = atl_camera_metadata_copy(request);
	request_apply_all(camera);
	if (camera->previewing)
		repeating_start(camera);
}

static struct atl_camera_metadata *camera2ndk_get_result_metadata(struct atl_camera *camera,
                                                                  const struct atl_camera_metadata *request)
{
	struct atl_camera_metadata *copy;

	/* the HAL echoes the request in its own result, so there is nothing to
	 * merge in here */
	(void)request;
	g_mutex_lock(&camera->lock);
	copy = atl_camera_metadata_copy(camera->result);
	g_mutex_unlock(&camera->lock);
	return copy;
}

static const struct atl_camera_backend camera2ndk_backend = {
	.name = "camera2ndk",
	.get_camera_count = camera2ndk_get_count,
	.get_camera_info = camera2ndk_get_info,
	.open = camera2ndk_open,
	.close = camera2ndk_close,
	.get_caps = camera2ndk_get_caps,
	.set_preview_size = camera2ndk_set_preview_size,
	.set_preview_format = camera2ndk_set_preview_format,
	.set_fps_range = camera2ndk_set_fps_range,
	.set_frame_callback = camera2ndk_set_frame_callback,
	.set_error_callback = camera2ndk_set_error_callback,
	.start_preview = camera2ndk_start_preview,
	.stop_preview = camera2ndk_stop_preview,
	.take_picture = camera2ndk_take_picture,
	.autofocus = camera2ndk_autofocus,
	.cancel_autofocus = camera2ndk_cancel_autofocus,
	.set_display_orientation = camera2ndk_set_display_orientation,
	.set_zoom = camera2ndk_set_zoom,
	.set_focus_mode = camera2ndk_set_focus_mode,
	.set_flash_mode = camera2ndk_set_flash_mode,
	.update_preview_texture = camera2ndk_update_preview_texture,
	.set_texture_callback = camera2ndk_set_texture_callback,
	.set_frame_pixels_needed = camera2ndk_set_frame_pixels_needed,
	.set_frame_size_needed = camera2ndk_set_frame_size_needed,
	.get_camera2_id_list = camera2ndk_get_id_list,
	.get_static_metadata = camera2ndk_get_static_metadata,
	.get_available_keys = camera2ndk_get_available_keys,
	.set_request_metadata = camera2ndk_set_request_metadata,
	.get_result_metadata = camera2ndk_get_result_metadata,
	.configure_streams = camera2ndk_configure_streams,
	.submit_request = camera2ndk_submit_request,
	.cancel_repeating = camera2ndk_cancel_repeating,
	.flush_requests = camera2ndk_flush_requests,
	.queue_input = camera2ndk_queue_input,
	.submit_reprocess = camera2ndk_submit_reprocess,
};

const struct atl_camera_backend *atl_camera_backend_camera2ndk_get(void)
{
	bool ok;

	g_mutex_lock(&ndk_lock);
	ok = ndk_load() && camera_ids_load();
	g_mutex_unlock(&ndk_lock);
	return ok ? &camera2ndk_backend : NULL;
}
