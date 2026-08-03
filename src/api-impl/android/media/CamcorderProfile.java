package android.media;

/**
 * Camcorder presets. ATL has no HAL profile table, so this describes what the
 * software encoders can actually do: 480p and 720p H.264/AAC in MPEG-4.
 */
public class CamcorderProfile {
	public static final int QUALITY_LOW = 0;
	public static final int QUALITY_HIGH = 1;
	public static final int QUALITY_QCIF = 2;
	public static final int QUALITY_CIF = 3;
	public static final int QUALITY_480P = 4;
	public static final int QUALITY_720P = 5;
	public static final int QUALITY_1080P = 6;
	public static final int QUALITY_QVGA = 7;
	public static final int QUALITY_2160P = 8;
	public static final int QUALITY_VGA = 9;
	public static final int QUALITY_4KDCI = 10;
	public static final int QUALITY_QHD = 11;
	public static final int QUALITY_2K = 12;

	public static final int QUALITY_TIME_LAPSE_LOW = 1000;
	public static final int QUALITY_TIME_LAPSE_HIGH = 1001;
	public static final int QUALITY_TIME_LAPSE_QCIF = 1002;
	public static final int QUALITY_TIME_LAPSE_CIF = 1003;
	public static final int QUALITY_TIME_LAPSE_480P = 1004;
	public static final int QUALITY_TIME_LAPSE_720P = 1005;
	public static final int QUALITY_TIME_LAPSE_1080P = 1006;
	public static final int QUALITY_TIME_LAPSE_QVGA = 1007;
	public static final int QUALITY_TIME_LAPSE_2160P = 1008;

	public static final int QUALITY_HIGH_SPEED_LOW = 2000;
	public static final int QUALITY_HIGH_SPEED_HIGH = 2001;
	public static final int QUALITY_HIGH_SPEED_480P = 2002;
	public static final int QUALITY_HIGH_SPEED_720P = 2003;
	public static final int QUALITY_HIGH_SPEED_1080P = 2004;
	public static final int QUALITY_HIGH_SPEED_2160P = 2005;

	public int duration;
	public int quality;
	public int fileFormat = MediaRecorder.OutputFormat.MPEG_4;
	public int videoCodec = MediaRecorder.VideoEncoder.H264;
	public int videoBitRate;
	public int videoFrameRate = 30;
	public int videoFrameWidth;
	public int videoFrameHeight;
	public int audioCodec = MediaRecorder.AudioEncoder.AAC;
	public int audioBitRate = 96000;
	public int audioSampleRate = 44100;
	public int audioChannels = 1;

	private CamcorderProfile(int quality, int width, int height, int videoBitRate) {
		this.quality = quality;
		this.videoFrameWidth = width;
		this.videoFrameHeight = height;
		this.videoBitRate = videoBitRate;
	}

	public static boolean hasProfile(int quality) {
		return hasProfile(0, quality);
	}

	public static boolean hasProfile(int cameraId, int quality) {
		switch (quality) {
			case QUALITY_LOW:
			case QUALITY_HIGH:
			case QUALITY_480P:
			case QUALITY_720P:
				return true;
			default:
				return false;
		}
	}

	public static CamcorderProfile get(int quality) {
		return get(0, quality);
	}

	public static CamcorderProfile get(int cameraId, int quality) {
		switch (quality) {
			case QUALITY_LOW:
			case QUALITY_480P:
				return new CamcorderProfile(quality, 640, 480, 2000000);
			case QUALITY_HIGH:
			case QUALITY_720P:
				return new CamcorderProfile(quality, 1280, 720, 4000000);
			default:
				throw new IllegalArgumentException("Unsupported quality level: " + quality);
		}
	}
}
