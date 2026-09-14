package android.hardware.camera2;

import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.camera2.impl.CameraMetadataNative;
import android.hardware.camera2.params.ColorSpaceTransform;
import android.hardware.camera2.params.Face;
import android.hardware.camera2.params.LensShadingMap;
import android.hardware.camera2.params.MeteringRectangle;
import android.hardware.camera2.params.OisSample;
import android.hardware.camera2.params.RggbChannelVector;
import android.util.Pair;
import android.util.Range;
import android.util.Rational;
import android.util.Size;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * The metadata of one delivered frame: what the camera did with the request
 * plus what its sensor read while doing it.
 *
 * The keys a result actually carries are the ones the backend put in it, so
 * getKeys() reports the frame's own entries rather than a fixed list.
 */
public class CaptureResult extends CameraMetadata<CaptureResult.Key<?>> {

	public static final class Key<T> {
		private final String name;
		private final Class<T> type;

		public Key(String name, Class<T> type) {
			if (name == null)
				throw new NullPointerException("key name must not be null");
			this.name = name;
			this.type = type;
		}

		public String getName() {
			return name;
		}

		Class<T> getType() {
			return type;
		}

		@Override
		public boolean equals(Object other) {
			return other instanceof Key && ((Key<?>)other).name.equals(name);
		}

		@Override
		public int hashCode() {
			return name.hashCode();
		}

		@Override
		public String toString() {
			return "CaptureResult.Key(" + name + ")";
		}
	}

	private final CameraMetadataNative results;
	private final CaptureRequest request;
	private final long frameNumber;
	private final int sequenceId;

	CaptureResult(CameraMetadataNative results, CaptureRequest request, long frameNumber, int sequenceId) {
		this.results = results;
		this.request = request;
		this.frameNumber = frameNumber;
		this.sequenceId = sequenceId;
	}

	@SuppressWarnings("unchecked")
	public <T> T get(Key<T> key) {
		return (T)results.get(key.getName(), key.getType());
	}

	@Override
	public List<Key<?>> getKeys() {
		List<Key<?>> list = new ArrayList<Key<?>>();

		for (int tag : results.getTags()) {
			String name = results.getTagName(tag);

			if (name != null)
				list.add(new Key<Object>(name, Object.class));
		}
		return Collections.unmodifiableList(list);
	}

	public CaptureRequest getRequest() {
		return request;
	}

	/**
	 * The bag behind this result, for the camera2 NDK bridge
	 * (ACameraMetadata_fromCameraMetadata). Google Camera hands its native
	 * HDR+ pipeline the Java result of every payload frame and reads it back
	 * through the NDK, so a result that cannot be converted is a shot with no
	 * frames in it.
	 */
	public CameraMetadataNative getAtlNativeMetadata() {
		return results;
	}

	/* the frame's own metadata, for a reprocess request built from it */
	CameraMetadataNative copySettings() {
		return results.copy();
	}

	public long getFrameNumber() {
		return frameNumber;
	}

	public int getSequenceId() {
		return sequenceId;
	}

	@Override
	public String toString() {
		return "CaptureResult(frame " + frameNumber + ")";
	}

	public static final Key<Long> SENSOR_TIMESTAMP =
	    new Key<Long>("android.sensor.timestamp", Long.class);
	public static final Key<Long> SENSOR_EXPOSURE_TIME =
	    new Key<Long>("android.sensor.exposureTime", Long.class);
	public static final Key<Long> SENSOR_FRAME_DURATION =
	    new Key<Long>("android.sensor.frameDuration", Long.class);
	public static final Key<Integer> SENSOR_SENSITIVITY =
	    new Key<Integer>("android.sensor.sensitivity", Integer.class);

	public static final Key<Integer> CONTROL_MODE =
	    new Key<Integer>("android.control.mode", Integer.class);
	public static final Key<Integer> CONTROL_CAPTURE_INTENT =
	    new Key<Integer>("android.control.captureIntent", Integer.class);
	public static final Key<Integer> CONTROL_AE_MODE =
	    new Key<Integer>("android.control.aeMode", Integer.class);
	public static final Key<Integer> CONTROL_AE_STATE =
	    new Key<Integer>("android.control.aeState", Integer.class);
	public static final Key<Boolean> CONTROL_AE_LOCK =
	    new Key<Boolean>("android.control.aeLock", Boolean.class);
	public static final Key<Range<Integer>> CONTROL_AE_TARGET_FPS_RANGE =
	    new Key("android.control.aeTargetFpsRange", Range.class);
	public static final Key<Integer> CONTROL_AF_MODE =
	    new Key<Integer>("android.control.afMode", Integer.class);
	public static final Key<Integer> CONTROL_AF_STATE =
	    new Key<Integer>("android.control.afState", Integer.class);
	public static final Key<Integer> CONTROL_AWB_MODE =
	    new Key<Integer>("android.control.awbMode", Integer.class);
	public static final Key<Integer> CONTROL_AWB_STATE =
	    new Key<Integer>("android.control.awbState", Integer.class);
	public static final Key<Boolean> CONTROL_AWB_LOCK =
	    new Key<Boolean>("android.control.awbLock", Boolean.class);
	public static final Key<Integer> CONTROL_EFFECT_MODE =
	    new Key<Integer>("android.control.effectMode", Integer.class);
	public static final Key<Integer> CONTROL_SCENE_MODE =
	    new Key<Integer>("android.control.sceneMode", Integer.class);
	public static final Key<Float> CONTROL_ZOOM_RATIO =
	    new Key<Float>("android.control.zoomRatio", Float.class);

	public static final Key<Integer> FLASH_MODE =
	    new Key<Integer>("android.flash.mode", Integer.class);
	public static final Key<Integer> FLASH_STATE =
	    new Key<Integer>("android.flash.state", Integer.class);

	public static final Key<Float> LENS_FOCAL_LENGTH =
	    new Key<Float>("android.lens.focalLength", Float.class);
	public static final Key<Float> LENS_FOCUS_DISTANCE =
	    new Key<Float>("android.lens.focusDistance", Float.class);
	public static final Key<Float> LENS_APERTURE =
	    new Key<Float>("android.lens.aperture", Float.class);
	public static final Key<Integer> LENS_STATE =
	    new Key<Integer>("android.lens.state", Integer.class);

	public static final Key<Rect> SCALER_CROP_REGION =
	    new Key<Rect>("android.scaler.cropRegion", Rect.class);

	public static final Key<Integer> JPEG_ORIENTATION =
	    new Key<Integer>("android.jpeg.orientation", Integer.class);
	public static final Key<Byte> JPEG_QUALITY =
	    new Key<Byte>("android.jpeg.quality", Byte.class);
	public static final Key<Size> JPEG_THUMBNAIL_SIZE =
	    new Key<Size>("android.jpeg.thumbnailSize", Size.class);

	public static final Key<Integer> STATISTICS_FACE_DETECT_MODE =
	    new Key<Integer>("android.statistics.faceDetectMode", Integer.class);
	public static final Key<Integer> STATISTICS_SCENE_FLICKER =
	    new Key<Integer>("android.statistics.sceneFlicker", Integer.class);
	public static final Key<Integer> NOISE_REDUCTION_MODE =
	    new Key<Integer>("android.noiseReduction.mode", Integer.class);
	public static final Key<Integer> EDGE_MODE =
	    new Key<Integer>("android.edge.mode", Integer.class);
	public static final Key<Integer> TONEMAP_MODE =
	    new Key<Integer>("android.tonemap.mode", Integer.class);
	public static final Key<Integer> COLOR_CORRECTION_MODE =
	    new Key<Integer>("android.colorCorrection.mode", Integer.class);
	public static final Key<ColorSpaceTransform> COLOR_CORRECTION_TRANSFORM =
	    new Key<ColorSpaceTransform>("android.colorCorrection.transform", ColorSpaceTransform.class);
	public static final Key<RggbChannelVector> COLOR_CORRECTION_GAINS =
	    new Key<RggbChannelVector>("android.colorCorrection.gains", RggbChannelVector.class);
	public static final Key<Rational[]> SENSOR_NEUTRAL_COLOR_POINT =
	    new Key<Rational[]>("android.sensor.neutralColorPoint", Rational[].class);

	public static final Key<MeteringRectangle[]> CONTROL_AE_REGIONS =
	    new Key<MeteringRectangle[]>("android.control.aeRegions", MeteringRectangle[].class);
	public static final Key<MeteringRectangle[]> CONTROL_AF_REGIONS =
	    new Key<MeteringRectangle[]>("android.control.afRegions", MeteringRectangle[].class);
	public static final Key<MeteringRectangle[]> CONTROL_AWB_REGIONS =
	    new Key<MeteringRectangle[]>("android.control.awbRegions", MeteringRectangle[].class);

	/* the HAL spreads these over several tags; CameraMetadataNative rebuilds them */
	public static final Key<Face[]> STATISTICS_FACES =
	    new Key<Face[]>("android.statistics.faces", Face[].class);
	public static final Key<OisSample[]> STATISTICS_OIS_SAMPLES =
	    new Key<OisSample[]>("android.statistics.oisSamples", OisSample[].class);
	public static final Key<LensShadingMap> STATISTICS_LENS_SHADING_CORRECTION_MAP =
	    new Key<LensShadingMap>("android.statistics.lensShadingCorrectionMap", LensShadingMap.class);

	/* the result half of the request keys: a result reports back what the
	 * camera did with the setting it was given */
	public static final Key<Integer> CONTROL_AE_EXPOSURE_COMPENSATION =
	    new Key<Integer>("android.control.aeExposureCompensation", Integer.class);
	public static final Key<Integer> CONTROL_AE_PRECAPTURE_TRIGGER =
	    new Key<Integer>("android.control.aePrecaptureTrigger", Integer.class);
	public static final Key<Integer> CONTROL_AF_TRIGGER =
	    new Key<Integer>("android.control.afTrigger", Integer.class);
	public static final Key<Integer> CONTROL_AF_SCENE_CHANGE =
	    new Key<Integer>("android.control.afSceneChange", Integer.class);
	public static final Key<Integer> CONTROL_POST_RAW_SENSITIVITY_BOOST =
	    new Key<Integer>("android.control.postRawSensitivityBoost", Integer.class);
	public static final Key<Integer> COLOR_CORRECTION_ABERRATION_MODE =
	    new Key<Integer>("android.colorCorrection.aberrationMode", Integer.class);
	public static final Key<Boolean> BLACK_LEVEL_LOCK =
	    new Key<Boolean>("android.blackLevel.lock", Boolean.class);

	public static final Key<Integer> LENS_OPTICAL_STABILIZATION_MODE =
	    new Key<Integer>("android.lens.opticalStabilizationMode", Integer.class);
	public static final Key<float[]> LENS_DISTORTION =
	    new Key<float[]>("android.lens.distortion", float[].class);
	public static final Key<float[]> LENS_INTRINSIC_CALIBRATION =
	    new Key<float[]>("android.lens.intrinsicCalibration", float[].class);
	public static final Key<float[]> LENS_POSE_ROTATION =
	    new Key<float[]>("android.lens.poseRotation", float[].class);
	public static final Key<float[]> LENS_POSE_TRANSLATION =
	    new Key<float[]>("android.lens.poseTranslation", float[].class);
	/* (near, far) in dioptres, one tag holding two floats */
	public static final Key<Pair<Float, Float>> LENS_FOCUS_RANGE =
	    new Key("android.lens.focusRange", Pair.class);

	public static final Key<String> LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID =
	    new Key<String>("android.logicalMultiCamera.activePhysicalId", String.class);
	public static final Key<Float> REPROCESS_EFFECTIVE_EXPOSURE_FACTOR =
	    new Key<Float>("android.reprocess.effectiveExposureFactor", Float.class);
	public static final Key<Rect> SCALER_RAW_CROP_REGION =
	    new Key<Rect>("android.scaler.rawCropRegion", Rect.class);

	public static final Key<float[]> SENSOR_DYNAMIC_BLACK_LEVEL =
	    new Key<float[]>("android.sensor.dynamicBlackLevel", float[].class);
	/* one (S, O) pair per CFA channel */
	public static final Key<Pair<Double, Double>[]> SENSOR_NOISE_PROFILE =
	    new Key("android.sensor.noiseProfile", Pair[].class);
	public static final Key<Long> SENSOR_ROLLING_SHUTTER_SKEW =
	    new Key<Long>("android.sensor.rollingShutterSkew", Long.class);
	public static final Key<Integer> SENSOR_PIXEL_MODE =
	    new Key<Integer>("android.sensor.pixelMode", Integer.class);

	public static final Key<Integer> STATISTICS_OIS_DATA_MODE =
	    new Key<Integer>("android.statistics.oisDataMode", Integer.class);
	public static final Key<Boolean> STATISTICS_HOT_PIXEL_MAP_MODE =
	    new Key<Boolean>("android.statistics.hotPixelMapMode", Boolean.class);
	/* the HAL lists the defective pixels as (x, y) int pairs */
	public static final Key<Point[]> STATISTICS_HOT_PIXEL_MAP =
	    new Key<Point[]>("android.statistics.hotPixelMap", Point[].class);

	private static final Map<String, Key<?>> KEYS_BY_NAME = keysByName();

	/** the typed constant for a key name, or null for one ATL has no constant for */
	static Key<?> keyForName(String name) {
		return KEYS_BY_NAME.get(name);
	}

	private static Map<String, Key<?>> keysByName() {
		Map<String, Key<?>> map = new HashMap<String, Key<?>>();

		for (java.lang.reflect.Field field : CaptureResult.class.getDeclaredFields()) {
			if (field.getType() != Key.class)
				continue;
			try {
				Key<?> key = (Key<?>)field.get(null);
				map.put(key.getName(), key);
			} catch (IllegalAccessException e) {
				/* every Key constant is public static final */
			}
		}
		return map;
	}
}
