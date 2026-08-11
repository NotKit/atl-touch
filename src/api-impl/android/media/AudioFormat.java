package android.media;

public class AudioFormat {

	int sampleRate = 44100;
	int channelMask;
	int encoding;

	public static class Builder {

		private AudioFormat audioFormat = new AudioFormat();

		public Builder setSampleRate(int sampleRate) {
			audioFormat.sampleRate = sampleRate;
			return this;
		}

		public Builder setChannelMask(int channelMask) {
			audioFormat.channelMask = channelMask;
			return this;
		}

		public Builder setEncoding(int encoding) {
			audioFormat.encoding = encoding;
			return this;
		}

		public AudioFormat build() {
			return audioFormat;
		}
	}

	public int getChannelCount() { return 0; }

	public int getChannelIndexMask() { return 0; }

	public int getChannelMask() { return 0; }

	public int getEncoding() { return 0; }

	public int getSampleRate() { return 0; }

	public static final int CHANNEL_INVALID = 0;

	public static final int CHANNEL_IN_MONO = 16;

	public static final int CHANNEL_IN_STEREO = 12;

	public static final int CHANNEL_OUT_MONO = 4;

	public static final int CHANNEL_OUT_STEREO = 12;

	public static final int ENCODING_AC3 = 5;

	public static final int ENCODING_DEFAULT = 1;

	public static final int ENCODING_DTS = 7;

	public static final int ENCODING_DTS_HD = 8;

	public static final int ENCODING_E_AC3 = 6;

	public static final int ENCODING_IEC61937 = 13;

	public static final int ENCODING_INVALID = 0;

	public static final int ENCODING_MP3 = 9;

	public static final int ENCODING_PCM_16BIT = 2;

	public static final int ENCODING_PCM_8BIT = 3;

	public static final int ENCODING_PCM_FLOAT = 4;
}
