package android.hardware;

import android.graphics.Rect;
import android.view.Surface;
import android.view.SurfaceHolder;

import java.io.IOException;
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

	public interface AutoFocusMoveCallback {
		void onAutoFocusMoving(boolean start, Camera camera);
	}

	public interface FaceDetectionListener {
		void onFaceDetection(Face[] faces, Camera camera);
	}

	/** Detected face. No backend reports faces, so these are never delivered. */
	public static class Face {
		public Rect rect;
		public int score;
		public int id = -1;
		public android.graphics.Point leftEye;
		public android.graphics.Point rightEye;
		public android.graphics.Point mouth;
	}

	/** A weighted rectangle in the -1000..1000 coordinate space, for focus/metering areas. */
	public static class Area {
		public Rect rect;
		public int weight;

		public Area(Rect rect, int weight) {
			this.rect = rect;
			this.weight = weight;
		}

		@Override
		public boolean equals(Object obj) {
			if (!(obj instanceof Area))
				return false;
			Area a = (Area)obj;
			if (rect == null ? a.rect != null : !rect.equals(a.rect))
				return false;
			return weight == a.weight;
		}
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

		public static final String EFFECT_NONE = "none";
		public static final String EFFECT_MONO = "mono";
		public static final String EFFECT_NEGATIVE = "negative";
		public static final String EFFECT_SOLARIZE = "solarize";
		public static final String EFFECT_SEPIA = "sepia";
		public static final String EFFECT_POSTERIZE = "posterize";
		public static final String EFFECT_WHITEBOARD = "whiteboard";
		public static final String EFFECT_BLACKBOARD = "blackboard";
		public static final String EFFECT_AQUA = "aqua";

		public static final String WHITE_BALANCE_AUTO = "auto";
		public static final String WHITE_BALANCE_INCANDESCENT = "incandescent";
		public static final String WHITE_BALANCE_FLUORESCENT = "fluorescent";
		public static final String WHITE_BALANCE_WARM_FLUORESCENT = "warm-fluorescent";
		public static final String WHITE_BALANCE_DAYLIGHT = "daylight";
		public static final String WHITE_BALANCE_CLOUDY_DAYLIGHT = "cloudy-daylight";
		public static final String WHITE_BALANCE_TWILIGHT = "twilight";
		public static final String WHITE_BALANCE_SHADE = "shade";

		public static final String SCENE_MODE_AUTO = "auto";
		public static final String SCENE_MODE_ACTION = "action";
		public static final String SCENE_MODE_PORTRAIT = "portrait";
		public static final String SCENE_MODE_LANDSCAPE = "landscape";
		public static final String SCENE_MODE_NIGHT = "night";
		public static final String SCENE_MODE_HDR = "hdr";

		public static final String ANTIBANDING_AUTO = "auto";
		public static final String ANTIBANDING_50HZ = "50hz";
		public static final String ANTIBANDING_60HZ = "60hz";
		public static final String ANTIBANDING_OFF = "off";

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

		/* --- image adjustment modes; a null "-values" list means unsupported --- */

		public void setWhiteBalance(String value) {
			set("whitebalance", value);
		}

		public String getWhiteBalance() {
			return get("whitebalance");
		}

		public List<String> getSupportedWhiteBalance() {
			return splitStrings(get("whitebalance-values"));
		}

		public void setColorEffect(String value) {
			set("effect", value);
		}

		public String getColorEffect() {
			return get("effect");
		}

		public List<String> getSupportedColorEffects() {
			return splitStrings(get("effect-values"));
		}

		public void setSceneMode(String value) {
			set("scene-mode", value);
		}

		public String getSceneMode() {
			return get("scene-mode");
		}

		public List<String> getSupportedSceneModes() {
			return splitStrings(get("scene-mode-values"));
		}

		public void setAntibanding(String value) {
			set("antibanding", value);
		}

		public String getAntibanding() {
			return get("antibanding");
		}

		public List<String> getSupportedAntibanding() {
			return splitStrings(get("antibanding-values"));
		}

		/* --- exposure --- */

		public void setExposureCompensation(int value) {
			set("exposure-compensation", value);
		}

		public int getExposureCompensation() {
			return getInt("exposure-compensation", 0);
		}

		public int getMinExposureCompensation() {
			return getInt("min-exposure-compensation", 0);
		}

		public int getMaxExposureCompensation() {
			return getInt("max-exposure-compensation", 0);
		}

		public float getExposureCompensationStep() {
			return getFloat("exposure-compensation-step", 0);
		}

		public boolean isAutoExposureLockSupported() {
			return "true".equals(get("auto-exposure-lock-supported"));
		}

		public void setAutoExposureLock(boolean toggle) {
			set("auto-exposure-lock", toggle ? "true" : "false");
		}

		public boolean getAutoExposureLock() {
			return "true".equals(get("auto-exposure-lock"));
		}

		public boolean isAutoWhiteBalanceLockSupported() {
			return "true".equals(get("auto-whitebalance-lock-supported"));
		}

		public void setAutoWhiteBalanceLock(boolean toggle) {
			set("auto-whitebalance-lock", toggle ? "true" : "false");
		}

		public boolean getAutoWhiteBalanceLock() {
			return "true".equals(get("auto-whitebalance-lock"));
		}

		/* --- focus / metering areas; with a max of 0 the setters are no-ops --- */

		public int getMaxNumFocusAreas() {
			return getInt("max-num-focus-areas", 0);
		}

		public List<Area> getFocusAreas() {
			return splitAreas(get("focus-areas"));
		}

		public void setFocusAreas(List<Area> focusAreas) {
			set("focus-areas", flattenAreas(focusAreas));
		}

		public int getMaxNumMeteringAreas() {
			return getInt("max-num-metering-areas", 0);
		}

		public List<Area> getMeteringAreas() {
			return splitAreas(get("metering-areas"));
		}

		public void setMeteringAreas(List<Area> meteringAreas) {
			set("metering-areas", flattenAreas(meteringAreas));
		}

		/* --- lens / face detection --- */

		public float getHorizontalViewAngle() {
			return getFloat("horizontal-view-angle", 0);
		}

		public float getVerticalViewAngle() {
			return getFloat("vertical-view-angle", 0);
		}

		public int getMaxNumDetectedFaces() {
			return getInt("max-num-detected-faces-hw", 0);
		}

		/* --- video --- */

		public List<Size> getSupportedVideoSizes() {
			return splitSizes(get("video-size-values"));
		}

		public Size getPreferredPreviewSizeForVideo() {
			return strToSize(get("preferred-preview-size-for-video"));
		}

		public boolean isVideoSnapshotSupported() {
			return "true".equals(get("video-snapshot-supported"));
		}

		public boolean isVideoStabilizationSupported() {
			return "true".equals(get("video-stabilization-supported"));
		}

		public void setVideoStabilization(boolean toggle) {
			set("video-stabilization", toggle ? "true" : "false");
		}

		public boolean getVideoStabilization() {
			return "true".equals(get("video-stabilization"));
		}

		public void setRecordingHint(boolean hint) {
			set("recording-hint", hint ? "true" : "false");
		}

		/* --- zoom ratios --- */

		public List<Integer> getZoomRatios() {
			String str = get("zoom-ratios");
			if (str == null)
				return null;
			ArrayList<Integer> ratios = new ArrayList<Integer>();
			for (String s : str.split(","))
				ratios.add(Integer.parseInt(s));
			return ratios;
		}

		/* --- jpeg EXIF geotagging; stored only, no backend consumes it yet --- */

		public void setGpsLatitude(double latitude) {
			set("gps-latitude", Double.toString(latitude));
		}

		public void setGpsLongitude(double longitude) {
			set("gps-longitude", Double.toString(longitude));
		}

		public void setGpsAltitude(double altitude) {
			set("gps-altitude", Double.toString(altitude));
		}

		public void setGpsTimestamp(long timestamp) {
			set("gps-timestamp", Long.toString(timestamp));
		}

		public void setGpsProcessingMethod(String processing_method) {
			set("gps-processing-method", processing_method);
		}

		public void removeGpsData() {
			remove("gps-latitude");
			remove("gps-longitude");
			remove("gps-altitude");
			remove("gps-timestamp");
			remove("gps-processing-method");
		}

		/* --- parsing helpers --- */

		private float getFloat(String key, float defaultValue) {
			try {
				return Float.parseFloat(map.get(key));
			} catch (NumberFormatException e) {
				return defaultValue;
			} catch (NullPointerException e) {
				return defaultValue;
			}
		}

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

		/* "(-10,-10,10,10,1),(...)" -> list of Area */
		private List<Area> splitAreas(String str) {
			if (str == null || str.length() < 2 || str.charAt(0) != '('
					|| str.charAt(str.length() - 1) != ')')
				return null;
			ArrayList<Area> areas = new ArrayList<Area>();
			int fromIndex = 1;
			int[] area = new int[5];
			while (true) {
				int endIndex = str.indexOf("),(", fromIndex);
				if (endIndex == -1)
					endIndex = str.length() - 1;
				splitInt(str.substring(fromIndex, endIndex), area);
				areas.add(new Area(new Rect(area[0], area[1], area[2], area[3]), area[4]));
				if (endIndex == str.length() - 1)
					break;
				fromIndex = endIndex + 3;
			}
			return areas;
		}

		private String flattenAreas(List<Area> areas) {
			if (areas == null || areas.isEmpty())
				return "(0,0,0,0,0)";
			StringBuilder sb = new StringBuilder();
			for (Area a : areas) {
				if (sb.length() > 0)
					sb.append(',');
				sb.append('(').append(a.rect.left).append(',').append(a.rect.top).append(',')
						.append(a.rect.right).append(',').append(a.rect.bottom).append(',')
						.append(a.weight).append(')');
			}
			return sb.toString();
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

	/* preview callback modes, must match camera_callbacks.h */
	private static final int PREVIEW_CB_NONE = 0;
	private static final int PREVIEW_CB_EVERY_FRAME = 1;
	private static final int PREVIEW_CB_ONE_SHOT = 2;
	private static final int PREVIEW_CB_WITH_BUFFER = 3;

	private long nativePtr;
	private int cameraId = -1;
	private ErrorCallback errorCallback;
	/* kept alive here as well as by the native global ref, AOSP-style */
	private PreviewCallback previewCallback;
	/* one-off callbacks, cleared once they have fired */
	private ShutterCallback shutterCallback;
	private PictureCallback rawCallback;
	private PictureCallback postviewCallback;
	private PictureCallback jpegCallback;
	private AutoFocusCallback autoFocusCallback;
	private AutoFocusMoveCallback autoFocusMoveCallback;
	private FaceDetectionListener faceDetectionListener;
	private SurfaceHolder previewHolder;
	private android.graphics.SurfaceTexture previewTexture;
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
			previewHolder = null;
			previewTexture = null;
			previewCallback = null;
			shutterCallback = null;
			rawCallback = null;
			postviewCallback = null;
			jpegCallback = null;
			autoFocusCallback = null;
			autoFocusMoveCallback = null;
			faceDetectionListener = null;
			native_release(nativePtr);
			nativePtr = 0;
		}
	}

	/**
	 * Preview frames are converted to bitmaps and posted through
	 * Surface.postFrame, the same path MediaCodec uses for decoded video.
	 * A null holder (or one without a surface) stops delivery, which is what
	 * apps do from surfaceDestroyed().
	 */
	public final void setPreviewDisplay(SurfaceHolder holder) throws IOException {
		previewHolder = holder;
		Surface surface = holder != null ? holder.getSurface() : null;
		if (nativePtr != 0)
			native_setPreviewSurface(nativePtr, surface);
	}

	/**
	 * Preview frames into a SurfaceTexture, the path TextureView and the
	 * texture-capture side of webrtc use. A null texture stops delivery.
	 */
	public final void setPreviewTexture(android.graphics.SurfaceTexture surfaceTexture) throws IOException {
		previewTexture = surfaceTexture;
		if (nativePtr != 0)
			native_setPreviewTexture(nativePtr, surfaceTexture);
	}

	/**
	 * NV21 preview frames as byte arrays, delivered on the main loop. Each
	 * callback gets a freshly allocated array; setPreviewCallbackWithBuffer()
	 * is the variant that recycles the app's own buffers instead.
	 */
	public final void setPreviewCallback(PreviewCallback cb) {
		setPreviewCallback(cb, cb != null ? PREVIEW_CB_EVERY_FRAME : PREVIEW_CB_NONE);
	}

	/** Like setPreviewCallback(), but the callback fires for one frame only. */
	public final void setOneShotPreviewCallback(PreviewCallback cb) {
		setPreviewCallback(cb, cb != null ? PREVIEW_CB_ONE_SHOT : PREVIEW_CB_NONE);
	}

	/**
	 * Preview frames written into buffers the app supplies through
	 * addCallbackBuffer(). Nothing is delivered while the queue is empty, so
	 * the app has to re-add a buffer once it is done with it.
	 */
	public final void setPreviewCallbackWithBuffer(PreviewCallback cb) {
		setPreviewCallback(cb, cb != null ? PREVIEW_CB_WITH_BUFFER : PREVIEW_CB_NONE);
	}

	private void setPreviewCallback(PreviewCallback cb, int mode) {
		previewCallback = cb;
		if (nativePtr != 0)
			native_setPreviewCallback(nativePtr, cb, mode);
	}

	/** Queues a buffer for setPreviewCallbackWithBuffer(); it must hold a whole frame. */
	public final void addCallbackBuffer(byte[] callbackBuffer) {
		if (callbackBuffer != null && nativePtr != 0)
			native_addCallbackBuffer(nativePtr, callbackBuffer);
	}

	public void setErrorCallback(ErrorCallback callback) {
		errorCallback = callback;
	}

	/**
	 * Continuous-focus move notifications. No backend reports focus movement,
	 * so the callback is stored but never fires.
	 */
	public void setAutoFocusMoveCallback(AutoFocusMoveCallback cb) {
		autoFocusMoveCallback = cb;
	}

	/**
	 * Face detection. getMaxNumDetectedFaces() is 0 on every backend, so
	 * startFaceDetection() is the AOSP error case for an app that ignores it.
	 */
	public void setFaceDetectionListener(FaceDetectionListener listener) {
		faceDetectionListener = listener;
	}

	public final void startFaceDetection() {
		throw new IllegalArgumentException("Face detection is not supported");
	}

	public final void stopFaceDetection() {}

	/**
	 * Shutter sound control. ATL plays no shutter sound, so disabling always
	 * succeeds and enabling is a no-op.
	 */
	public final boolean enableShutterSound(boolean enabled) {
		return true;
	}

	/* Camera ownership is per-process here: nothing else can grab the device. */
	public final void lock() {}

	public final void unlock() {}

	public final void reconnect() throws IOException {
		if (nativePtr == 0)
			throw new IOException("camera is released");
	}

	/**
	 * Captures a picture at the configured picture size. The preview stops as
	 * the frame is grabbed and only startPreview() brings it back, AOSP-style;
	 * the callbacks arrive on the main loop in shutter, raw, postview, jpeg
	 * order. Raw and postview data are always null: no backend produces them.
	 */
	public final void takePicture(ShutterCallback shutter, PictureCallback raw, PictureCallback jpeg) {
		takePicture(shutter, raw, null, jpeg);
	}

	public final void takePicture(ShutterCallback shutter, PictureCallback raw,
			PictureCallback postview, PictureCallback jpeg) {
		Parameters params = getParameters();
		Size pictureSize = params.getPictureSize();
		if (nativePtr == 0 || pictureSize == null)
			throw new RuntimeException("takePicture failed");

		shutterCallback = shutter;
		rawCallback = raw;
		postviewCallback = postview;
		jpegCallback = jpeg;
		if (!native_takePicture(nativePtr, pictureSize.width, pictureSize.height,
				params.getJpegQuality())) {
			shutterCallback = null;
			rawCallback = null;
			postviewCallback = null;
			jpegCallback = null;
			throw new RuntimeException("takePicture failed");
		}
	}

	/** Focus asynchronously; the callback lands on the main loop. */
	public final void autoFocus(AutoFocusCallback cb) {
		autoFocusCallback = cb;
		if (nativePtr != 0)
			native_autoFocus(nativePtr);
	}

	public final void cancelAutoFocus() {
		autoFocusCallback = null;
		if (nativePtr != 0)
			native_cancelAutoFocus(nativePtr);
	}

	/* --- native -> app callbacks, all called on the main loop --- */

	private void dispatchShutter() {
		ShutterCallback cb = shutterCallback;
		shutterCallback = null;
		if (cb != null)
			cb.onShutter();
	}

	private void dispatchPictureTaken(byte[] jpegData) {
		PictureCallback raw = rawCallback;
		PictureCallback postview = postviewCallback;
		PictureCallback jpeg = jpegCallback;
		rawCallback = null;
		postviewCallback = null;
		jpegCallback = null;

		if (raw != null)
			raw.onPictureTaken(null, this);
		if (postview != null)
			postview.onPictureTaken(null, this);
		if (jpeg != null)
			jpeg.onPictureTaken(jpegData, this);
	}

	private void dispatchAutoFocus(boolean success) {
		AutoFocusCallback cb = autoFocusCallback;
		autoFocusCallback = null;
		if (cb != null)
			cb.onAutoFocus(success, this);
	}

	private void dispatchError(int error) {
		ErrorCallback cb = errorCallback;
		if (cb != null)
			cb.onError(error, this);
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
	private native void native_setPreviewSurface(long nativePtr, Surface surface);
	private native void native_setPreviewTexture(long nativePtr, android.graphics.SurfaceTexture surfaceTexture);
	private native void native_setPreviewCallback(long nativePtr, PreviewCallback cb, int mode);
	private native void native_addCallbackBuffer(long nativePtr, byte[] callbackBuffer);
	private native boolean native_takePicture(long nativePtr, int width, int height, int jpegQuality);
	private native void native_autoFocus(long nativePtr);
	private native void native_cancelAutoFocus(long nativePtr);
	private native void native_startPreview(long nativePtr);
	private native void native_stopPreview(long nativePtr);
	private native String native_getDefaultParameters(long nativePtr);
	private native boolean native_setParameters(long nativePtr, int width, int height,
			int format, int fpsMin, int fpsMax);
	private native void native_setDisplayOrientation(long nativePtr, int degrees);
}
