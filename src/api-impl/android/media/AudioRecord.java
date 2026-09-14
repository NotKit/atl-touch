package android.media;

import java.nio.ByteBuffer;

import android.os.Handler;

public class AudioRecord implements AudioRouting, MicrophoneDirection {

	public static final int RECORDSTATE_STOPPED = 1;
	public static final int RECORDSTATE_RECORDING = 3;

	public static final int ERROR_BAD_VALUE = -2;

	private long pcm_handle;
	private int channels; // set by native constructor
	private int recordingState = RECORDSTATE_STOPPED;
	private int sampleRateInHz;
	private AudioFormat format;
	private short[] scratch; // reused by read(ByteBuffer, int), which runs per frame

	private native long native_constructor(int streamType, int sampleRateInHz, int num_channels, int audioFormat, int bufferSizeInBytes);
	private native void native_record(long pcm_handle);
	private native void native_stop(long pcm_handle);
	private native int native_read(long pcm_handle, short[] audioData, int offsetInShorts, int framesToWrite);
	private native void native_release(long pcm_handle);

	public AudioRecord(int streamType, int sampleRateInHz, int channelConfig, int audioFormat, int bufferSizeInBytes) {
		this.sampleRateInHz = sampleRateInHz;
		this.format = new AudioFormat();
		this.format.sampleRate = sampleRateInHz;
		this.format.channelMask = channelConfig;
		this.format.encoding = audioFormat;
		pcm_handle = native_constructor(streamType, sampleRateInHz, channelConfig, audioFormat, bufferSizeInBytes);
	}

	public static native int getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat);

	public int getSampleRate() {
		return sampleRateInHz;
	}

	public AudioFormat getFormat() {
		return format;
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

	/* 16-bit PCM is the only encoding the native side records */
	public int read(byte[] audioData, int offsetInBytes, int sizeInBytes) {
		if ((audioData == null)
		    || (offsetInBytes < 0) || (sizeInBytes < 0)
		    || (offsetInBytes + sizeInBytes < 0)
		    || (offsetInBytes + sizeInBytes > audioData.length)) {
			return ERROR_BAD_VALUE;
		}

		short[] samples = new short[sizeInBytes / 2];
		int read = read(samples, 0, samples.length);
		if (read < 0)
			return read;
		for (int i = 0; i < read; i++) {
			audioData[offsetInBytes + 2 * i] = (byte)(samples[i] & 0xff);
			audioData[offsetInBytes + 2 * i + 1] = (byte)(samples[i] >> 8);
		}
		return read * 2;
	}

	/**
	 * The overload recording loops use, so that the frames can be handed to a
	 * native encoder without copying them out of a direct buffer again. As in
	 * AOSP the data lands at the start of the buffer and the position is left
	 * alone; native_read() takes a short[], hence the scratch array.
	 */
	public int read(ByteBuffer audioBuffer, int sizeInBytes) {
		if (audioBuffer == null || sizeInBytes < 0)
			return ERROR_BAD_VALUE;

		int sizeInShorts = Math.min(sizeInBytes, audioBuffer.capacity()) / 2;
		if (scratch == null || scratch.length < sizeInShorts)
			scratch = new short[sizeInShorts];

		int shortsRead = read(scratch, 0, sizeInShorts);
		if (shortsRead <= 0)
			return shortsRead;

		// duplicate() so the caller's position survives; it does not inherit the
		// byte order, and asShortBuffer() honours whatever order is set here.
		ByteBuffer dup = audioBuffer.duplicate();
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

	@Override
	public AudioDeviceInfo getPreferredDevice() {
		return null;
	}

	@Override
	public boolean setPreferredDevice(AudioDeviceInfo deviceInfo) {
		return true;
	}

	@Override
	public AudioDeviceInfo getRoutedDevice() {
		return null;
	}

	@Override
	public void addOnRoutingChangedListener(OnRoutingChangedListener listener, Handler handler) {
	}

	@Override
	public void removeOnRoutingChangedListener(OnRoutingChangedListener listener) {
	}

	@Override
	public boolean setPreferredMicrophoneDirection(int direction) {
		return true;
	}

	@Override
	public boolean setPreferredMicrophoneFieldDimension(float zoom) {
		return true;
	}

	public static class Builder {

		private int audioSource;
		private AudioFormat audioFormat;
		private int bufferSizeInBytes = 32768;

		public Builder setAudioSource(int audioSource) {
			this.audioSource = audioSource;
			return this;
		}

		public Builder setAudioFormat(AudioFormat audioFormat) {
			this.audioFormat = audioFormat;
			return this;
		}

		public Builder setBufferSizeInBytes(int bufferSizeInBytes) {
			this.bufferSizeInBytes = bufferSizeInBytes;
			return this;
		}

		public AudioRecord build() {
			return new AudioRecord(audioSource, audioFormat.sampleRate, audioFormat.channelMask, audioFormat.encoding, bufferSizeInBytes);
		}
	}

	public static final int ERROR = -1;
	public static final int ERROR_INVALID_OPERATION = -3;
	public static final int STATE_INITIALIZED = 1;
	public static final int SUCCESS = 0;

	public int getAudioFormat() { return 0; }

	public int getAudioSessionId() { return 0; }

	public int getAudioSource() { return 0; }

	public int getBufferSizeInFrames() { return 0; }

	public int getChannelCount() { return 0; }

	public int getTimestamp(AudioTimestamp timestamp, int timebase) { return ERROR_INVALID_OPERATION; }
}
