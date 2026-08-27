package android.media;

public class AudioRecord {

	public static final int RECORDSTATE_STOPPED = 1;
	public static final int RECORDSTATE_RECORDING = 3;

	public static final int ERROR_BAD_VALUE = -2;

	private long pcm_handle;
	private int channels; // set by native constructor
	private int recordingState = RECORDSTATE_STOPPED;
	private int sampleRateInHz;
	private short[] scratch; // reused by read(ByteBuffer, int), which runs per frame

	private native long native_constructor(int streamType, int sampleRateInHz, int num_channels, int audioFormat, int bufferSizeInBytes);
	private native void native_record(long pcm_handle);
	private native void native_stop(long pcm_handle);
	private native int native_read(long pcm_handle, short[] audioData, int offsetInShorts, int framesToWrite);
	private native void native_release(long pcm_handle);

	public AudioRecord(int streamType, int sampleRateInHz, int channelConfig, int audioFormat, int bufferSizeInBytes) {
		this.sampleRateInHz = sampleRateInHz;
		pcm_handle = native_constructor(streamType, sampleRateInHz, channelConfig, audioFormat, bufferSizeInBytes);
	}

	public static native int getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat);

	public int getSampleRate() {
		return sampleRateInHz;
	}

	public int getState() {
		return /*STATE_INITIALIZED*/ 1;
	}

	public int getRecordingState() {
		return recordingState;
	}

	public void startRecording() {
		native_record(pcm_handle);
		recordingState = RECORDSTATE_RECORDING;
	}

	public int read(short[] audioData, int offsetInShorts, int sizeInShorts) {
		/* sanity check the parameters before calling native_write */
		if ((audioData == null)
		    || (offsetInShorts < 0) || (sizeInShorts < 0)
		    || (offsetInShorts + sizeInShorts < 0)
		    || (offsetInShorts + sizeInShorts > audioData.length)) {
			return ERROR_BAD_VALUE;
		}

		return native_read(pcm_handle, audioData, offsetInShorts, sizeInShorts / channels) * channels;
	}

	/**
	 * The overload recording loops use, so that the frames can be handed to a
	 * native encoder without copying them out of a direct buffer again. As in
	 * AOSP the data lands at the start of the buffer and the position is left
	 * alone; native_read() takes a short[], hence the scratch array.
	 */
	public int read(java.nio.ByteBuffer audioBuffer, int sizeInBytes) {
		if (audioBuffer == null || sizeInBytes < 0) {
			return ERROR_BAD_VALUE;
		}

		int sizeInShorts = Math.min(sizeInBytes, audioBuffer.capacity()) / 2;
		if (scratch == null || scratch.length < sizeInShorts) {
			scratch = new short[sizeInShorts];
		}

		int shortsRead = read(scratch, 0, sizeInShorts);
		if (shortsRead <= 0) {
			return shortsRead;
		}

		// duplicate() so the caller's position survives; it does not inherit the
		// byte order, and asShortBuffer() honours whatever order is set here.
		java.nio.ByteBuffer dup = audioBuffer.duplicate();
		dup.order(audioBuffer.order());
		dup.position(0);
		dup.asShortBuffer().put(scratch, 0, shortsRead);

		return shortsRead * 2;
	}

	public void stop() {
		native_stop(pcm_handle);
		recordingState = RECORDSTATE_STOPPED;
	}

	public void release() {
		native_release(pcm_handle);
		pcm_handle = 0;
	}

	public static class Builder {

		private int audioSource;
		private AudioFormat audioFormat;

		public Builder setAudioSource(int audioSource) {
			this.audioSource = audioSource;
			return this;
		}

		public Builder setAudioFormat(AudioFormat audioFormat) {
			this.audioFormat = audioFormat;
			return this;
		}

		public AudioRecord build() {
			return new AudioRecord(audioSource, audioFormat.sampleRate, audioFormat.channelMask, audioFormat.encoding, 32768);
		}
	
	public android.media.AudioRecord.Builder setBufferSizeInBytes(int a0) throws java.lang.IllegalArgumentException { return null; }
}

	public android.media.AudioDeviceInfo getRoutedDevice() { return null; }

	public android.media.AudioFormat getFormat() { return null; }

	public boolean setPreferredDevice(android.media.AudioDeviceInfo a0) { return false; }

	public int getAudioFormat() { return 0; }

	public int getAudioSessionId() { return 0; }

	public int getAudioSource() { return 0; }

	public int getBufferSizeInFrames() { return 0; }

	public int getChannelCount() { return 0; }

	public static final int ERROR = -1;

	public static final int ERROR_INVALID_OPERATION = -3;

	public static final int STATE_INITIALIZED = 1;

	public static final int SUCCESS = 0;

	public int getTimestamp(android.media.AudioTimestamp a0, int a1) { return 0; }
}
