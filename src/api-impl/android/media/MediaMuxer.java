package android.media;

import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * Writes the access units a MediaCodec encoder produced into an MP4.
 *
 * One video track: that is what a camera app muxes, and what the native
 * pipeline (video_muxer.h) carries. stop() is what writes the moov, so a file
 * whose muxer was never stopped is not playable.
 */
public final class MediaMuxer {

	public static final class OutputFormat {
		public static final int MUXER_OUTPUT_MPEG_4 = 0;
		public static final int MUXER_OUTPUT_WEBM = 1;
		public static final int MUXER_OUTPUT_3GPP = 2;
		public static final int MUXER_OUTPUT_HEIF = 3;
		public static final int MUXER_OUTPUT_OGG = 4;
	}

	private long nativePtr;
	private boolean started;
	private boolean stopped;
	private int trackCount;

	public MediaMuxer(String path, int format) throws IOException {
		if (path == null)
			throw new IllegalArgumentException("path must not be null");
		if (format != OutputFormat.MUXER_OUTPUT_MPEG_4 && format != OutputFormat.MUXER_OUTPUT_3GPP)
			throw new IllegalArgumentException("only MP4 output is implemented, not format " + format);

		nativePtr = native_create(path);
		if (nativePtr == 0)
			throw new IOException("cannot write " + path);
	}

	public int addTrack(MediaFormat format) {
		checkOpen();
		if (started)
			throw new IllegalStateException("addTrack() after start()");
		if (format == null)
			throw new IllegalArgumentException("format must not be null");

		String mime = format.getString(MediaFormat.KEY_MIME);
		if (mime != null && !mime.equals("video/avc"))
			throw new IllegalArgumentException("only video/avc tracks are implemented, not " + mime);

		int width = format.containsKey(MediaFormat.KEY_WIDTH) ? format.getInteger(MediaFormat.KEY_WIDTH) : 0;
		int height = format.containsKey(MediaFormat.KEY_HEIGHT) ? format.getInteger(MediaFormat.KEY_HEIGHT) : 0;
		int fps = format.containsKey(MediaFormat.KEY_FRAME_RATE) ? format.getInteger(MediaFormat.KEY_FRAME_RATE) : 0;
		ByteBuffer csd = format.getByteBuffer("csd-0");

		if (!native_addTrack(nativePtr, width, height, fps, csd))
			throw new IllegalStateException("the muxer refused the track");
		return trackCount++;
	}

	public void setOrientationHint(int degrees) {
		checkOpen();
		if (started)
			throw new IllegalStateException("setOrientationHint() after start()");
		native_setOrientation(nativePtr, degrees);
	}

	public void setLocation(float latitude, float longitude) {
		checkOpen();
	}

	public void start() {
		checkOpen();
		if (started)
			throw new IllegalStateException("this muxer is already started");
		if (trackCount == 0)
			throw new IllegalStateException("start() without a track");
		if (!native_start(nativePtr))
			throw new IllegalStateException("the muxer refused to start");
		started = true;
	}

	public void writeSampleData(int trackIndex, ByteBuffer byteBuf, MediaCodec.BufferInfo bufferInfo) {
		checkOpen();
		if (!started)
			throw new IllegalStateException("writeSampleData() before start()");
		if (trackIndex < 0 || trackIndex >= trackCount)
			throw new IllegalArgumentException("no track " + trackIndex);
		if (byteBuf == null || bufferInfo == null)
			throw new IllegalArgumentException("writeSampleData needs a buffer and its info");
		if (bufferInfo.size <= 0)
			return;

		boolean keyframe = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0;

		/* AOSP window: the sample is what the info describes, wherever the app
		 * left the buffer's own position */
		byteBuf.limit(bufferInfo.offset + bufferInfo.size);
		byteBuf.position(bufferInfo.offset);
		native_write(nativePtr, byteBuf, bufferInfo.size, bufferInfo.presentationTimeUs, keyframe);
	}

	public void stop() {
		checkOpen();
		if (!started || stopped)
			throw new IllegalStateException("this muxer is not started");
		stopped = true;
		native_stop(nativePtr);
	}

	public void release() {
		if (nativePtr != 0) {
			native_release(nativePtr);
			nativePtr = 0;
		}
	}

	private void checkOpen() {
		if (nativePtr == 0)
			throw new IllegalStateException("this MediaMuxer has been released");
	}

	private static native long native_create(String path);
	private static native boolean native_addTrack(long ptr, int width, int height, int fps, ByteBuffer csd);
	private static native void native_setOrientation(long ptr, int degrees);
	private static native boolean native_start(long ptr);
	private static native void native_write(long ptr, ByteBuffer buffer, int size,
	    long presentationTimeUs, boolean keyframe);
	private static native void native_stop(long ptr);
	private static native void native_release(long ptr);
}
