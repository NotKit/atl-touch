package android.media;

import android.content.Context;

public class MediaRecorder {
	public class AudioEncoder {
		public static final int AAC = 3;
		public static final int OPUS = 7;
		public static final int VORBIS = 7;
	}

	public class AudioSource {
		public static final int DEFAULT = 0;
		public static final int MIC = 1;
	
	public static final int CAMCORDER = 5;

	public static final int UNPROCESSED = 9;

	public static final int VOICE_CALL = 4;

	public static final int VOICE_COMMUNICATION = 7;

	public static final int VOICE_DOWNLINK = 3;

	public static final int VOICE_PERFORMANCE = 10;

	public static final int VOICE_RECOGNITION = 6;

	public static final int VOICE_UPLINK = 2;
}

	public class MetricsConstants {}
	public class OutputFormat {
		public static final int AAC_ADTS = 6;
		public static final int DEFAULT = 0;
		public static final int THREE_GPP = 1;
		public static final int MPEG_4 = 2;
		public static final int WEBM = 9;
		public static final int OGG = 11;
	}
	public class VideoEncoder {
		public static final int DEFAULT = 0;
		public static final int H263 = 1;
		public static final int H264 = 2;
		public static final int MPEG_4_SP = 3;
		public static final int VP8 = 4;
		public static final int HEVC = 5;
	}
	public class VideoSource {
		public static final int DEFAULT = 0;
		public static final int CAMERA = 1;
		public static final int SURFACE = 2;
	}

	public interface OnErrorListener {}
	public interface OnInfoListener {}

	public MediaRecorder() {}
	public MediaRecorder(Context context) {}

	private int audioSource;
	private int outputFormat;
	private int audioEncoder;

	public void setAudioSource(int audioSource) {
		this.audioSource = audioSource;
	}

	public void setOutputFormat(int outputFormat) {
		this.outputFormat = outputFormat;
	}

	public void setAudioEncoder(int audioEncoder) {
		this.audioEncoder = audioEncoder;
	}

	public int getMaxAmplitude() {
		return 0;
	}

	public void setAudioEncodingBitRate(int audioEncodingBitrate) {}
	public void setAudioSamplingRate(int setAudioSamplingRate) {}
	public void setOutputFile(String filePath) {}
	public void prepare() {}
	public void start() {}
	public void stop() {}
	public void resume() {}
	public void pause() {}
	public void release() {}
}
