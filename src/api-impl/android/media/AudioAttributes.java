package android.media;

public class AudioAttributes {

	int streamType;

	public int getFlags() { return 0; }

	public int getUsage() { return 0; }

	public class Builder {

		public Builder setContentType(int content_type) {
			return this;
		}

		public Builder setUsage(int usage) {
			return this;
		}

		public Builder setFlags(int flags) {
			return this;
		}

		public Builder setLegacyStreamType(int legacy_stream_type) {
			return this;
		}

		public AudioAttributes build() {
			return new AudioAttributes();
		}
	
	public android.media.AudioAttributes.Builder setAllowedCapturePolicy(int a0) { return null; }
}

	public int getAllowedCapturePolicy() { return 0; }

	public int getContentType() { return 0; }

	public static final int CONTENT_TYPE_SPEECH = 1;

	public static final int CONTENT_TYPE_UNKNOWN = 0;

	public static final int USAGE_UNKNOWN = 0;

	public static final int USAGE_VOICE_COMMUNICATION = 2;
}
