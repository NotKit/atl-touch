package android.media;

import android.content.Context;
import android.view.Surface;

import java.io.File;
import java.io.FileDescriptor;
import java.io.IOException;

/**
 * Video recording onto a Surface a camera2 session can target.
 *
 * prepare() builds the native H.264 encoder (video_encoder.h) and hands out
 * its Surface; the capture session pushes frames into it and stop() finishes
 * the MP4. There is no audio track: ATL has no capture path for one, so an
 * audio source is accepted and logged rather than recorded.
 */
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

	public static final int MEDIA_RECORDER_INFO_MAX_DURATION_REACHED = 800;
	public static final int MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED = 801;

	public interface OnErrorListener {
		void onError(MediaRecorder recorder, int what, int extra);
	}
	public interface OnInfoListener {
		void onInfo(MediaRecorder recorder, int what, int extra);
	}

	public MediaRecorder() {}
	public MediaRecorder(Context context) {}

	private int audioSource = -1;
	private int videoSource = -1;
	private int outputFormat;
	private int audioEncoder;
	private int videoEncoder;
	private int videoWidth = 640;
	private int videoHeight = 480;
	private int videoFrameRate = 30;
	private int videoBitRate;
	private int orientationHint;
	private String outputPath;

	private long nativePtr;
	private Surface surface;
	private boolean prepared;
	private boolean recording;
	private OnErrorListener errorListener;
	private OnInfoListener infoListener;

	public void setAudioSource(int audioSource) {
		this.audioSource = audioSource;
	}

	public void setVideoSource(int videoSource) {
		this.videoSource = videoSource;
	}

	public void setOutputFormat(int outputFormat) {
		this.outputFormat = outputFormat;
	}

	public void setAudioEncoder(int audioEncoder) {
		this.audioEncoder = audioEncoder;
	}

	public void setVideoEncoder(int videoEncoder) {
		this.videoEncoder = videoEncoder;
	}

	public void setVideoSize(int width, int height) {
		if (width < 2 || height < 2)
			throw new IllegalArgumentException("a recording needs a positive size");
		videoWidth = width;
		videoHeight = height;
	}

	public void setVideoFrameRate(int rate) {
		videoFrameRate = rate;
	}

	public void setVideoEncodingBitRate(int bitRate) {
		videoBitRate = bitRate;
	}

	public void setOrientationHint(int degrees) {
		orientationHint = degrees;
	}

	public void setLocation(float latitude, float longitude) {}

	public void setMaxDuration(int maxDurationMs) {}

	public void setMaxFileSize(long maxFileSize) {}

	public void setPreviewDisplay(Surface surface) {}

	public void setCaptureRate(double fps) {
		if (fps > 0)
			videoFrameRate = (int)Math.round(fps);
	}

	public void setProfile(CamcorderProfile profile) {
		if (profile == null)
			return;
		outputFormat = profile.fileFormat;
		videoEncoder = profile.videoCodec;
		audioEncoder = profile.audioCodec;
		videoWidth = profile.videoFrameWidth;
		videoHeight = profile.videoFrameHeight;
		videoFrameRate = profile.videoFrameRate;
		videoBitRate = profile.videoBitRate;
	}

	public int getMaxAmplitude() {
		return 0;
	}

	public void setOnErrorListener(OnErrorListener listener) {
		errorListener = listener;
	}

	public void setOnInfoListener(OnInfoListener listener) {
		infoListener = listener;
	}

	public void setAudioEncodingBitRate(int audioEncodingBitrate) {}
	public void setAudioSamplingRate(int setAudioSamplingRate) {}
	public void setAudioChannels(int channels) {}

	public void setOutputFile(String filePath) {
		outputPath = filePath;
	}

	public void setOutputFile(File file) {
		outputPath = file == null ? null : file.getAbsolutePath();
	}

	public void setOutputFile(FileDescriptor fd) {
		throw new IllegalArgumentException("MediaRecorder needs a path, not a file descriptor");
	}

	public void prepare() throws IOException {
		if (prepared)
			throw new IllegalStateException("this MediaRecorder is already prepared");
		if (outputPath == null)
			throw new IllegalStateException("setOutputFile() first");
		if (audioSource >= 0)
			System.out.println("MediaRecorder: no audio capture under ATL, recording video only");

		prepared = true;
		/* audio-only recording has nothing to encode: it stays the no-op it
		 * has always been under ATL rather than writing an empty video file */
		if (videoSource < 0) {
			System.out.println("MediaRecorder: no video source, nothing will be recorded");
			return;
		}

		surface = new Surface();
		nativePtr = native_prepare(videoWidth, videoHeight, videoFrameRate, videoBitRate,
		    outputPath, surface);
		if (nativePtr == 0) {
			surface = null;
			throw new IOException("cannot record " + videoWidth + "x" + videoHeight +
			    " video to " + outputPath);
		}
	}

	/** The recording input; valid between prepare() and release(). */
	public Surface getSurface() {
		if (nativePtr == 0)
			throw new IllegalStateException("prepare() before getSurface()");
		return surface;
	}

	public void start() {
		if (!prepared)
			throw new IllegalStateException("prepare() before start()");
		if (nativePtr != 0 && !native_start(nativePtr))
			throw new IllegalStateException("the encoder refused to start");
		recording = true;
	}

	public void stop() {
		if (!recording)
			throw new IllegalStateException("this MediaRecorder is not recording");
		recording = false;
		if (nativePtr != 0)
			native_stop(nativePtr);
	}

	public void resume() {}

	public void pause() {}

	public void reset() {
		release();
		audioSource = -1;
		videoSource = -1;
		outputPath = null;
		prepared = false;
	}

	public void release() {
		if (nativePtr != 0) {
			if (recording)
				native_stop(nativePtr);
			native_release(nativePtr);
			nativePtr = 0;
		}
		recording = false;
		prepared = false;
		if (surface != null) {
			surface.release();
			surface = null;
		}
	}

	/** Frames the encoder took; the recording's only in-process observable. */
	public long getRecordedFrameCount() {
		return nativePtr == 0 ? 0 : native_frameCount(nativePtr);
	}

	private static native long native_prepare(int width, int height, int fps, int bitRate,
	    String path, Surface surface);
	private static native boolean native_start(long ptr);
	private static native void native_stop(long ptr);
	private static native long native_frameCount(long ptr);
	private static native void native_release(long ptr);
}
