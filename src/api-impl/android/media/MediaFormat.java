package android.media;

import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.Map;

public class MediaFormat {

	public static final String KEY_MIME = "mime";
	public static final String KEY_WIDTH = "width";
	public static final String KEY_HEIGHT = "height";
	public static final String KEY_BIT_RATE = "bitrate";
	public static final String KEY_FRAME_RATE = "frame-rate";
	public static final String KEY_I_FRAME_INTERVAL = "i-frame-interval";
	public static final String KEY_COLOR_FORMAT = "color-format";
	public static final String KEY_MAX_INPUT_SIZE = "max-input-size";
	public static final String KEY_DURATION = "durationUs";
	public static final String KEY_CHANNEL_COUNT = "channel-count";
	public static final String KEY_SAMPLE_RATE = "sample-rate";

	public static final String MIMETYPE_VIDEO_AVC = "video/avc";
	public static final String MIMETYPE_VIDEO_HEVC = "video/hevc";
	public static final String MIMETYPE_AUDIO_AAC = "audio/mp4a-latm";

	private Map<String, Object> map = new HashMap<>();

	public MediaFormat() {}

	public MediaFormat(MediaFormat other) {
		map.putAll(other.map);
	}

	public static MediaFormat createVideoFormat(String mime, int width, int height) {
		MediaFormat format = new MediaFormat();

		format.setString(KEY_MIME, mime);
		format.setInteger(KEY_WIDTH, width);
		format.setInteger(KEY_HEIGHT, height);
		return format;
	}

	public static MediaFormat createAudioFormat(String mime, int sampleRate, int channelCount) {
		MediaFormat format = new MediaFormat();

		format.setString(KEY_MIME, mime);
		format.setInteger(KEY_SAMPLE_RATE, sampleRate);
		format.setInteger(KEY_CHANNEL_COUNT, channelCount);
		return format;
	}

	public void setString(String key, String value) {
		map.put(key, value);
	}

	public void setInteger(String key, int value) {
		map.put(key, value);
	}

	public void setByteBuffer(String key, ByteBuffer value) {
		map.put(key, value);
	}

	public void setFloat(String key, float value) {
		map.put(key, value);
	}

	public ByteBuffer getByteBuffer(String name) {
		return (ByteBuffer)map.get(name);
	}

	public int getInteger(String name) {
		return (int)map.get(name);
	}

	public int getInteger(String name, int defaultValue) {
		Object value = map.get(name);

		return value instanceof Integer ? (Integer)value : defaultValue;
	}

	public float getFloat(String name) {
		return (float)map.get(name);
	}

	public boolean containsKey(String name) {
		return map.containsKey(name);
	}

	public String toString() {
		return map.toString();
	}

	public String getString(String name) {
		return (String)map.get(name);
	}

	public long getLong(String name) {
		return (long)map.get(name);
	}


	public static final int VIDEO_ENCODING_STATISTICS_LEVEL_1 = 1;

	public static final java.lang.String KEY_BITRATE_MODE = "bitrate-mode";




	public static final java.lang.String KEY_COLOR_RANGE = "color-range";

	public static final java.lang.String KEY_COLOR_STANDARD = "color-standard";

	public static final java.lang.String KEY_CROP_BOTTOM = "crop-bottom";

	public static final java.lang.String KEY_CROP_LEFT = "crop-left";

	public static final java.lang.String KEY_CROP_RIGHT = "crop-right";

	public static final java.lang.String KEY_CROP_TOP = "crop-top";




	public static final java.lang.String KEY_MAX_HEIGHT = "max-height";

	public static final java.lang.String KEY_MAX_WIDTH = "max-width";



	public static final java.lang.String KEY_SLICE_HEIGHT = "slice-height";

	public static final java.lang.String KEY_STRIDE = "stride";

	public static final java.lang.String KEY_VIDEO_ENCODING_STATISTICS_LEVEL = "video-encoding-statistics-level";

	public static final java.lang.String KEY_VIDEO_QP_AVERAGE = "video-qp-average";

}
