/*
 * android.media.MediaPlayer over GStreamer playbin.
 *
 * GStreamer is the playback engine on Ubuntu Touch (and reaches Android
 * hardware codecs on hybris ports through the droid elements), so playbin
 * gives us decoder auto-plugging, A/V clock sync, seeking and the audio sink
 * (PulseAudio/PipeWire) without hand-rolling a player over libav.
 *
 * The bus watch runs on the GLib main loop — the same thread as the Android
 * main Looper — so events are forwarded to Java (postEventFromNative) without
 * extra marshalling. Video is decoded but discarded into a fakesink for now;
 * rendering video frames into the Skia scene is part of the MediaCodec/
 * dmabuf task.
 */

#include <stdbool.h>
#include <stdio.h>

#include <gst/gst.h>

#include "../defines.h"
#include "../util.h"
#include "../generated_headers/android_media_MediaPlayer.h"

#include "atl_gst.h"

/* must match the WHAT_* constants in MediaPlayer.java */
#define EVENT_PREPARED      1
#define EVENT_COMPLETION    2
#define EVENT_SEEK_COMPLETE 3
#define EVENT_ERROR         100

#define MEDIA_ERROR_UNKNOWN 1

struct media_player {
	GstElement *playbin;
	guint bus_watch;
	jobject jplayer;      /* global ref to the java MediaPlayer */
	bool looping;
	bool prepared;
	bool prepare_async_pending; /* next ASYNC_DONE means "prepared", not "seek done" */
	bool seek_pending;
};

static void post_event(struct media_player *player, int what, int arg1, int arg2)
{
	JNIEnv *env = get_jni_env();
	(*env)->CallVoidMethod(env, player->jplayer,
	                       handle_cache.media_player.postEventFromNative, what, arg1, arg2);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
}

static gboolean bus_callback(GstBus *bus, GstMessage *message, gpointer data)
{
	struct media_player *player = data;

	switch (GST_MESSAGE_TYPE(message)) {
		case GST_MESSAGE_EOS:
			if (player->looping) {
				gst_element_seek_simple(player->playbin, GST_FORMAT_TIME,
				                        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0);
			} else {
				/* AOSP: playback complete leaves the player paused at the end */
				gst_element_set_state(player->playbin, GST_STATE_PAUSED);
				post_event(player, EVENT_COMPLETION, 0, 0);
			}
			break;
		case GST_MESSAGE_ERROR: {
			GError *error = NULL;
			char *debug = NULL;
			gst_message_parse_error(message, &error, &debug);
			fprintf(stderr, "MediaPlayer: GStreamer error: %s (%s)\n",
			        error ? error->message : "?", debug ? debug : "");
			g_clear_error(&error);
			g_free(debug);
			gst_element_set_state(player->playbin, GST_STATE_NULL);
			player->prepared = false;
			post_event(player, EVENT_ERROR, MEDIA_ERROR_UNKNOWN, 0);
			break;
		}
		case GST_MESSAGE_ASYNC_DONE:
			if (player->prepare_async_pending) {
				player->prepare_async_pending = false;
				player->prepared = true;
				post_event(player, EVENT_PREPARED, 0, 0);
			} else if (player->seek_pending) {
				player->seek_pending = false;
				post_event(player, EVENT_SEEK_COMPLETE, 0, 0);
			}
			break;
		default:
			break;
	}
	return G_SOURCE_CONTINUE;
}

JNIEXPORT jlong JNICALL Java_android_media_MediaPlayer_native_1create(JNIEnv *env, jobject this)
{
	if (!atl_gst_ensure_init())
		return 0;

	struct media_player *player = g_new0(struct media_player, 1);
	player->playbin = gst_element_factory_make("playbin", NULL);
	if (!player->playbin) {
		fprintf(stderr, "MediaPlayer: playbin unavailable (gst-plugins-base missing?)\n");
		g_free(player);
		return 0;
	}
	player->jplayer = _REF(this);

	GstElement *audio_sink = atl_gst_make_audio_sink();
	if (audio_sink)
		g_object_set(player->playbin, "audio-sink", audio_sink, NULL);
	/* no video rendering path yet — decode-and-drop keeps A/V files playing
	 * (and the clock advancing) instead of erroring out on a missing sink */
	GstElement *video_sink = gst_element_factory_make("fakesink", NULL);
	if (video_sink) {
		g_object_set(video_sink, "sync", TRUE, NULL);
		g_object_set(player->playbin, "video-sink", video_sink, NULL);
	}

	GstBus *bus = gst_element_get_bus(player->playbin);
	player->bus_watch = gst_bus_add_watch(bus, bus_callback, player);
	gst_object_unref(bus);

	return _INTPTR(player);
}

JNIEXPORT void JNICALL Java_android_media_MediaPlayer_native_1setDataSource(JNIEnv *env, jclass class, jlong player_ptr, jstring path_jstr)
{
	struct media_player *player = _PTR(player_ptr);
	const char *path = (*env)->GetStringUTFChars(env, path_jstr, NULL);

	char *uri = gst_uri_is_valid(path) ? g_strdup(path) : gst_filename_to_uri(path, NULL);
	g_object_set(player->playbin, "uri", uri, NULL);
	g_free(uri);

	(*env)->ReleaseStringUTFChars(env, path_jstr, path);
}

/* synchronous prepare: preroll to PAUSED */
JNIEXPORT jboolean JNICALL Java_android_media_MediaPlayer_native_1prepare(JNIEnv *env, jclass class, jlong player_ptr)
{
	struct media_player *player = _PTR(player_ptr);

	gst_element_set_state(player->playbin, GST_STATE_PAUSED);
	GstStateChangeReturn ret = gst_element_get_state(player->playbin, NULL, NULL, 10 * GST_SECOND);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		/* the bus watch reports the error to the app; clean up state here */
		gst_element_set_state(player->playbin, GST_STATE_NULL);
		return false;
	}
	player->prepared = true;
	return true;
}

JNIEXPORT void JNICALL Java_android_media_MediaPlayer_native_1prepareAsync(JNIEnv *env, jclass class, jlong player_ptr)
{
	struct media_player *player = _PTR(player_ptr);

	player->prepare_async_pending = true;
	if (gst_element_set_state(player->playbin, GST_STATE_PAUSED) == GST_STATE_CHANGE_SUCCESS) {
		/* completed synchronously; no ASYNC_DONE will follow */
		player->prepare_async_pending = false;
		player->prepared = true;
		post_event(player, EVENT_PREPARED, 0, 0);
	}
}

JNIEXPORT void JNICALL Java_android_media_MediaPlayer_native_1start(JNIEnv *env, jclass class, jlong player_ptr)
{
	struct media_player *player = _PTR(player_ptr);
	gst_element_set_state(player->playbin, GST_STATE_PLAYING);
}

JNIEXPORT void JNICALL Java_android_media_MediaPlayer_native_1pause(JNIEnv *env, jclass class, jlong player_ptr)
{
	struct media_player *player = _PTR(player_ptr);
	gst_element_set_state(player->playbin, GST_STATE_PAUSED);
}

JNIEXPORT void JNICALL Java_android_media_MediaPlayer_native_1stop(JNIEnv *env, jclass class, jlong player_ptr)
{
	struct media_player *player = _PTR(player_ptr);
	gst_element_set_state(player->playbin, GST_STATE_READY);
	player->prepared = false;
}

JNIEXPORT jboolean JNICALL Java_android_media_MediaPlayer_native_1isPlaying(JNIEnv *env, jclass class, jlong player_ptr)
{
	struct media_player *player = _PTR(player_ptr);
	GstState state = GST_STATE_NULL;
	GstState pending = GST_STATE_VOID_PENDING;
	gst_element_get_state(player->playbin, &state, &pending, 0);
	return (pending == GST_STATE_VOID_PENDING ? state : pending) == GST_STATE_PLAYING;
}

JNIEXPORT void JNICALL Java_android_media_MediaPlayer_native_1seekTo(JNIEnv *env, jclass class, jlong player_ptr, jint msec)
{
	struct media_player *player = _PTR(player_ptr);

	player->seek_pending = true;
	if (!gst_element_seek_simple(player->playbin, GST_FORMAT_TIME,
	                             GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
	                             (gint64)msec * GST_MSECOND)) {
		player->seek_pending = false;
		post_event(player, EVENT_SEEK_COMPLETE, 0, 0); /* seekTo contract: always answered */
	}
}

JNIEXPORT void JNICALL Java_android_media_MediaPlayer_native_1setLooping(JNIEnv *env, jclass class, jlong player_ptr, jboolean looping)
{
	struct media_player *player = _PTR(player_ptr);
	player->looping = looping;
}

JNIEXPORT void JNICALL Java_android_media_MediaPlayer_native_1setVolume(JNIEnv *env, jclass class, jlong player_ptr, jfloat volume)
{
	struct media_player *player = _PTR(player_ptr);
	g_object_set(player->playbin, "volume", (gdouble)volume, NULL);
}

JNIEXPORT jint JNICALL Java_android_media_MediaPlayer_native_1getDuration(JNIEnv *env, jclass class, jlong player_ptr)
{
	struct media_player *player = _PTR(player_ptr);
	gint64 duration = 0;
	if (!gst_element_query_duration(player->playbin, GST_FORMAT_TIME, &duration))
		return -1;
	return duration / GST_MSECOND;
}

JNIEXPORT jint JNICALL Java_android_media_MediaPlayer_native_1getCurrentPosition(JNIEnv *env, jclass class, jlong player_ptr)
{
	struct media_player *player = _PTR(player_ptr);
	gint64 position = 0;
	if (!gst_element_query_position(player->playbin, GST_FORMAT_TIME, &position))
		return 0;
	return position / GST_MSECOND;
}

JNIEXPORT void JNICALL Java_android_media_MediaPlayer_native_1reset(JNIEnv *env, jclass class, jlong player_ptr)
{
	struct media_player *player = _PTR(player_ptr);
	gst_element_set_state(player->playbin, GST_STATE_NULL);
	player->prepared = false;
	player->prepare_async_pending = false;
	player->seek_pending = false;
	player->looping = false;
}

JNIEXPORT void JNICALL Java_android_media_MediaPlayer_native_1release(JNIEnv *env, jclass class, jlong player_ptr)
{
	struct media_player *player = _PTR(player_ptr);

	gst_element_set_state(player->playbin, GST_STATE_NULL);
	g_source_remove(player->bus_watch);
	gst_object_unref(player->playbin);
	_UNREF(player->jplayer);
	g_free(player);
}
