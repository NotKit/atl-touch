package android.hardware;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;

public class Camera {

	public static final int CAMERA_ERROR_UNKNOWN = 1;
	public static final int CAMERA_ERROR_EVICTED = 2;
	public static final int CAMERA_ERROR_SERVER_DIED = 100;

	public interface PreviewCallback {
		void onPreviewFrame(byte[] data, Camera camera);
	}

	public interface AutoFocusCallback {
		void onAutoFocus(boolean success, Camera camera);
	}

	public interface ShutterCallback {
		void onShutter();
	}

	public interface PictureCallback {
		void onPictureTaken(byte[] data, Camera camera);
	}

	public interface ErrorCallback {
		void onError(int error, Camera camera);
	}

	public static class CameraInfo {
		public static final int CAMERA_FACING_BACK = 0;
		public static final int CAMERA_FACING_FRONT = 1;
		public int facing;
		public int orientation;
		public boolean canDisableShutterSound = true;
	}

	public static class Size {
		public int width;
		public int height;

		public Size(int w, int h) {
			width = w;
			height = h;
		}

		@Override
		public boolean equals(Object obj) {
			if (!(obj instanceof Size))
				return false;
			Size s = (Size)obj;
			return width == s.width && height == s.height;
		}

		@Override
		public int hashCode() {
			return width * 32713 + height;
		}
	}

	/**
	 * AOSP-style string key/value map. getParameters() populates it from the
	 * native defaults; setParameters() pushes the backend-relevant keys down.
	 */
	public class Parameters {
		public static final String FOCUS_MODE_AUTO = "auto";
		public static final String FOCUS_MODE_INFINITY = "infinity";
		public static final String FOCUS_MODE_MACRO = "macro";
		public static final String FOCUS_MODE_FIXED = "fixed";
		public static final String FOCUS_MODE_EDOF = "edof";
		public static final String FOCUS_MODE_CONTINUOUS_VIDEO = "continuous-video";
		public static final String FOCUS_MODE_CONTINUOUS_PICTURE = "continuous-picture";

		public static final String FLASH_MODE_OFF = "off";
		public static final String FLASH_MODE_AUTO = "auto";
		public static final String FLASH_MODE_ON = "on";
		public static final String FLASH_MODE_RED_EYE = "red-eye";
		public static final String FLASH_MODE_TORCH = "torch";

		public static final int PREVIEW_FPS_MIN_INDEX = 0;
		public static final int PREVIEW_FPS_MAX_INDEX = 1;

		private final LinkedHashMap<String, String> map = new LinkedHashMap<String, String>();

		private Parameters() {}

		public String flatten() {
			StringBuilder flattened = new StringBuilder(128);
			for (String key : map.keySet()) {
				flattened.append(key);
				flattened.append('=');
				flattened.append(map.get(key));
				flattened.append(';');
			}
			if (flattened.length() > 0)  // chop the trailing semicolon, AOSP-style
				flattened.deleteCharAt(flattened.length() - 1);
			return flattened.toString();
		}

		public void unflatten(String flattened) {
			map.clear();
			if (flattened == null)
				return;
			for (String kv : flattened.split(";")) {
				int pos = kv.indexOf('=');
				if (pos == -1)
					continue;
				map.put(kv.substring(0, pos), kv.substring(pos + 1));
			}
		}

		public void set(String key, String value) {
			map.put(key, value);
		}

		public void set(String key, int value) {
			map.put(key, Integer.toString(value));
		}

		public String get(String key) {
			return map.get(key);
		}

		public int getInt(String key) {
			return Integer.parseInt(map.get(key));
		}

		public void remove(String key) {
			map.remove(key);
		}

		private int getInt(String key, int defaultValue) {
			try {
				return Integer.parseInt(map.get(key));
			} catch (NumberFormatException e) {
				return defaultValue;
			}
		}

		/* --- preview / picture size --- */

		public void setPreviewSize(int width, int height) {
			set("preview-size", width + "x" + height);
		}

		public Size getPreviewSize() {
			return strToSize(get("preview-size"));
		}

		public List<Size> getSupportedPreviewSizes() {
			return splitSizes(get("preview-size-values"));
		}

		public void setPictureSize(int width, int height) {
			set("picture-size", width + "x" + height);
		}

		public Size getPictureSize() {
			return strToSize(get("picture-size"));
		}

		public List<Size> getSupportedPictureSizes() {
			return splitSizes(get("picture-size-values"));
		}

		/* --- formats (android.graphics.ImageFormat values) --- */

		public void setPreviewFormat(int pixel_format) {
			String s = cameraFormatForPixelFormat(pixel_format);
			if (s == null)
				throw new IllegalArgumentException("Invalid pixel_format=" + pixel_format);
			set("preview-format", s);
		}

		public int getPreviewFormat() {
			return pixelFormatForCameraFormat(get("preview-format"));
		}

		public List<Integer> getSupportedPreviewFormats() {
			String str = get("preview-format-values");
			if (str == null)
				return null;
			ArrayList<Integer> formats = new ArrayList<Integer>();
			for (String s : str.split(",")) {
				int f = pixelFormatForCameraFormat(s);
				if (f != 0)
					formats.add(f);
			}
			return formats;
		}

		public void setPictureFormat(int pixel_format) {
			String s = cameraFormatForPixelFormat(pixel_format);
			if (s == null)
				throw new IllegalArgumentException("Invalid pixel_format=" + pixel_format);
			set("picture-format", s);
		}

		public int getPictureFormat() {
			return pixelFormatForCameraFormat(get("picture-format"));
		}

		public List<Integer> getSupportedPictureFormats() {
			String str = get("picture-format-values");
			if (str == null)
				return null;
			ArrayList<Integer> formats = new ArrayList<Integer>();
			for (String s : str.split(",")) {
				int f = pixelFormatForCameraFormat(s);
				if (f != 0)
					formats.add(f);
			}
			return formats;
		}

		/* --- fps --- */

		public void setPreviewFpsRange(int min, int max) {
			set("preview-fps-range", min + "," + max);
		}

		public void getPreviewFpsRange(int[] range) {
			if (range == null || range.length != 2)
				throw new IllegalArgumentException("range must be an array with two elements.");
			splitInt(get("preview-fps-range"), range);
		}

		public List<int[]> getSupportedPreviewFpsRange() {
			return splitRange(get("preview-fps-range-values"));
		}

		/** @deprecated legacy single-value fps API, still probed by some apps */
		public void setPreviewFrameRate(int fps) {
			set("preview-frame-rate", fps);
		}

		public int getPreviewFrameRate() {
			return getInt("preview-frame-rate", 0);
		}

		public List<Integer> getSupportedPreviewFrameRates() {
			String str = get("preview-frame-rate-values");
			if (str == null)
				return null;
			ArrayList<Integer> rates = new ArrayList<Integer>();
			for (String s : str.split(","))
				rates.add(Integer.parseInt(s));
			return rates;
		}

		/* --- focus / flash --- */

		public void setFocusMode(String value) {
			set("focus-mode", value);
		}

		public String getFocusMode() {
			return get("focus-mode");
		}

		public List<String> getSupportedFocusModes() {
			return splitStrings(get("focus-mode-values"));
		}

		public void setFlashMode(String value) {
			set("flash-mode", value);
		}

		public String getFlashMode() {
			return get("flash-mode");
		}

		public List<String> getSupportedFlashModes() {
			return splitStrings(get("flash-mode-values"));
		}

		/* --- zoom --- */

		public boolean isZoomSupported() {
			return "true".equals(get("zoom-supported"));
		}

		public int getMaxZoom() {
			return getInt("max-zoom", 0);
		}

		public void setZoom(int value) {
			set("zoom", value);
		}

		public int getZoom() {
			return getInt("zoom", 0);
		}

		/* --- jpeg --- */

		public void setRotation(int rotation) {
			if (rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270)
				set("rotation", rotation);
			else
				throw new IllegalArgumentException("Invalid rotation=" + rotation);
		}

		public void setJpegQuality(int quality) {
			set("jpeg-quality", quality);
		}

		public int getJpegQuality() {
			return getInt("jpeg-quality", 85);
		}

		/* --- parsing helpers --- */

		private Size strToSize(String str) {
			if (str == null)
				return null;
			int pos = str.indexOf('x');
			if (pos == -1)
				return null;
			return new Size(Integer.parseInt(str.substring(0, pos)),
					Integer.parseInt(str.substring(pos + 1)));
		}

		private List<Size> splitSizes(String str) {
			if (str == null)
				return null;
			ArrayList<Size> sizes = new ArrayList<Size>();
			for (String s : str.split(",")) {
				Size size = strToSize(s);
				if (size != null)
					sizes.add(size);
			}
			return sizes;
		}

		private List<String> splitStrings(String str) {
			if (str == null)
				return null;
			ArrayList<String> strings = new ArrayList<String>();
			for (String s : str.split(","))
				strings.add(s);
			return strings;
		}

		private void splitInt(String str, int[] output) {
			if (str == null)
				return;
			String[] parts = str.split(",");
			for (int i = 0; i < parts.length && i < output.length; i++)
				output[i] = Integer.parseInt(parts[i]);
		}

		/* "(10000,26623),(30000,30000)" -> list of int[2] */
		private List<int[]> splitRange(String str) {
			if (str == null || str.length() < 2 || str.charAt(0) != '('
					|| str.charAt(str.length() - 1) != ')')
				return null;
			ArrayList<int[]> ranges = new ArrayList<int[]>();
			int fromIndex = 1;
			while (true) {
				int endIndex = str.indexOf("),(", fromIndex);
				if (endIndex == -1)
					endIndex = str.length() - 1;
				int[] range = new int[2];
				splitInt(str.substring(fromIndex, endIndex), range);
				ranges.add(range);
				if (endIndex == str.length() - 1)
					break;
				fromIndex = endIndex + 3;
			}
			return ranges;
		}
	}

	/* AOSP mapping between ImageFormat ints and Parameters format strings */
	private static String cameraFormatForPixelFormat(int format) {
		switch (format) {
			case 4: return "rgb565";              /* ImageFormat.RGB_565 */
			case 16: return "yuv422sp";           /* ImageFormat.NV16 */
			case 17: return "yuv420sp";           /* ImageFormat.NV21 */
			case 20: return "yuv422i-yuyv";       /* ImageFormat.YUY2 */
			case 256: return "jpeg";              /* ImageFormat.JPEG */
			case 0x32315659: return "yuv420p";    /* ImageFormat.YV12 */
			default: return null;
		}
	}

	private static int pixelFormatForCameraFormat(String format) {
		if (format == null)
			return 0; /* ImageFormat.UNKNOWN */
		if (format.equals("rgb565"))
			return 4;
		if (format.equals("yuv422sp"))
			return 16;
		if (format.equals("yuv420sp"))
			return 17;
		if (format.equals("yuv422i-yuyv"))
			return 20;
		if (format.equals("jpeg"))
			return 256;
		if (format.equals("yuv420p"))
			return 0x32315659;
		return 0;
	}

	private long nativePtr;
	private int cameraId = -1;
	private ErrorCallback errorCallback;
	/* authoritative parameter state; getParameters() hands out copies */
	private String parametersFlattened;
	private int displayOrientation = 0;

	private Camera(int cameraId) {
		this.cameraId = cameraId;
		this.nativePtr = native_open(cameraId);
		if (this.nativePtr != 0)
			this.parametersFlattened = native_getDefaultParameters(nativePtr);
	}

	public static int getNumberOfCameras() {
		return native_getNumberOfCameras();
	}

	public static void getCameraInfo(int cameraId, CameraInfo cameraInfo) {
		native_getCameraInfo(cameraId, cameraInfo);
	}

	public static Camera open(int cameraId) {
		Camera camera = new Camera(cameraId);
		if (camera.nativePtr == 0)
			throw new RuntimeException("Fail to connect to camera service");
		return camera;
	}

	public static Camera open() {
		int count = getNumberOfCameras();
		CameraInfo info = new CameraInfo();
		for (int i = 0; i < count; i++) {
			getCameraInfo(i, info);
			if (info.facing == CameraInfo.CAMERA_FACING_BACK)
				return open(i);
		}
		return null;
	}

	public final void release() {
		if (nativePtr != 0) {
			native_release(nativePtr);
			nativePtr = 0;
		}
	}

	public void setErrorCallback(ErrorCallback callback) {
		errorCallback = callback;
	}

	public Parameters getParameters() {
		Parameters params = new Parameters();
		params.unflatten(parametersFlattened);
		return params;
	}

	public void setParameters(Parameters params) {
		Size previewSize = params.getPreviewSize();
		int format = params.getPreviewFormat();
		int[] fpsRange = new int[2];
		if (params.get("preview-fps-range") != null)
			params.getPreviewFpsRange(fpsRange);
		if (nativePtr == 0 || previewSize == null ||
				!native_setParameters(nativePtr, previewSize.width, previewSize.height,
						format, fpsRange[0], fpsRange[1]))
			throw new RuntimeException("setParameters failed");
		parametersFlattened = params.flatten();
	}

	public final void setDisplayOrientation(int degrees) {
		displayOrientation = degrees;
		if (nativePtr != 0)
			native_setDisplayOrientation(nativePtr, degrees);
	}

	public final void startPreview() {
		if (nativePtr != 0)
			native_startPreview(nativePtr);
	}

	public final void stopPreview() {
		if (nativePtr != 0)
			native_stopPreview(nativePtr);
	}

	private static native int native_getNumberOfCameras();
	private static native void native_getCameraInfo(int cameraId, CameraInfo cameraInfo);
	private native long native_open(int cameraId);
	private native void native_release(long nativePtr);
	private native void native_startPreview(long nativePtr);
	private native void native_stopPreview(long nativePtr);
	private native String native_getDefaultParameters(long nativePtr);
	private native boolean native_setParameters(long nativePtr, int width, int height,
			int format, int fpsMin, int fpsMax);
	private native void native_setDisplayOrientation(long nativePtr, int degrees);
}
