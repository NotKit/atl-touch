/*
 * One finished frame into an android.view.Surface, for src/libandroid.
 *
 * A frame an app's native code posted through an ANativeWindow - with
 * ANativeWindow_unlockAndPost, or as a buffer in a surface transaction - takes
 * the same road a decoded video frame does: a Bitmap, then
 * Surface.postFrame(), then SurfaceView.postFrame() and an invalidate. A
 * Surface with nothing behind it drops the frame, which is what AOSP's does
 * once its consumer is gone.
 *
 * It lives here rather than in libandroid.so because building a Bitmap needs
 * Skia; libandroid.so is loaded for an app's own native code and does not link
 * against the main library, so it resolves this by name.
 *
 * The Java classes are looked up here rather than taken from handle_cache so
 * this works in the headless tests too (they load the library into a plain
 * dalvikvm, which never runs set_up_handle_cache()).
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <jni.h>

#include "defines.h"

extern void *atl_video_frame_wrap_skbitmap(uint8_t *pixels, int width, int height);
extern bool atl_camera_write_png(const char *path, const uint8_t *rgba, int width, int height);

/*
 * ATL_DEBUG_PRESENT=<dir>: every 30th frame an ANativeWindow presents, as a
 * PNG, plus a running count. This is the last place ATL has the frame as
 * pixels, so it separates "the app's buffer was converted right" from "the
 * view drew it" - and on a device there is no other way to see either.
 * Debug only: it costs a PNG encode.
 */
#define PNG_EVERY   150 /* five seconds of a 30 fps viewfinder */
#define RATE_EVERY  120
#define COUNT_EVERY 30

static void dump_frame(const uint8_t *rgba, int width, int height)
{
	static const char *dir;
	static bool tried;
	static uint64_t count;
	static int64_t started, marked, last, worst;
	char path[512];
	FILE *f;

	if (!tried) {
		tried = true;
		dir = getenv("ATL_DEBUG_PRESENT");
	}
	if (!dir)
		return;

	count++;

	/*
	 * How steadily frames are actually arriving, which is the question a
	 * stuttering viewfinder asks and no PNG answers. The worst gap is the
	 * one that matters: an even rate half of what the camera produces is a
	 * throughput problem, while the same average with a gap of hundreds of
	 * milliseconds in it is something stalling.
	 */
	int64_t now = g_get_monotonic_time();

	if (!started)
		started = now;
	if (last && now - last > worst)
		worst = now - last;
	last = now;

	if (count % RATE_EVERY == 0) {
		double since = (now - (marked ? marked : started)) / 1e6;

		if (since > 0)
			fprintf(stderr, "Surface post: %llu frames presented, last %d in %.1fs "
			                "(%.1f/s, worst gap %.0f ms)\n",
			        (unsigned long long)count, RATE_EVERY, since, RATE_EVERY / since,
			        worst / 1000.0);
		marked = now;
		worst = 0;
	}

	/* the counter is cheap enough to keep current, but not per frame: this
	 * runs on the thread the app presents from, and a file write per frame
	 * is a hitch per frame */
	if (count % COUNT_EVERY == 0) {
		snprintf(path, sizeof(path), "%s/present-count", dir);
		f = fopen(path, "w");
		if (f) {
			fprintf(f, "%llu\n", (unsigned long long)count);
			fclose(f);
		}
	}

	/* the picture is a PNG encode, so it is rarer by a lot - dumping one
	 * every 30 frames is its own stutter */
	if (count % PNG_EVERY != 1)
		return;

	snprintf(path, sizeof(path), "%s/present-%06llu.png", dir, (unsigned long long)count);
	if (!atl_camera_write_png(path, rgba, width, height))
		fprintf(stderr, "Surface post: failed to write %s\n", path);
}

static jclass bitmap_class;
static jmethodID bitmap_from_native;
static jclass surface_class;
static jmethodID surface_post_frame;

static bool ensure_java_refs(JNIEnv *env)
{
	jclass bitmap, surface;

	if (bitmap_class && surface_class)
		return true;

	bitmap = (*env)->FindClass(env, "android/graphics/Bitmap");
	surface = (*env)->FindClass(env, "android/view/Surface");
	if (!bitmap || !surface) {
		fprintf(stderr, "Surface post: android/graphics/Bitmap or android/view/Surface not found\n");
		(*env)->ExceptionClear(env);
		return false;
	}
	bitmap_from_native = (*env)->GetStaticMethodID(env, bitmap, "fromNative", "(J)Landroid/graphics/Bitmap;");
	surface_post_frame = (*env)->GetMethodID(env, surface, "postFrame", "(Landroid/graphics/Bitmap;)V");
	if (!bitmap_from_native || !surface_post_frame) {
		fprintf(stderr, "Surface post: Bitmap.fromNative or Surface.postFrame not found\n");
		(*env)->ExceptionClear(env);
		return false;
	}
	bitmap_class = (*env)->NewGlobalRef(env, bitmap);
	surface_class = (*env)->NewGlobalRef(env, surface);
	return true;
}

/*
 * Takes the pixels: they become an SkBitmap's, and are freed with it.
 * Callable from any thread the caller has attached. 0 on success.
 */
int atl_surface_post_rgba(JNIEnv *env, jobject surface, uint8_t *rgba, int width, int height)
{
	jobject bitmap;
	void *skbitmap;

	if (!env || !surface || !rgba || width <= 0 || height <= 0) {
		free(rgba);
		return -1;
	}
	if (!ensure_java_refs(env)) {
		free(rgba);
		return -1;
	}

	dump_frame(rgba, width, height);

	skbitmap = atl_video_frame_wrap_skbitmap(rgba, width, height); /* takes the pixels */
	if (!skbitmap)
		return -1;

	bitmap = (*env)->CallStaticObjectMethod(env, bitmap_class, bitmap_from_native, _INTPTR(skbitmap));
	if (!bitmap || (*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		return -1;
	}
	(*env)->CallVoidMethod(env, surface, surface_post_frame, bitmap);
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		(*env)->DeleteLocalRef(env, bitmap);
		return -1;
	}
	(*env)->DeleteLocalRef(env, bitmap);
	return 0;
}
