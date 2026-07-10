#ifndef ATL_GST_H
#define ATL_GST_H

#include <gst/gst.h>
#include <stdbool.h>

/* Lazily initialize GStreamer (safe to call repeatedly, from any thread).
 * Returns false if gst_init_check failed — media playback unavailable. */
bool atl_gst_ensure_init(void);

/* The audio sink for playback elements: NULL for playbin's default
 * (autoaudiosink → PulseAudio/PipeWire on real systems), or an element built
 * from the ATL_MEDIA_AUDIO_SINK env description (e.g. "fakesink sync=true"
 * for headless test machines with no audio server). Caller owns the ref. */
GstElement *atl_gst_make_audio_sink(void);

#endif
