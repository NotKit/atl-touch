package android.media;

import android.media.MediaCodec.BufferInfo;
import android.view.Surface;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayDeque;
import java.util.Queue;

public class MediaCodec {

	public static final int BUFFER_FLAG_KEY_FRAME = 0x1;
	public static final int BUFFER_FLAG_SYNC_FRAME = 0x1;
	public static final int BUFFER_FLAG_CODEC_CONFIG = 0x2;
	public static final int BUFFER_FLAG_END_OF_STREAM = 0x4;

	public static final int CONFIGURE_FLAG_ENCODE = 0x1;

	public static final int INFO_TRY_AGAIN_LATER = -1;
	public static final int INFO_OUTPUT_FORMAT_CHANGED = -2;
	public static final int INFO_OUTPUT_BUFFERS_CHANGED = -3;

	private String codecName;
	private ByteBuffer[] inputBuffers;
	private ByteBuffer[] outputBuffers;
	private long[] inputBufferTimestamps;
	private long native_codec;
	private boolean outputFormatSet = false;
	private MediaFormat mediaFormat;

	private Queue<Integer> freeOutputBuffers;
	private Queue<Integer> queuedInputBuffers;
	private Queue<Integer> freeInputBuffers;

	/* encoder mode (createEncoderByType + createInputSurface): the frames come
	 * from a Surface, so there is no input buffer loop, and the output is the
	 * encoded access units the native encoder hands over */
	private final boolean encoder;
	private long native_encoder;
	private Surface inputSurface;
	private int pendingOutputIndex = -1;
	private final BufferInfo pendingInfo = new BufferInfo();

	private MediaCodec(String codecName) throws IOException {
		this.codecName = codecName;
		this.encoder = false;
		native_codec = native_constructor(codecName);
		if (native_codec == 0) {
			throw new IOException("Unable to create MediaCodec: " + codecName);
		}
	}

	private MediaCodec(String codecName, boolean encoder) {
		this.codecName = codecName;
		this.encoder = encoder;
	}

	public static MediaCodec createByCodecName(String codecName) throws IOException {
		return new MediaCodec(codecName);
	}

	/** ATL encodes H.264 and nothing else; the encoder itself is built by configure(). */
	public static MediaCodec createEncoderByType(String type) throws IOException {
		if (!"video/avc".equals(type))
			throw new IOException("no encoder for " + type);
		return new MediaCodec("h264-encoder", true);
	}

	public String getName() {
		return codecName;
	}

	public void configure(MediaFormat format, Surface surface, MediaCrypto crypto, int flags) {
		System.out.println("MediaCodec.configure(" + format + ", " + surface + ", " + crypto + ", " + flags + "): codecName=" + codecName);
		this.mediaFormat = format;

		if (encoder) {
			configureEncoder(format);
			return;
		}

		int maxInputSize = 262144;
		if (format.containsKey("max-input-size")) {
			maxInputSize = format.getInteger("max-input-size");
		}
		inputBuffers = new ByteBuffer[1];
		inputBufferTimestamps = new long[inputBuffers.length];
		freeInputBuffers = new ArrayDeque<>(inputBuffers.length);
		queuedInputBuffers = new ArrayDeque<>(inputBuffers.length);
		for (int i = 0; i < inputBuffers.length; i++) {
			inputBuffers[i] = ByteBuffer.allocate(maxInputSize).order(ByteOrder.LITTLE_ENDIAN);
			freeInputBuffers.add(i);
		}
		outputBuffers = new ByteBuffer[2];
		freeOutputBuffers = new ArrayDeque<>(outputBuffers.length);
		for (int i = 0; i < outputBuffers.length; i++) {
			outputBuffers[i] = ByteBuffer.allocate(8192).order(ByteOrder.LITTLE_ENDIAN);
			freeOutputBuffers.add(i);
		}

		if ("aac".equals(codecName) || "mp3".equals(codecName) || "opus".equals(codecName)) {
			native_configure_audio(native_codec, format.getByteBuffer("csd-0"), format.getInteger("sample-rate"), format.getInteger("channel-count"));
		} else if ("h264".equals(codecName) || "vp8".equals(codecName) || "vp9".equals(codecName)) {
			native_configure_video(native_codec, format.getByteBuffer("csd-0"), format.getByteBuffer("csd-1"), surface);
		} else {
			System.out.println("configure: format " + format + " not implemented");
			Thread.dumpStack();
			System.exit(1);
		}
	}

	/* the encoder half: one native encoder, its Surface, and the access units
	 * it produces read straight into the output buffers */
	private void configureEncoder(MediaFormat format) {
		int width = format.getInteger(MediaFormat.KEY_WIDTH);
		int height = format.getInteger(MediaFormat.KEY_HEIGHT);
		int bitRate = format.getInteger(MediaFormat.KEY_BIT_RATE, 0);
		int frameRate = format.getInteger(MediaFormat.KEY_FRAME_RATE, 30);

		native_encoder = native_encoder_create(width, height, frameRate, bitRate);
		if (native_encoder == 0)
			throw new IllegalStateException("cannot encode " + width + "x" + height + " H.264");

		/* one access unit fits in an uncompressed frame, whatever the bitrate */
		int bufferSize = Math.max(width * height * 3 / 2, 65536);
		outputBuffers = new ByteBuffer[2];
		freeOutputBuffers = new ArrayDeque<>(outputBuffers.length);
		for (int i = 0; i < outputBuffers.length; i++) {
			outputBuffers[i] = ByteBuffer.allocate(bufferSize).order(ByteOrder.LITTLE_ENDIAN);
			freeOutputBuffers.add(i);
		}
	}

	/** The encoder's input: a Surface a camera2 session (or anything else) fills. */
	public Surface createInputSurface() {
		if (!encoder || native_encoder == 0)
			throw new IllegalStateException("createInputSurface() needs a configured encoder");
		if (inputSurface == null) {
			inputSurface = new Surface();
			native_encoder_inputSurface(native_encoder, inputSurface);
		}
		return inputSurface;
	}

	public void signalEndOfInputStream() {
		if (!encoder || native_encoder == 0)
			throw new IllegalStateException("signalEndOfInputStream() needs a configured encoder");
		native_encoder_signalEndOfInputStream(native_encoder);
	}

	public void start() {
		System.out.println("MediaCodec.start(): codecName=" + codecName);
		if (encoder) {
			if (native_encoder == 0 || !native_encoder_start(native_encoder))
				throw new IllegalStateException("the encoder refused to start");
			return;
		}
		native_start(native_codec);
	}

	public ByteBuffer[] getInputBuffers() {
		return inputBuffers;
	}

	public ByteBuffer getInputBuffer(int index) {
		return inputBuffers[index];
	}

	public ByteBuffer[] getOutputBuffers() {
		return outputBuffers;
	}

	public ByteBuffer getOutputBuffer(int index) {
		return outputBuffers[index];
	}

	public int dequeueOutputBuffer(BufferInfo info, long timeoutUs) {
		if (encoder)
			return dequeueEncodedBuffer(info, timeoutUs);
		if (!outputFormatSet) {
			outputFormatSet = true;
			return /*INFO_OUTPUT_FORMAT_CHANGED*/ -2;
		}
		Integer index = freeOutputBuffers.poll();
		if (index == null) {
			return /*INFO_TRY_AGAIN_LATER*/ -1;
		}
		outputBuffers[index].clear();
		int ret = native_dequeueOutputBuffer(native_codec, outputBuffers[index], info);
		if (ret == 0) {
			ret = index;
		} else {
			freeOutputBuffers.add(index);
			ret = /*INFO_TRY_AGAIN_LATER*/ -1;
		}
		return ret;
	}

	/*
	 * The first access unit is what says what the stream is, so it is held
	 * back until the app has been told the output format — the order AOSP
	 * reports and every muxing loop is written against.
	 */
	private int dequeueEncodedBuffer(BufferInfo info, long timeoutUs) {
		if (native_encoder == 0)
			return INFO_TRY_AGAIN_LATER;

		if (pendingOutputIndex < 0) {
			Integer index = freeOutputBuffers.poll();

			if (index == null)
				return INFO_TRY_AGAIN_LATER;
			outputBuffers[index].clear();
			if (native_encoder_dequeue(native_encoder, outputBuffers[index], pendingInfo, timeoutUs) != 0) {
				freeOutputBuffers.add(index);
				return INFO_TRY_AGAIN_LATER;
			}
			pendingOutputIndex = index;
		}
		if (!outputFormatSet) {
			outputFormatSet = true;
			return INFO_OUTPUT_FORMAT_CHANGED;
		}

		int index = pendingOutputIndex;
		pendingOutputIndex = -1;
		info.offset = pendingInfo.offset;
		info.size = pendingInfo.size;
		info.flags = pendingInfo.flags;
		info.presentationTimeUs = pendingInfo.presentationTimeUs;
		outputBuffers[index].position(0);
		outputBuffers[index].limit(info.offset + info.size);
		return index;
	}

	public void releaseOutputBuffer(int index, boolean render) {
		if (encoder) {
			freeOutputBuffers.add(index);
			return;
		}
		native_releaseOutputBuffer(native_codec, outputBuffers[index], render);
		freeOutputBuffers.add(index);
	}

	public void releaseOutputBuffer(int index, long presentationTimeUs) {
		native_releaseOutputBuffer(native_codec, outputBuffers[index], true);
		freeOutputBuffers.add(index);
	}

	public MediaFormat getOutputFormat(int index) { return null; }

	/** For an encoder this carries csd-0, the SPS/PPS of the stream so far. */
	public MediaFormat getOutputFormat() {
		if (!encoder || native_encoder == 0)
			return mediaFormat;

		MediaFormat format = MediaFormat.createVideoFormat("video/avc",
		    mediaFormat.getInteger(MediaFormat.KEY_WIDTH),
		    mediaFormat.getInteger(MediaFormat.KEY_HEIGHT));
		byte[] csd = native_encoder_csd(native_encoder);

		format.setInteger(MediaFormat.KEY_FRAME_RATE,
		    mediaFormat.getInteger(MediaFormat.KEY_FRAME_RATE, 30));
		if (csd != null)
			format.setByteBuffer("csd-0", ByteBuffer.wrap(csd));
		return format;
	}

	public void flush() {}

	private void tryProcessInputBuffer() {
		Integer index = queuedInputBuffers.peek();
		if (index != null) {
			int ret;
			if (index == -1) { // end of stream
				ret = native_queueInputBuffer(native_codec, null, 0);
			} else {
				ret = native_queueInputBuffer(native_codec, inputBuffers[index], inputBufferTimestamps[index]);
			}
			if (ret == 0) {
				queuedInputBuffers.remove(index);
				if (index != -1)
					freeInputBuffers.add(index);
			}
		}
	}

	public int dequeueInputBuffer(long timeoutUs) {
		tryProcessInputBuffer();
		Integer index = freeInputBuffers.poll();
		if (index == null) {
			return /*INFO_TRY_AGAIN_LATER*/ -1;
		} else {
			return index;
		}
	}

	public void queueInputBuffer(int index, int offset, int size, long presentationTimeUs, int flags) {
		if ((flags & BUFFER_FLAG_END_OF_STREAM) != 0) {
			queuedInputBuffers.add(-1);
			tryProcessInputBuffer();
			return;
		}
		inputBufferTimestamps[index] = presentationTimeUs;
		queuedInputBuffers.add(index);
		tryProcessInputBuffer();
	}

	public void setVideoScalingMode(int mode) {
		System.out.println("MediaCodec.setVideoScalingMode(" + mode + "): codecName=" + codecName);
	}

	public void stop() {
		if (encoder && native_encoder != 0)
			native_encoder_finish(native_encoder);
	}

	public void release() {
		System.out.println("MediaCodec.release(): codecName=" + codecName);
		if (native_encoder != 0) {
			native_encoder_release(native_encoder);
			native_encoder = 0;
			if (inputSurface != null) {
				inputSurface.release();
				inputSurface = null;
			}
		}
		if (native_codec != 0) {
			if (outputBuffers != null) {
				for (int i = 0; i < outputBuffers.length; i++) {
					if (!freeOutputBuffers.contains(i))
						releaseOutputBuffer(i, false);
				}
			}
			native_release(native_codec);
		}
		native_codec = 0;
	}

	@Override
	@SuppressWarnings("deprecation")
	protected void finalize() throws Throwable {
		try {
			super.finalize();
		} finally {
			release();
		}
	}

	private native long native_constructor(String codecName);
	private native void native_configure_audio(long codec, ByteBuffer extradata, int sampleRate, int channelCount);
	private native void native_configure_video(long codec, ByteBuffer csd0, ByteBuffer csd1, Surface surface);
	private native void native_start(long codec);
	private native int native_queueInputBuffer(long codec, ByteBuffer buffer, long presentationTimeUs);
	private native int native_dequeueOutputBuffer(long codec, ByteBuffer buffer, BufferInfo info);
	private native void native_releaseOutputBuffer(long codec, ByteBuffer buffer, boolean render);
	private native void native_release(long codec);

	public static final class CryptoException extends RuntimeException {
		public int getErrorCode() { return 0; }
	}
	private static native long native_encoder_create(int width, int height, int fps, int bitRate);
	private static native void native_encoder_inputSurface(long encoder, Surface surface);
	private static native boolean native_encoder_start(long encoder);
	/* 0 with the info filled in, or -1 when nothing is encoded yet */
	private static native int native_encoder_dequeue(long encoder, ByteBuffer buffer, BufferInfo info,
	    long timeoutUs);
	private static native byte[] native_encoder_csd(long encoder);
	private static native void native_encoder_signalEndOfInputStream(long encoder);
	private static native void native_encoder_finish(long encoder);
	private static native void native_encoder_release(long encoder);

	public static final class CryptoInfo {
		public static final class Pattern {
			private int blocksToEncrypt;
			private int blocksToSkip;

			public Pattern(int blocksToEncrypt, int blocksToSkip) {
				this.blocksToEncrypt = blocksToEncrypt;
				this.blocksToSkip = blocksToSkip;
			}

			public void set(int blocksToEncrypt, int blocksToSkip) {
				this.blocksToEncrypt = blocksToEncrypt;
				this.blocksToSkip = blocksToSkip;
			}

			public int getEncryptBlocks() {
				return blocksToEncrypt;
			}

			public int getSkipBlocks() {
				return blocksToSkip;
			}
		}
	
	public android.media.MediaCodec.CryptoInfo.Pattern getPattern() { return null; }

	public byte[] iv;

	public byte[] key;

	public int mode;

	public int numSubSamples;

	public int[] numBytesOfClearData;

	public int[] numBytesOfEncryptedData;

	public void set(int a0, int[] a1, int[] a2, byte[] a3, byte[] a4, int a5) { }

	public void setPattern(android.media.MediaCodec.CryptoInfo.Pattern a0) { }
}

	public static final class BufferInfo {
		public int size;
		public int flags;
		public int offset;
		public long presentationTimeUs;
	
	public void set(int a0, int a1, long a2, int a3) { }
}

	public static interface OnFrameRenderedListener {}

	public static abstract class Callback {
	
	public void onError(android.media.MediaCodec a0, android.media.MediaCodec.CodecException a1) { }

	public void onInputBufferAvailable(android.media.MediaCodec a0, int a1) { }

	public void onOutputBufferAvailable(android.media.MediaCodec a0, int a1, android.media.MediaCodec.BufferInfo a2) { }

	public void onOutputFormatChanged(android.media.MediaCodec a0, android.media.MediaFormat a1) { }
}

	public static class CodecException extends IllegalStateException {
	
	public int getErrorCode() { return 0; }
}

	public android.media.MediaCodecInfo getCodecInfo() { return null; }

	public android.media.MediaFormat getInputFormat() { return null; }








	public static final java.lang.String PARAMETER_KEY_REQUEST_SYNC_FRAME = "request-sync";

	public static final java.lang.String PARAMETER_KEY_VIDEO_BITRATE = "video-bitrate";

	public void queueSecureInputBuffer(int a0, int a1, android.media.MediaCodec.CryptoInfo a2, long a3, int a4) throws android.media.MediaCodec.CryptoException { }

	public void setCallback(android.media.MediaCodec.Callback a0) { }

	public void setParameters(android.os.Bundle a0) { }
}
