/*
 * android.media.SoundPool over GStreamer.
 *
 * Loading just records the sample's URI; every play() spins up a transient
 * playbin that tears itself down on EOS/error, so overlapping plays of the
 * same sample work naturally. This trades a little start latency for
 * simplicity — a pre-decoded low-latency path can come later if a game
 * needs it.
 */

#include <stdio.h>

#include <gst/gst.h>

#include "../defines.h"
#include "../generated_headers/android_media_SoundPool.h"

#include "../media/atl_gst.h"

struct sound_pool {
	GPtrArray *uris; /* owned strings; soundID is index+1 (0 = load failure, per AOSP) */
};

struct pool_stream {
	GstElement *playbin;
	guint bus_watch;
	int loops_remaining; /* -1 = forever */
};

JNIEXPORT jlong JNICALL Java_android_media_SoundPool_native_1constructor(JNIEnv *env, jclass class)
{
	struct sound_pool *pool = g_new0(struct sound_pool, 1);
	pool->uris = g_ptr_array_new_with_free_func(g_free);
	return _INTPTR(pool);
}

JNIEXPORT jint JNICALL Java_android_media_SoundPool_nativeLoad(JNIEnv *env, jclass class, jlong pool_ptr, jstring path_jstr)
{
	struct sound_pool *pool = _PTR(pool_ptr);
	const char *path = (*env)->GetStringUTFChars(env, path_jstr, NULL);
	char *uri = gst_filename_to_uri(path, NULL);
	(*env)->ReleaseStringUTFChars(env, path_jstr, path);

	if (!uri || !atl_gst_ensure_init()) {
		g_free(uri);
		return 0;
	}
	g_ptr_array_add(pool->uris, uri);
	return pool->uris->len; /* 1-based */
}

static void stream_free(struct pool_stream *stream)
{
	gst_element_set_state(stream->playbin, GST_STATE_NULL);
	gst_object_unref(stream->playbin);
	g_free(stream);
}

static gboolean stream_bus_callback(GstBus *bus, GstMessage *message, gpointer data)
{
	struct pool_stream *stream = data;

	switch (GST_MESSAGE_TYPE(message)) {
		case GST_MESSAGE_EOS:
			if (stream->loops_remaining != 0) {
				if (stream->loops_remaining > 0)
					stream->loops_remaining--;
				gst_element_seek_simple(stream->playbin, GST_FORMAT_TIME,
				                        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0);
				return G_SOURCE_CONTINUE;
			}
			stream_free(stream);
			return G_SOURCE_REMOVE;
		case GST_MESSAGE_ERROR: {
			GError *error = NULL;
			gst_message_parse_error(message, &error, NULL);
			fprintf(stderr, "SoundPool: GStreamer error: %s\n", error ? error->message : "?");
			g_clear_error(&error);
			stream_free(stream);
			return G_SOURCE_REMOVE;
		}
		default:
			return G_SOURCE_CONTINUE;
	}
}

JNIEXPORT jint JNICALL Java_android_media_SoundPool_nativePlay(JNIEnv *env, jclass class, jlong pool_ptr, jint soundID, jfloat volume, jint loop)
{
	struct sound_pool *pool = _PTR(pool_ptr);

	if (soundID < 1 || (guint)soundID > pool->uris->len)
		return 0;

	GstElement *playbin = gst_element_factory_make("playbin", NULL);
	if (!playbin)
		return 0;

	struct pool_stream *stream = g_new0(struct pool_stream, 1);
	stream->playbin = playbin;
	stream->loops_remaining = loop;

	g_object_set(playbin,
	             "uri", g_ptr_array_index(pool->uris, soundID - 1),
	             "volume", (gdouble)volume,
	             NULL);
	GstElement *audio_sink = atl_gst_make_audio_sink();
	if (audio_sink)
		g_object_set(playbin, "audio-sink", audio_sink, NULL);

	GstBus *gst_bus = gst_element_get_bus(playbin);
	stream->bus_watch = gst_bus_add_watch(gst_bus, stream_bus_callback, stream);
	gst_object_unref(gst_bus);

	gst_element_set_state(playbin, GST_STATE_PLAYING);
	return soundID;
}
