package android.media;

public class MediaCodecInfo {

	private String name;
	private String mime;

	public MediaCodecInfo(String name, String mime) {
		this.name = name;
		this.mime = mime;
	}

	public String getName() {
		return name;
	}

	public boolean isEncoder() {
		return false;
	}

	public String[] getSupportedTypes() {
		return new String[] {mime};
	}

	public CodecCapabilities getCapabilitiesForType(String type) {
		return new CodecCapabilities();
	}

	public static class CodecCapabilities {

		/* Empty, not null: callers iterate these without a null check, and an
		 * exception thrown inside a JNI callback can abort the process rather
		 * than propagate -- webrtc's codec enumeration does exactly that. There
		 * is no profile or colour format information to give: MediaCodec here
		 * decodes through ffmpeg and exposes no buffer formats. */
		public CodecProfileLevel[] profileLevels = new CodecProfileLevel[0];

		public boolean isFeatureSupported(String feature) {
			System.out.println("CodecCapabilities.isFeatureSupported(" + feature + ")");
			return false;
		}

		public boolean isFeatureRequired(String feature) {
			System.out.println("CodecCapabilities.isFeatureRequired(" + feature + ")");
			return false;
		}

		public AudioCapabilities getAudioCapabilities() {
			return new AudioCapabilities();
		}
	
	public android.media.MediaCodecInfo.VideoCapabilities getVideoCapabilities() { return null; }

	public int[] colorFormats = new int[0];

	public static final int COLOR_FormatSurface = 2130708361;

	public static final int COLOR_FormatYUV420Planar = 19;

	public static final int COLOR_FormatYUV420SemiPlanar = 21;

	public static final int COLOR_QCOM_FormatYUV420SemiPlanar = 2141391872;

	public static final java.lang.String FEATURE_AdaptivePlayback = "adaptive-playback";

	public static final java.lang.String FEATURE_EncodingStatistics = "encoding-statistics";

	public static final java.lang.String FEATURE_TunneledPlayback = "tunneled-playback";
}

	public static class CodecProfileLevel {
	public int level;

	public int profile;

	public static final int AV1ProfileMain10 = 2;

	public static final int AV1ProfileMain10HDR10 = 4096;

	public static final int AV1ProfileMain10HDR10Plus = 8192;

	public static final int AVCProfileHigh10 = 16;

	public static final int HEVCProfileMain10 = 2;

	public static final int HEVCProfileMain10HDR10 = 4096;

	public static final int HEVCProfileMain10HDR10Plus = 8192;

	public static final int VP9Profile2 = 4;

	public static final int VP9Profile2HDR = 4096;

	public static final int VP9Profile2HDR10Plus = 16384;

	public static final int VP9Profile3 = 8;

	public static final int VP9Profile3HDR = 8192;

	public static final int VP9Profile3HDR10Plus = 32768;

	public static final int AVCLevel3 = 256;

	public static final int AVCProfileHigh = 8;
}

	public static class AudioCapabilities {

		public boolean isSampleRateSupported(int sampleRate) {
			return true;
		}

		public int getMaxInputChannelCount() {
			return 2;
		}
	}

	public boolean isHardwareAccelerated() { return false; }

	public boolean isSoftwareOnly() { return false; }

	public static class EncoderCapabilities { 
	public static final int BITRATE_MODE_CBR = 2;
}

	public static class VideoCapabilities { 
	public boolean isSizeSupported(int a0, int a1) { return false; }
}
}
