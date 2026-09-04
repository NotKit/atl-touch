package android.media;

/**
 * Only the attributes themselves are honest: there is one ALSA device and no
 * routing policy here, so nothing in the audio path acts on a usage or a flag.
 */
public class AudioAttributes {

	int streamType = AudioManager.STREAM_MUSIC;
	int usage = USAGE_UNKNOWN;
	int contentType = CONTENT_TYPE_UNKNOWN;
	int flags;
	int capturePolicy = ALLOW_CAPTURE_BY_ALL;

	public int getFlags() { return flags; }

	public int getUsage() { return usage; }

	/* static: apps build attributes before they have any */
	public static class Builder {

		private final AudioAttributes attributes = new AudioAttributes();
		private boolean legacyStreamTypeSet;

		public Builder setContentType(int content_type) {
			attributes.contentType = content_type;
			return this;
		}

		public Builder setUsage(int usage) {
			attributes.usage = usage;
			return this;
		}

		public Builder setFlags(int flags) {
			attributes.flags = flags;
			return this;
		}

		public Builder setLegacyStreamType(int legacy_stream_type) {
			attributes.streamType = legacy_stream_type;
			legacyStreamTypeSet = true;
			return this;
		}

		/* No haptic channels here, so muting them is already true. */
		public Builder setHapticChannelsMuted(boolean muted) {
			return this;
		}

		public AudioAttributes build() {
			if (!legacyStreamTypeSet) {
				attributes.streamType = attributes.usage == USAGE_VOICE_COMMUNICATION
				    ? AudioManager.STREAM_VOICE_CALL : AudioManager.STREAM_MUSIC;
			}
			return attributes;
		}

		public Builder setAllowedCapturePolicy(int policy) {
			attributes.capturePolicy = policy;
			return this;
		}
	}

	public int getAllowedCapturePolicy() { return capturePolicy; }

	public int getContentType() { return contentType; }

	public static final int ALLOW_CAPTURE_BY_ALL = 1;

	public static final int ALLOW_CAPTURE_BY_SYSTEM = 2;

	public static final int ALLOW_CAPTURE_BY_NONE = 3;

	public static final int CONTENT_TYPE_SPEECH = 1;

	public static final int CONTENT_TYPE_UNKNOWN = 0;

	public static final int USAGE_MEDIA = 1;

	public static final int USAGE_UNKNOWN = 0;

	public static final int USAGE_VOICE_COMMUNICATION = 2;
}
