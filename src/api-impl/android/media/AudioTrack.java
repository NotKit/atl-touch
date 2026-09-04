package android.media;

import java.nio.ByteBuffer;

public class AudioTrack {
	public interface OnPlaybackPositionUpdateListener {
		void onMarkerReached(AudioTrack track);
		void onPeriodicNotification(AudioTrack track);
	}

	public static final int SUCCESS = 0;
	public static final int ERROR = -1;
	public static final int ERROR_BAD_VALUE = -2; // basically EINVAL
	public static final int ERROR_INVALID_OPERATION = -3;

	public static final int PLAYSTATE_STOPPED = 1;
	public static final int PLAYSTATE_PAUSED = 2;
	public static final int PLAYSTATE_PLAYING = 3;

	public static final int STATE_UNINITIALIZED = 0;
	public static final int STATE_INITIALIZED = 1;
	public static final int STATE_NO_STATIC_DATA = 2;

	public static final int MODE_STATIC = 0;
	public static final int MODE_STREAM = 1;

	public static final int WRITE_BLOCKING = 0;
	public static final int WRITE_NON_BLOCKING = 1;

	public static final int PERFORMANCE_MODE_NONE = 0;
	public static final int PERFORMANCE_MODE_LOW_LATENCY = 1;
	public static final int PERFORMANCE_MODE_POWER_SAVING = 2;

	int streamType;
	int sampleRateInHz;
	int channelConfig;
	int audioFormat;
	int bufferSizeInBytes;
	int mode;
	private int sessionId;
	private int playbackState = PLAYSTATE_STOPPED;
	private int playbackHeadPosition = 0;
	private float volume = 1.f;
	private byte[] writeScratch; // for write(ByteBuffer, ...) on a direct buffer

	// for native code's use
	long pcm_handle;
	long params;
	int channels;
	int period_time;
	/** frames in the ALSA ring buffer; the native constructor overwrites the estimate below */
	int bufferSizeInFrames;
	/** XRUNs seen by native_write() since construction */
	int underrunCount;
	// mostly
	static int frames;
	OnPlaybackPositionUpdateListener periodic_update_listener;

	native void native_constructor(int streamType, int sampleRateInHz, int num_channels, int audioFormat, int bufferSizeInBytes, int mode);
	public AudioTrack(int streamType, int sampleRateInHz, int channelConfig, int audioFormat, int bufferSizeInBytes, int mode) {
		this.streamType = streamType;
		this.sampleRateInHz = sampleRateInHz;
		this.channelConfig = channelConfig;
		this.audioFormat = audioFormat;
		this.bufferSizeInBytes = bufferSizeInBytes;
		this.mode = mode;
		// pre-seed what the native constructor reports back, so that the accessors
		// are still answerable when the PCM cannot be opened at all
		this.channels = channelCountForConfig(channelConfig);
		this.bufferSizeInFrames = bufferSizeInBytes / (this.channels * 2); // 2 means PCM16

		native_constructor(streamType, sampleRateInHz, channelConfig, audioFormat, bufferSizeInBytes, mode);
	}

	public AudioTrack(int streamType, int sampleRateInHz, int channelConfig, int audioFormat, int bufferSizeInBytes, int mode, int sessionId) {
		this(streamType, sampleRateInHz, channelConfig, audioFormat, bufferSizeInBytes, mode);
		this.sessionId = sessionId != AudioManager.AUDIO_SESSION_ID_GENERATE ? sessionId : AudioManager.nextAudioSessionId();
	}

	public AudioTrack(AudioAttributes attributes, AudioFormat format, int bufferSizeInBytes, int mode, int sessionId) {
		this(attributes.streamType, format.sampleRate, format.channelMask, format.encoding, bufferSizeInBytes, mode, sessionId);
	}

	/** Mirrors the channel_config switch in api-impl-jni/audio/audio.c. */
	static int channelCountForConfig(int channelConfig) {
		return channelConfig == AudioFormat.CHANNEL_OUT_STEREO ? 2 : 1;
	}

	public static native int getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat);

	public void setPlaybackPositionUpdateListener(OnPlaybackPositionUpdateListener listener) {
		this.periodic_update_listener = listener;
	}

	public int setPositionNotificationPeriod(int periodInFrames) {
		System.out.println("\n\nAudioTrack.nsetPositionNotificationPeriod(" + periodInFrames + "); called\n\n\n\n");
		return 0; // SUCCESS
	}

	public int getPositionNotificationPeriod() {
		return this.frames;
	}

	public void play() {
		playbackState = PLAYSTATE_PLAYING;
		native_play();
	}

	public void stop() {
		System.out.println("STUB: AudioTrack.stop()\n");
		playbackState = PLAYSTATE_STOPPED;
	}

	public void flush() {
		System.out.println("STUB: AudioTrack.flush()\n");
	}

	public void release() {
		native_release();
		pcm_handle = 0; // so that a second release() is not a double close
		playbackState = PLAYSTATE_STOPPED;
	}

	/** STATE_INITIALIZED only when the native constructor really got a PCM. */
	public int getState() {
		return pcm_handle != 0 ? STATE_INITIALIZED : STATE_UNINITIALIZED;
	}

	public int write(byte[] audioData, int offsetInBytes, int sizeInBytes) {
		/* sanity check the parameters before calling native_write */
		if ((audioData == null)
		    || (offsetInBytes < 0) || (sizeInBytes < 0)
		    || (offsetInBytes + sizeInBytes < 0)
		    || (offsetInBytes + sizeInBytes > audioData.length)) {
			return ERROR_BAD_VALUE;
		}

		int framesToWrite = sizeInBytes / channels / 2; // 2 means PCM16
		int ret = native_write(audioData, offsetInBytes, framesToWrite, volume);
		if (ret > 0) {
			playbackHeadPosition += ret;
		}
		return ret * channels * 2; // 2 means PCM16
	}

	/**
	 * As in AOSP: the bytes are taken from the buffer's current position and the
	 * position is advanced by however many were written. webrtc hands us a direct
	 * buffer, which has no backing array, hence the scratch copy.
	 */
	public int write(ByteBuffer audioData, int sizeInBytes, int writeMode) {
		if (audioData == null || sizeInBytes < 0 || sizeInBytes > audioData.remaining()) {
			return ERROR_BAD_VALUE;
		}

		int written;
		if (audioData.hasArray()) {
			written = write(audioData.array(), audioData.arrayOffset() + audioData.position(), sizeInBytes);
		} else {
			if (writeScratch == null || writeScratch.length < sizeInBytes) {
				writeScratch = new byte[sizeInBytes];
			}
			// duplicate() so that a failed write leaves the position alone
			audioData.duplicate().get(writeScratch, 0, sizeInBytes);
			written = write(writeScratch, 0, sizeInBytes);
		}

		if (written > 0) {
			audioData.position(audioData.position() + written);
		}
		return written;
	}

	public int write(short audioData[], int offsetInShorts, int sizeInShorts) {
		/* sanity check the parameters before calling native_write */
		if ((audioData == null)
		    || (offsetInShorts < 0) || (sizeInShorts < 0)
		    || (offsetInShorts + sizeInShorts < 0)
		    || (offsetInShorts + sizeInShorts > audioData.length)) {
			return ERROR_BAD_VALUE;
		}

		int framesToWrite = sizeInShorts / channels;
		int ret = native_write(audioData, offsetInShorts, framesToWrite, volume);
		if (ret > 0) {
			playbackHeadPosition += ret;
		}
		return ret * channels;
	}

	public int getAudioSessionId() {
		return sessionId;
	}

	public int getSampleRate() {
		return sampleRateInHz;
	}

	/** No setPlaybackRate() here, so the playback rate is always the sample rate. */
	public int getPlaybackRate() {
		return sampleRateInHz;
	}

	public int getChannelCount() {
		return channels;
	}

	/** ALSA has a single ring buffer, so the size and the capacity are the same. */
	public int getBufferSizeInFrames() {
		return bufferSizeInFrames;
	}

	public int getBufferCapacityInFrames() {
		return bufferSizeInFrames;
	}

	/** The ALSA buffer is negotiated at snd_pcm_hw_params() time and cannot be resized. */
	public int setBufferSizeInFrames(int bufferSizeInFrames) {
		return ERROR_INVALID_OPERATION;
	}

	public int getUnderrunCount() {
		return underrunCount;
	}

	public int setStereoVolume(float leftVolume, float rightVolume) {
		this.volume = (leftVolume + rightVolume) / 2;
		return 0;
	}

	public int getPlayState() {
		return playbackState;
	}

	public void pause() {
		playbackState = PLAYSTATE_PAUSED;
		native_pause();
	}

	public int getPlaybackHeadPosition() {
		return playbackHeadPosition - native_getPlaybackHeadPosition();
	}

	public int setVolume(float volume) {
		this.volume = volume;
		return 0;
	}

	public boolean getTimestamp(AudioTimestamp timestamp) {
		return false;
	}

	private native int native_getPlaybackHeadPosition();
	public native void native_play();
	public native void native_pause();
	private native int native_write(byte[] audioData, int offsetInBytes, int framesToWrite, float volume);
	private native int native_write(short[] audioData, int offsetInShorts, int framesToWrite, float volume);
	public native void native_release();

	/** There is one ALSA device and no per-stream routing, so every stream runs at it. */
	public static int getNativeOutputSampleRate(int streamType) {
		return AudioManager.OUTPUT_SAMPLE_RATE;
	}

	// nested classes
	public static class Builder {
		private AudioAttributes mAttributes;
		private AudioFormat mFormat;
		private int mBufferSizeInBytes;
		private int mTransferMode = MODE_STREAM;
		private int mSessionId = AudioManager.AUDIO_SESSION_ID_GENERATE;

		public Builder() {}

		public AudioTrack build() {
			AudioAttributes attributes = mAttributes != null ? mAttributes : new AudioAttributes.Builder().build();
			AudioFormat format = mFormat != null ? mFormat : new AudioFormat.Builder().build();
			int bufferSizeInBytes = mBufferSizeInBytes > 0 ? mBufferSizeInBytes
			    : getMinBufferSize(format.sampleRate, format.channelMask, format.encoding);
			return new AudioTrack(attributes, format, bufferSizeInBytes, mTransferMode, mSessionId);
		}

		public Builder setAudioAttributes(AudioAttributes attributes) {
			mAttributes = attributes;
			return this;
		}

		public Builder setAudioFormat(AudioFormat format) {
			mFormat = format;
			return this;
		}

		public Builder setBufferSizeInBytes(int bufferSizeInBytes) {
			mBufferSizeInBytes = bufferSizeInBytes;
			return this;
		}

		public Builder setTransferMode(int mode) {
			mTransferMode = mode;
			return this;
		}

		/** Ignored: ALSA is opened the same way whatever latency the app asks for. */
		public Builder setPerformanceMode(int performanceMode) {
			return this;
		}

		public Builder setSessionId(int sessionId) throws IllegalArgumentException {
			mSessionId = sessionId;
			return this;
		}
	}

	public static float getMaxVolume() { return 1.0f; }

	public static float getMinVolume() { return 0.0f; }
}
