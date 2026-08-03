/*
 * android.graphics.SurfaceTexture, see surface_texture.h.
 *
 * There is no BufferQueue and no EGLImage here: the producer converts its NV21
 * frame to RGBA into a single-frame mailbox (newest frame wins, like the other
 * camera paths), and the consumer either uploads that with glTexImage2D into a
 * GL_TEXTURE_2D or copies it into a Bitmap for the Skia scene. An app sampling
 * the texture as samplerExternalOES therefore gets a plain 2D texture.
 *
 * Classes are looked up here rather than taken from handle_cache so this also
 * works without the launcher having populated the cache.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <GLES2/gl2.h>

#include "../defines.h"

#include "camera_frame.h"
#include "surface_texture.h"

#include "../generated_headers/android_graphics_SurfaceTexture.h"

extern void *atl_video_frame_wrap_skbitmap(uint8_t *pixels, int width, int height);
extern void atl_video_frame_skbitmap_pixels_changed(void *skbitmap);

/* bitmaps handed to the scene are reused round-robin, so the frame rate does
 * not allocate native pixel memory the GC never reclaims */
#define ST_BITMAPS 3

struct st_bitmap {
	jobject bitmap; /* global ref to the android.graphics.Bitmap */
	uint8_t *pixels;
	void *skbitmap;
	int width;
	int height;
};

struct atl_surface_texture {
	gint refcount;
	JavaVM *jvm;
	jweak self; /* weak: the app owns the SurfaceTexture's lifetime */

	GMutex lock;
	bool released;
	bool single_buffer;
	bool listener_enabled;
	bool idle_queued;

	/* the frame waiting to be taken, RGBA (converted by the producer) */
	uint8_t *pending;
	size_t pending_capacity;
	bool pending_valid;
	int pending_width;
	int pending_height;
	int64_t pending_timestamp;

	/* the frame consumers see, swapped in from pending */
	uint8_t *current;
	size_t current_capacity;
	bool current_valid;
	int width;
	int height;
	int64_t timestamp;
	uint64_t serial; /* bumped every time a frame is taken */

	unsigned tex_name;
	bool attached;
	uint64_t uploaded_serial;
	int uploaded_width;
	int uploaded_height;

	int default_width;
	int default_height;

	/* set when the producer fills the texture itself (hybris fast path) */
	struct atl_surface_texture_source source;

	struct st_bitmap bitmaps[ST_BITMAPS];
	int next_bitmap;
	int last_bitmap;
	uint64_t bitmap_serial;

	uint64_t submitted;
	uint64_t dropped;
};

static jclass bitmap_class;
static jmethodID bitmap_from_native;
static jmethodID post_frame_available;

static bool ensure_java_refs(JNIEnv *env)
{
	if (bitmap_class && post_frame_available)
		return true;

	jclass bitmap = (*env)->FindClass(env, "android/graphics/Bitmap");
	jclass texture = (*env)->FindClass(env, "android/graphics/SurfaceTexture");
	if (bitmap && texture) {
		bitmap_from_native = (*env)->GetStaticMethodID(env, bitmap, "fromNative", "(J)Landroid/graphics/Bitmap;");
		post_frame_available = (*env)->GetMethodID(env, texture, "postFrameAvailableFromNative", "()V");
	}
	if (!bitmap || !texture || !bitmap_from_native || !post_frame_available) {
		fprintf(stderr, "SurfaceTexture: Bitmap.fromNative or postFrameAvailableFromNative not found\n");
		(*env)->ExceptionClear(env);
		return false;
	}
	bitmap_class = (*env)->NewGlobalRef(env, bitmap);
	return true;
}

static void texture_unref(struct atl_surface_texture *texture)
{
	if (!g_atomic_int_dec_and_test(&texture->refcount))
		return;

	free(texture->pending);
	free(texture->current);
	g_mutex_clear(&texture->lock);
	free(texture);
}

void atl_surface_texture_ref(struct atl_surface_texture *texture)
{
	if (texture)
		g_atomic_int_inc(&texture->refcount);
}

void atl_surface_texture_unref(struct atl_surface_texture *texture)
{
	if (texture)
		texture_unref(texture);
}

struct atl_surface_texture *atl_surface_texture_from_java(JNIEnv *env, jobject surface_texture)
{
	struct atl_surface_texture *texture;

	if (!surface_texture)
		return NULL;
	texture = _PTR(_GET_LONG_FIELD(surface_texture, "nativePtr"));
	if (!texture)
		return NULL;
	g_atomic_int_inc(&texture->refcount);
	return texture;
}

static JNIEnv *get_env(struct atl_surface_texture *texture)
{
	JNIEnv *env;

	if ((*texture->jvm)->GetEnv(texture->jvm, (void **)&env, JNI_VERSION_1_6) == JNI_OK)
		return env;

	/* the main loop runs on the Java main thread in the launcher, but it can
	 * be a thread of its own */
	JavaVMAttachArgs args = {JNI_VERSION_1_6, "atl-surface-texture", NULL};
	if ((*texture->jvm)->AttachCurrentThreadAsDaemon(texture->jvm, (void **)&env, &args) != JNI_OK) {
		fprintf(stderr, "SurfaceTexture: failed to attach the delivery thread\n");
		return NULL;
	}
	return env;
}

/* onFrameAvailable is app code, so it runs on the main loop (the same
 * reasoning as the Camera.PreviewCallback dispatcher) */
static gboolean deliver_frame_available(gpointer user)
{
	struct atl_surface_texture *texture = user;
	JNIEnv *env = get_env(texture);

	if (!env)
		return G_SOURCE_REMOVE;

	/* the ref is taken under the lock: release() drops the weak ref */
	g_mutex_lock(&texture->lock);
	texture->idle_queued = false;
	jobject self = NULL;
	if (!texture->released && texture->listener_enabled)
		self = (*env)->NewLocalRef(env, texture->self);
	g_mutex_unlock(&texture->lock);

	if (self) {
		(*env)->CallVoidMethod(env, self, post_frame_available);
		if ((*env)->ExceptionCheck(env)) {
			(*env)->ExceptionDescribe(env);
			(*env)->ExceptionClear(env);
		}
		(*env)->DeleteLocalRef(env, self); /* nothing frees local refs outside a JNI call */
	}
	return G_SOURCE_REMOVE;
}

/* queue onFrameAvailable for the main loop; call with the lock held */
static void post_frame_available_locked(struct atl_surface_texture *texture)
{
	if (!texture->listener_enabled || texture->idle_queued || texture->released)
		return;
	texture->idle_queued = true;
	g_atomic_int_inc(&texture->refcount);
	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, deliver_frame_available, texture,
	                (GDestroyNotify)texture_unref);
}

void atl_surface_texture_set_source(struct atl_surface_texture *texture,
                                    const struct atl_surface_texture_source *source)
{
	if (!texture)
		return;
	g_mutex_lock(&texture->lock);
	if (source)
		texture->source = *source;
	else
		memset(&texture->source, 0, sizeof(texture->source));
	g_mutex_unlock(&texture->lock);
}

void atl_surface_texture_notify_frame_available(struct atl_surface_texture *texture)
{
	if (!texture)
		return;
	g_mutex_lock(&texture->lock);
	texture->submitted++;
	/* the pixels are the producer's business, but getTimestamp() still has to
	 * move: webrtc timestamps its frames with it */
	texture->timestamp = g_get_monotonic_time() * 1000;
	post_frame_available_locked(texture);
	g_mutex_unlock(&texture->lock);
}

void atl_surface_texture_submit(struct atl_surface_texture *texture, const uint8_t *nv21,
                                int width, int height, int stride)
{
	size_t size = (size_t)width * height * 4;

	g_mutex_lock(&texture->lock);
	if (texture->released)
		goto out;

	if (texture->pending_capacity < size) {
		free(texture->pending);
		texture->pending = malloc(size);
		texture->pending_capacity = texture->pending ? size : 0;
	}
	if (!texture->pending)
		goto out;

	atl_camera_nv21_to_rgba(nv21, width, height, stride, texture->pending);
	if (texture->pending_valid) /* the consumer is behind: keep the newest frame only */
		texture->dropped++;
	texture->pending_valid = true;
	texture->pending_width = width;
	texture->pending_height = height;
	texture->pending_timestamp = g_get_monotonic_time() * 1000; /* ns, like AOSP */
	texture->submitted++;

	post_frame_available_locked(texture);
out:
	g_mutex_unlock(&texture->lock);
}

/* move the pending frame to the consumer side; call with the lock held */
static void take_frame_locked(struct atl_surface_texture *texture)
{
	if (!texture->pending_valid)
		return;

	uint8_t *rgba = texture->current;
	size_t capacity = texture->current_capacity;

	texture->current = texture->pending;
	texture->current_capacity = texture->pending_capacity;
	texture->pending = rgba;
	texture->pending_capacity = capacity;
	texture->pending_valid = false;
	texture->current_valid = true;
	texture->width = texture->pending_width;
	texture->height = texture->pending_height;
	texture->timestamp = texture->pending_timestamp;
	texture->serial++;
}

static void bitmap_release(struct st_bitmap *buffer, JNIEnv *env)
{
	if (buffer->bitmap)
		(*env)->DeleteGlobalRef(env, buffer->bitmap); /* the Bitmap owns the SkBitmap */
	memset(buffer, 0, sizeof(*buffer));
}

static bool bitmap_prepare(struct st_bitmap *buffer, JNIEnv *env, int width, int height)
{
	if (buffer->bitmap && buffer->width == width && buffer->height == height)
		return true;

	bitmap_release(buffer, env);

	uint8_t *pixels = malloc((size_t)width * height * 4);
	if (!pixels)
		return false;
	void *skbitmap = atl_video_frame_wrap_skbitmap(pixels, width, height); /* takes the pixels */
	if (!skbitmap)
		return false;

	jobject bitmap = (*env)->CallStaticObjectMethod(env, bitmap_class, bitmap_from_native, _INTPTR(skbitmap));
	if (!bitmap || (*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		return false;
	}
	buffer->bitmap = (*env)->NewGlobalRef(env, bitmap);
	(*env)->DeleteLocalRef(env, bitmap);
	buffer->pixels = pixels;
	buffer->skbitmap = skbitmap;
	buffer->width = width;
	buffer->height = height;
	return true;
}

JNIEXPORT jlong JNICALL Java_android_graphics_SurfaceTexture_native_1create(JNIEnv *env, jobject this, jint tex_name,
                                                                            jboolean attached, jboolean single_buffer_mode)
{
	struct atl_surface_texture *texture;

	if (!ensure_java_refs(env))
		return 0;

	texture = calloc(1, sizeof(*texture));
	texture->refcount = 1;
	(*env)->GetJavaVM(env, &texture->jvm);
	g_mutex_init(&texture->lock);
	texture->self = (*env)->NewWeakGlobalRef(env, this);
	texture->tex_name = tex_name;
	texture->attached = attached;
	texture->single_buffer = single_buffer_mode;
	return _INTPTR(texture);
}

JNIEXPORT void JNICALL Java_android_graphics_SurfaceTexture_native_1release(JNIEnv *env, jobject this, jlong texture_ptr)
{
	struct atl_surface_texture *texture = _PTR(texture_ptr);

	if (!texture)
		return;

	g_mutex_lock(&texture->lock);
	texture->released = true;
	texture->listener_enabled = false;
	texture->pending_valid = false;
	texture->current_valid = false;
	memset(&texture->source, 0, sizeof(texture->source));
	for (int i = 0; i < ST_BITMAPS; i++)
		bitmap_release(&texture->bitmaps[i], env);
	if (texture->self)
		(*env)->DeleteWeakGlobalRef(env, texture->self);
	texture->self = NULL;
	if (texture->submitted || texture->dropped)
		fprintf(stderr, "SurfaceTexture: %" G_GUINT64_FORMAT " frames received, %"
		        G_GUINT64_FORMAT " dropped\n", texture->submitted, texture->dropped);
	g_mutex_unlock(&texture->lock);

	/* a producer or a queued idle call may still hold a reference */
	texture_unref(texture);
}

JNIEXPORT void JNICALL Java_android_graphics_SurfaceTexture_native_1setFrameAvailableEnabled(JNIEnv *env, jobject this,
                                                                                             jlong texture_ptr, jboolean enabled)
{
	struct atl_surface_texture *texture = _PTR(texture_ptr);

	if (!texture)
		return;
	g_mutex_lock(&texture->lock);
	texture->listener_enabled = enabled;
	g_mutex_unlock(&texture->lock);
}

JNIEXPORT void JNICALL Java_android_graphics_SurfaceTexture_native_1setDefaultBufferSize(JNIEnv *env, jobject this,
                                                                                          jlong texture_ptr, jint width, jint height)
{
	struct atl_surface_texture *texture = _PTR(texture_ptr);

	if (!texture)
		return;
	/* only advisory: the producer decides the frame size, and the camera gets
	 * its size from Camera.Parameters */
	g_mutex_lock(&texture->lock);
	texture->default_width = width;
	texture->default_height = height;
	g_mutex_unlock(&texture->lock);
}

JNIEXPORT void JNICALL Java_android_graphics_SurfaceTexture_native_1updateTexImage(JNIEnv *env, jobject this, jlong texture_ptr)
{
	struct atl_surface_texture *texture = _PTR(texture_ptr);

	if (!texture)
		return;

	g_mutex_lock(&texture->lock);
	if (!texture->attached) {
		g_mutex_unlock(&texture->lock);
		jclass exception = (*env)->FindClass(env, "java/lang/IllegalStateException");
		if (exception)
			(*env)->ThrowNew(env, exception, "SurfaceTexture is not attached to a GL context");
		return;
	}

	/* the producer owns the texture: let it pull in its own newest frame */
	if (texture->source.update) {
		if (texture->source.update(texture->source.user, texture->tex_name)) {
			g_mutex_unlock(&texture->lock);
			return;
		}
		fprintf(stderr, "SurfaceTexture: the producer's texture fast path failed, "
		                "falling back to the CPU upload\n");
		memset(&texture->source, 0, sizeof(texture->source));
	}

	take_frame_locked(texture);
	if (!texture->current_valid || texture->uploaded_serial == texture->serial) {
		g_mutex_unlock(&texture->lock);
		return; /* nothing new: AOSP keeps showing the last frame too */
	}

	glBindTexture(GL_TEXTURE_2D, texture->tex_name);
	if (texture->uploaded_width != texture->width || texture->uploaded_height != texture->height ||
	    texture->uploaded_serial == 0) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->width, texture->height, 0,
		             GL_RGBA, GL_UNSIGNED_BYTE, texture->current);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		texture->uploaded_width = texture->width;
		texture->uploaded_height = texture->height;
	} else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height,
		                GL_RGBA, GL_UNSIGNED_BYTE, texture->current);
	}
	texture->uploaded_serial = texture->serial;

	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
		fprintf(stderr, "SurfaceTexture: GL error 0x%x uploading a %dx%d frame (no current context?)\n",
		        error, texture->width, texture->height);
	g_mutex_unlock(&texture->lock);
}

/* tex_name 0 detaches */
JNIEXPORT void JNICALL Java_android_graphics_SurfaceTexture_native_1attachToGLContext(JNIEnv *env, jobject this,
                                                                                      jlong texture_ptr, jint tex_name)
{
	struct atl_surface_texture *texture = _PTR(texture_ptr);

	if (!texture)
		return;
	g_mutex_lock(&texture->lock);
	texture->tex_name = tex_name;
	texture->attached = tex_name != 0;
	/* the new texture object is empty, so the next update must re-upload */
	texture->uploaded_serial = 0;
	texture->uploaded_width = 0;
	texture->uploaded_height = 0;
	g_mutex_unlock(&texture->lock);
}

JNIEXPORT void JNICALL Java_android_graphics_SurfaceTexture_native_1getTransformMatrix(JNIEnv *env, jobject this,
                                                                                       jlong texture_ptr, jfloatArray matrix)
{
	struct atl_surface_texture *texture = _PTR(texture_ptr);
	/* column-major vertical flip: frames are uploaded top row first, which GL
	 * samples at t = 1 */
	static const jfloat vflip[16] = {
		1, 0, 0, 0,
		0, -1, 0, 0,
		0, 0, 1, 0,
		0, 1, 0, 1,
	};
	jfloat from_source[16];
	bool have_source = false;

	if (texture) {
		g_mutex_lock(&texture->lock);
		if (texture->source.get_transform)
			have_source = texture->source.get_transform(texture->source.user, from_source);
		g_mutex_unlock(&texture->lock);
	}

	(*env)->SetFloatArrayRegion(env, matrix, 0, 16, have_source ? from_source : vflip);
}

JNIEXPORT jlong JNICALL Java_android_graphics_SurfaceTexture_native_1getTimestamp(JNIEnv *env, jobject this, jlong texture_ptr)
{
	struct atl_surface_texture *texture = _PTR(texture_ptr);
	jlong timestamp;

	if (!texture)
		return 0;
	g_mutex_lock(&texture->lock);
	timestamp = texture->timestamp;
	g_mutex_unlock(&texture->lock);
	return timestamp;
}

/* the TextureView path: the newest frame as a Bitmap, reused across frames */
JNIEXPORT jobject JNICALL Java_android_graphics_SurfaceTexture_native_1getLatestFrameBitmap(JNIEnv *env, jobject this,
                                                                                            jlong texture_ptr)
{
	struct atl_surface_texture *texture = _PTR(texture_ptr);
	jobject bitmap = NULL;

	if (!texture)
		return NULL;

	g_mutex_lock(&texture->lock);
	take_frame_locked(texture);
	if (!texture->current_valid)
		goto out;

	if (texture->bitmap_serial != texture->serial || !texture->bitmaps[texture->last_bitmap].bitmap) {
		struct st_bitmap *buffer = &texture->bitmaps[texture->next_bitmap];

		if (!bitmap_prepare(buffer, env, texture->width, texture->height))
			goto out;
		memcpy(buffer->pixels, texture->current, (size_t)texture->width * texture->height * 4);
		atl_video_frame_skbitmap_pixels_changed(buffer->skbitmap);
		texture->last_bitmap = texture->next_bitmap;
		texture->next_bitmap = (texture->next_bitmap + 1) % ST_BITMAPS;
		texture->bitmap_serial = texture->serial;
	}
	bitmap = (*env)->NewLocalRef(env, texture->bitmaps[texture->last_bitmap].bitmap);
out:
	g_mutex_unlock(&texture->lock);
	return bitmap;
}
