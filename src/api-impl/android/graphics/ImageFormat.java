package android.graphics;

/** The format constants ImageReader, Camera and camera2 stream configurations speak in. */
public class ImageFormat {

	public static final int UNKNOWN = 0;
	public static final int RGB_565 = 4;
	public static final int YV12 = 0x32315659;
	public static final int Y8 = 0x20203859;
	public static final int Y16 = 0x20363159;
	public static final int YCBCR_P010 = 0x36;
	public static final int NV16 = 16;
	public static final int NV21 = 17;
	public static final int YUY2 = 20;
	public static final int JPEG = 256;
	public static final int DEPTH_JPEG = 0x69656963;
	public static final int YUV_420_888 = 0x23;
	public static final int YUV_422_888 = 0x27;
	public static final int YUV_444_888 = 0x28;
	public static final int FLEX_RGB_888 = 0x29;
	public static final int FLEX_RGBA_8888 = 0x2A;
	public static final int RAW_SENSOR = 0x20;
	public static final int RAW_PRIVATE = 0x24;
	public static final int RAW10 = 0x25;
	public static final int RAW12 = 0x26;
	public static final int DEPTH16 = 0x44363159;
	public static final int DEPTH_POINT_CLOUD = 0x101;
	public static final int PRIVATE = 0x22;
	public static final int HEIC = 0x48454946;
	public static final int HEIC_ULTRAHDR = 4102;
	public static final int JPEG_R = 4101;
	public static final int RAW14 = 44;
	public static final int YCBCR_P210 = 60;

	public static int getBitsPerPixel(int format) {
		switch (format) {
		case RGB_565:
		case YUY2:
		case Y16:
		case YCBCR_P010:
		case NV16:
		case RAW_SENSOR:
		case DEPTH16:
			return 16;
		case NV21:
		case YV12:
		case YUV_420_888:
			return 12;
		case Y8:
			return 8;
		case RAW10:
			return 10;
		case RAW12:
			return 12;
		case YUV_422_888:
			return 16;
		case YUV_444_888:
		case FLEX_RGB_888:
			return 24;
		case FLEX_RGBA_8888:
			return 32;
		default:
			return -1;
		}
	}

	/** True for the formats whose buffers are opaque to the app. */
	public static boolean isPublicFormat(int format) {
		return format != PRIVATE && format != UNKNOWN;
	}
}
