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
		CodecCapabilities caps = new CodecCapabilities();
		if ("video/avc".equals(type)) {
			caps.profileLevels = new CodecProfileLevel[] {
				new CodecProfileLevel(CodecProfileLevel.AVCProfileBaseline, CodecProfileLevel.AVCLevel61),
				new CodecProfileLevel(CodecProfileLevel.AVCProfileMain, CodecProfileLevel.AVCLevel61)
			};
		}
		return caps;
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
	
	public android.media.MediaCodecInfo.VideoCapabilities getVideoCapabilities() { return new VideoCapabilities(); }

	public int[] colorFormats = new int[0];

	public static final int COLOR_FormatSurface = 2130708361;

	public static final int COLOR_FormatYUV420Planar = 19;

	public static final int COLOR_FormatYUV420SemiPlanar = 21;

	public static final int COLOR_QCOM_FormatYUV420SemiPlanar = 2141391872;

	public static final java.lang.String FEATURE_AdaptivePlayback = "adaptive-playback";

	public static final java.lang.String FEATURE_EncodingStatistics = "encoding-statistics";

	public static final java.lang.String FEATURE_TunneledPlayback = "tunneled-playback";

		public int getMaxSupportedInstances() {
			return 0;
		}
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

		public CodecProfileLevel() {}

		public CodecProfileLevel(int profile, int level) {
			this.profile = profile;
			this.level = level;
		}

		public static final int AVCProfileBaseline = 0x01;

		public static final int AVCProfileMain = 0x02;

		public static final int AVCProfileExtended = 0x04;

		public static final int AVCProfileHigh422 = 0x20;

		public static final int AVCProfileHigh444 = 0x40;

		public static final int AVCLevel1 = 0x01;

		public static final int AVCLevel1b = 0x02;

		public static final int AVCLevel11 = 0x04;

		public static final int AVCLevel12 = 0x08;

		public static final int AVCLevel13 = 0x10;

		public static final int AVCLevel2 = 0x20;

		public static final int AVCLevel21 = 0x40;

		public static final int AVCLevel22 = 0x80;

		public static final int AVCLevel31 = 0x200;

		public static final int AVCLevel32 = 0x400;

		public static final int AVCLevel4 = 0x800;

		public static final int AVCLevel41 = 0x1000;

		public static final int AVCLevel42 = 0x2000;

		public static final int AVCLevel5 = 0x4000;

		public static final int AVCLevel51 = 0x8000;

		public static final int AVCLevel52 = 0x10000;

		public static final int AVCLevel6 = 0x20000;

		public static final int AVCLevel61 = 0x40000;

		public static final int AVCLevel62 = 0x80000;

		public static final int VP9Profile0 = 0x01;

		public static final int VP9Profile1 = 0x02;

		public static final int VP9Level1 = 0x1;

		public static final int VP9Level11 = 0x2;

		public static final int VP9Level2 = 0x4;

		public static final int VP9Level21 = 0x8;

		public static final int VP9Level3 = 0x10;

		public static final int VP9Level31 = 0x20;

		public static final int VP9Level4 = 0x40;

		public static final int VP9Level41 = 0x80;

		public static final int VP9Level5 = 0x100;

		public static final int VP9Level51 = 0x200;

		public static final int VP9Level52 = 0x400;

		public static final int VP9Level6 = 0x800;

		public static final int VP9Level61 = 0x1000;

		public static final int VP9Level62 = 0x2000;

		public static final int VP8ProfileMain = 0x01;

		public static final int VP8LevelMain = 0x01;

		public static final int HEVCProfileMain = 0x01;

		public static final int HEVCProfileMainStill = 0x04;

		public static final int HEVCMainTierLevel1 = 0x1;

		public static final int HEVCMainTierLevel2 = 0x2;

		public static final int HEVCMainTierLevel21 = 0x4;

		public static final int HEVCMainTierLevel3 = 0x8;

		public static final int HEVCMainTierLevel31 = 0x10;

		public static final int HEVCMainTierLevel4 = 0x20;

		public static final int HEVCMainTierLevel41 = 0x40;

		public static final int HEVCMainTierLevel5 = 0x80;

		public static final int HEVCMainTierLevel51 = 0x100;

		public static final int HEVCMainTierLevel52 = 0x200;

		public static final int HEVCMainTierLevel6 = 0x400;

		public static final int HEVCMainTierLevel61 = 0x800;

		public static final int HEVCMainTierLevel62 = 0x1000;

		public static final int HEVCHighTierLevel1 = 0x1;

		public static final int HEVCHighTierLevel2 = 0x2;

		public static final int HEVCHighTierLevel21 = 0x4;

		public static final int HEVCHighTierLevel3 = 0x8;

		public static final int HEVCHighTierLevel31 = 0x10;

		public static final int HEVCHighTierLevel4 = 0x20;

		public static final int HEVCHighTierLevel41 = 0x40;

		public static final int HEVCHighTierLevel5 = 0x80;

		public static final int HEVCHighTierLevel51 = 0x100;

		public static final int HEVCHighTierLevel52 = 0x200;

		public static final int HEVCHighTierLevel6 = 0x400;

		public static final int HEVCHighTierLevel61 = 0x800;

		public static final int HEVCHighTierLevel62 = 0x1000;

		public static final int AACObjectMain = 1;

		public static final int AACObjectLC = 2;

		public static final int AACObjectSSR = 3;

		public static final int AACObjectLTP = 4;

		public static final int AACObjectHE = 5;

		public static final int AACObjectScalable = 6;

		public static final int AACObjectERLC = 17;

		public static final int AACObjectERScalable = 20;

		public static final int AACObjectLD = 23;

		public static final int AACObjectHE_PS = 29;

		public static final int AACObjectELD = 39;

		public static final int AACObjectXHE = 42;
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

		public int getWidthAlignment() {
			return 1;
		}

		public int getHeightAlignment() {
			return 1;
		}

		public boolean areSizeAndRateSupported(int width, int height, double frameRate) {
			return width > 0 && height > 0 && frameRate > 0;
		}
}
}
