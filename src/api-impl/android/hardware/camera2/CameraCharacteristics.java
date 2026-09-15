package android.hardware.camera2;

import android.graphics.Rect;
import android.hardware.camera2.impl.CameraMetadataNative;
import android.hardware.camera2.params.BlackLevelPattern;
import android.hardware.camera2.params.ColorSpaceTransform;
import android.hardware.camera2.params.DeviceStateSensorOrientationMap;
import android.hardware.camera2.params.DynamicRangeProfiles;
import android.hardware.camera2.params.MultiResolutionStreamConfigurationMap;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.util.Range;
import android.util.Rational;
import android.util.Size;
import android.util.SizeF;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * The static properties of one camera, read from the backend's metadata bag.
 *
 * A Key is just a name plus the Java type it marshals to; a key the camera does
 * not report reads back as null, and a key ATL has no constant for (a vendor
 * key, say) is readable by constructing a Key with its name.
 */
public final class CameraCharacteristics extends CameraMetadata<CameraCharacteristics.Key<?>> {

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
			return "CameraCharacteristics.Key(" + name + ")";
		}
	}

	private final String cameraId;
	/* AOSP's name for the bag, because camera apps reach it by reflection */
	private final CameraMetadataNative mProperties;
	private List<Key<?>> keys;

	CameraCharacteristics(String cameraId, CameraMetadataNative metadata) {
		this.cameraId = cameraId;
		this.mProperties = metadata;
	}

	String getId() {
		return cameraId;
	}

	CameraMetadataNative getMetadata() {
		return mProperties;
	}

	/** the bag behind these characteristics, for the camera2 NDK bridge */
	public CameraMetadataNative getAtlNativeMetadata() {
		return mProperties;
	}

	@Override
	CameraMetadataNative getAtlBag() {
		return mProperties;
	}

	@SuppressWarnings("unchecked")
	public <T> T get(Key<T> key) {
		return (T)mProperties.get(key.getName(), key.getType());
	}

	@Override
	public List<Key<?>> getKeys() {
		if (keys == null) {
			List<Key<?>> list = new ArrayList<Key<?>>();

			for (int tag : CameraMetadataNative.getAvailableKeys(cameraId,
			    CameraMetadataNative.KEYS_CHARACTERISTICS)) {
				String name = mProperties.getTagName(tag);
				if (name == null)
					continue; /* a tag nothing names is only reachable by id */
				Key<?> known = KEYS_BY_NAME.get(name);
				list.add(known != null ? known : new Key<Object>(name, Object.class));
			}
			keys = Collections.unmodifiableList(list);
		}
		return keys;
	}

	/**
	 * The request keys this camera acts on, as the backend listed them. A tag
	 * ATL has a typed constant for comes back as that constant, so an app can
	 * compare the list against CaptureRequest.CONTROL_AE_MODE and friends.
	 */
	public List<CaptureRequest.Key<?>> getAvailableCaptureRequestKeys() {
		List<CaptureRequest.Key<?>> list = new ArrayList<CaptureRequest.Key<?>>();

		for (String name : keyNames(CameraMetadataNative.KEYS_REQUEST)) {
			CaptureRequest.Key<?> known = CaptureRequest.keyForName(name);

			list.add(known != null ? known : new CaptureRequest.Key<Object>(name, Object.class));
		}
		return Collections.unmodifiableList(list);
	}

	public List<CaptureResult.Key<?>> getAvailableCaptureResultKeys() {
		List<CaptureResult.Key<?>> list = new ArrayList<CaptureResult.Key<?>>();

		for (String name : keyNames(CameraMetadataNative.KEYS_RESULT)) {
			CaptureResult.Key<?> known = CaptureResult.keyForName(name);

			list.add(known != null ? known : new CaptureResult.Key<Object>(name, Object.class));
		}
		return Collections.unmodifiableList(list);
	}

	/**
	 * ATL configures every session the same way, so no request key is a
	 * session-only one.
	 */
	public List<CaptureRequest.Key<?>> getAvailableSessionKeys() {
		return Collections.emptyList();
	}

	public List<CaptureRequest.Key<?>> getAvailablePhysicalCameraRequestKeys() {
		return Collections.emptyList();
	}

	public List<CameraCharacteristics.Key<?>> getAvailableSessionCharacteristicsKeys() {
		return Collections.emptyList();
	}

	/**
	 * The sub-cameras a logical multi-camera is made of, straight out of the
	 * backend's mProperties: the HAL packs them as NUL-terminated strings in one
	 * byte[]. Each of them is a camera id CameraManager.getCameraCharacteristics
	 * answers for, while getCameraIdList() keeps to the logical cameras, which
	 * is what AOSP does too.
	 */
	public Set<String> getPhysicalCameraIds() {
		int[] capabilities = get(REQUEST_AVAILABLE_CAPABILITIES);

		if (capabilities == null)
			return Collections.emptySet();
		boolean logical = false;
		for (int capability : capabilities)
			logical |= capability == REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA;
		if (!logical)
			return Collections.emptySet();

		byte[] packed = get(LOGICAL_MULTI_CAMERA_PHYSICAL_IDS);
		if (packed == null)
			return Collections.emptySet();

		Set<String> ids = new LinkedHashSet<String>();
		for (String id : new String(packed, StandardCharsets.UTF_8).split("\0")) {
			if (!id.isEmpty())
				ids.add(id);
		}
		return Collections.unmodifiableSet(ids);
	}

	@Override
	public String toString() {
		return "CameraCharacteristics(" + cameraId + ")";
	}

	private List<String> keyNames(int which) {
		List<String> names = new ArrayList<String>();

		for (int tag : CameraMetadataNative.getAvailableKeys(cameraId, which)) {
			String name = mProperties.getTagName(tag);

			if (name != null)
				names.add(name);
		}
		return names;
	}

	public static final Key<Integer> LENS_FACING =
	    new Key<Integer>("android.lens.facing", Integer.class);
	public static final Key<Integer> INFO_SUPPORTED_HARDWARE_LEVEL =
	    new Key<Integer>("android.info.supportedHardwareLevel", Integer.class);

	public static final Key<Rect> SENSOR_INFO_ACTIVE_ARRAY_SIZE =
	    new Key<Rect>("android.sensor.info.activeArraySize", Rect.class);
	public static final Key<Rect> SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE =
	    new Key<Rect>("android.sensor.info.preCorrectionActiveArraySize", Rect.class);
	public static final Key<Size> SENSOR_INFO_PIXEL_ARRAY_SIZE =
	    new Key<Size>("android.sensor.info.pixelArraySize", Size.class);
	public static final Key<SizeF> SENSOR_INFO_PHYSICAL_SIZE =
	    new Key<SizeF>("android.sensor.info.physicalSize", SizeF.class);
	public static final Key<Integer> SENSOR_ORIENTATION =
	    new Key<Integer>("android.sensor.orientation", Integer.class);
	public static final Key<Integer> SENSOR_INFO_TIMESTAMP_SOURCE =
	    new Key<Integer>("android.sensor.info.timestampSource", Integer.class);
	public static final Key<Integer> SENSOR_INFO_COLOR_FILTER_ARRANGEMENT =
	    new Key<Integer>("android.sensor.info.colorFilterArrangement", Integer.class);
	public static final Key<Range<Integer>> SENSOR_INFO_SENSITIVITY_RANGE =
	    new Key("android.sensor.info.sensitivityRange", Range.class);
	public static final Key<Range<Long>> SENSOR_INFO_EXPOSURE_TIME_RANGE =
	    new Key("android.sensor.info.exposureTimeRange", Range.class);
	public static final Key<Long> SENSOR_INFO_MAX_FRAME_DURATION =
	    new Key<Long>("android.sensor.info.maxFrameDuration", Long.class);
	public static final Key<Integer> SENSOR_INFO_WHITE_LEVEL =
	    new Key<Integer>("android.sensor.info.whiteLevel", Integer.class);
	public static final Key<int[]> SENSOR_AVAILABLE_TEST_PATTERN_MODES =
	    new Key<int[]>("android.sensor.availableTestPatternModes", int[].class);

	public static final Key<int[]> CONTROL_AVAILABLE_MODES =
	    new Key<int[]>("android.control.availableModes", int[].class);
	public static final Key<int[]> CONTROL_AE_AVAILABLE_MODES =
	    new Key<int[]>("android.control.aeAvailableModes", int[].class);
	public static final Key<Range<Integer>[]> CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES =
	    new Key("android.control.aeAvailableTargetFpsRanges", Range[].class);
	public static final Key<Range<Integer>> CONTROL_AE_COMPENSATION_RANGE =
	    new Key("android.control.aeCompensationRange", Range.class);
	public static final Key<Rational> CONTROL_AE_COMPENSATION_STEP =
	    new Key<Rational>("android.control.aeCompensationStep", Rational.class);
	public static final Key<int[]> CONTROL_AE_AVAILABLE_ANTIBANDING_MODES =
	    new Key<int[]>("android.control.aeAvailableAntibandingModes", int[].class);
	public static final Key<int[]> CONTROL_AF_AVAILABLE_MODES =
	    new Key<int[]>("android.control.afAvailableModes", int[].class);
	public static final Key<int[]> CONTROL_AWB_AVAILABLE_MODES =
	    new Key<int[]>("android.control.awbAvailableModes", int[].class);
	public static final Key<int[]> CONTROL_AVAILABLE_EFFECTS =
	    new Key<int[]>("android.control.availableEffects", int[].class);
	public static final Key<int[]> CONTROL_AVAILABLE_SCENE_MODES =
	    new Key<int[]>("android.control.availableSceneModes", int[].class);
	public static final Key<int[]> CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES =
	    new Key<int[]>("android.control.availableVideoStabilizationModes", int[].class);
	public static final Key<int[]> CONTROL_MAX_REGIONS =
	    new Key<int[]>("android.control.maxRegions", int[].class);
	/* the three synthetic keys AOSP splits android.control.maxRegions into */
	public static final Key<Integer> CONTROL_MAX_REGIONS_AE =
	    new Key<Integer>("android.control.maxRegionsAe", Integer.class);
	public static final Key<Integer> CONTROL_MAX_REGIONS_AWB =
	    new Key<Integer>("android.control.maxRegionsAwb", Integer.class);
	public static final Key<Integer> CONTROL_MAX_REGIONS_AF =
	    new Key<Integer>("android.control.maxRegionsAf", Integer.class);
	public static final Key<Range<Float>> CONTROL_ZOOM_RATIO_RANGE =
	    new Key("android.control.zoomRatioRange", Range.class);
	public static final Key<Boolean> CONTROL_AE_LOCK_AVAILABLE =
	    new Key<Boolean>("android.control.aeLockAvailable", Boolean.class);
	public static final Key<Boolean> CONTROL_AWB_LOCK_AVAILABLE =
	    new Key<Boolean>("android.control.awbLockAvailable", Boolean.class);

	public static final Key<Boolean> FLASH_INFO_AVAILABLE =
	    new Key<Boolean>("android.flash.info.available", Boolean.class);

	public static final Key<float[]> LENS_INFO_AVAILABLE_FOCAL_LENGTHS =
	    new Key<float[]>("android.lens.info.availableFocalLengths", float[].class);
	public static final Key<float[]> LENS_INFO_AVAILABLE_APERTURES =
	    new Key<float[]>("android.lens.info.availableApertures", float[].class);
	public static final Key<Float> LENS_INFO_MINIMUM_FOCUS_DISTANCE =
	    new Key<Float>("android.lens.info.minimumFocusDistance", Float.class);
	public static final Key<Integer> LENS_INFO_FOCUS_DISTANCE_CALIBRATION =
	    new Key<Integer>("android.lens.info.focusDistanceCalibration", Integer.class);
	public static final Key<int[]> LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION =
	    new Key<int[]>("android.lens.info.availableOpticalStabilization", int[].class);
	public static final Key<Size> LENS_INFO_SHADING_MAP_SIZE =
	    new Key<Size>("android.lens.info.shadingMapSize", Size.class);

	public static final Key<BlackLevelPattern> SENSOR_BLACK_LEVEL_PATTERN =
	    new Key<BlackLevelPattern>("android.sensor.blackLevelPattern", BlackLevelPattern.class);

	public static final Key<int[]> REQUEST_AVAILABLE_CAPABILITIES =
	    new Key<int[]>("android.request.availableCapabilities", int[].class);
	public static final Key<DynamicRangeProfiles> REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES =
	    new Key<DynamicRangeProfiles>("android.request.availableDynamicRangeProfiles",
	        DynamicRangeProfiles.class);
	public static final Key<Integer> REQUEST_PARTIAL_RESULT_COUNT =
	    new Key<Integer>("android.request.partialResultCount", Integer.class);
	public static final Key<Byte> REQUEST_PIPELINE_MAX_DEPTH =
	    new Key<Byte>("android.request.pipelineMaxDepth", Byte.class);

	/* @hide in AOSP as well: apps read it through getPhysicalCameraIds() */
	private static final Key<byte[]> LOGICAL_MULTI_CAMERA_PHYSICAL_IDS =
	    new Key<byte[]>("android.logicalMultiCamera.physicalIds", byte[].class);

	/* synthesised out of the three stream tables, the way AOSP does it */
	public static final Key<StreamConfigurationMap> SCALER_STREAM_CONFIGURATION_MAP =
	    new Key<StreamConfigurationMap>("android.scaler.streamConfigurationMap",
	        StreamConfigurationMap.class);
	public static final Key<MultiResolutionStreamConfigurationMap> SCALER_MULTI_RESOLUTION_STREAM_CONFIGURATION_MAP =
	    new Key<MultiResolutionStreamConfigurationMap>(
	        "android.scaler.multiResolutionStreamConfigurationMap",
	        MultiResolutionStreamConfigurationMap.class);
	public static final Key<Float> SCALER_AVAILABLE_MAX_DIGITAL_ZOOM =
	    new Key<Float>("android.scaler.availableMaxDigitalZoom", Float.class);
	public static final Key<Integer> SCALER_CROPPING_TYPE =
	    new Key<Integer>("android.scaler.croppingType", Integer.class);

	public static final Key<Size[]> JPEG_AVAILABLE_THUMBNAIL_SIZES =
	    new Key<Size[]>("android.jpeg.availableThumbnailSizes", Size[].class);

	public static final Key<Integer> SYNC_MAX_LATENCY =
	    new Key<Integer>("android.sync.maxLatency", Integer.class);

	public static final Key<int[]> STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES =
	    new Key<int[]>("android.statistics.info.availableFaceDetectModes", int[].class);
	public static final Key<Integer> STATISTICS_INFO_MAX_FACE_COUNT =
	    new Key<Integer>("android.statistics.info.maxFaceCount", Integer.class);
	public static final Key<int[]> STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES =
	    new Key<int[]>("android.statistics.info.availableLensShadingMapModes", int[].class);
	public static final Key<int[]> STATISTICS_INFO_AVAILABLE_OIS_DATA_MODES =
	    new Key<int[]>("android.statistics.info.availableOisDataModes", int[].class);

	public static final Key<int[]> NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES =
	    new Key<int[]>("android.noiseReduction.availableNoiseReductionModes", int[].class);
	public static final Key<int[]> EDGE_AVAILABLE_EDGE_MODES =
	    new Key<int[]>("android.edge.availableEdgeModes", int[].class);
	public static final Key<int[]> TONEMAP_AVAILABLE_TONE_MAP_MODES =
	    new Key<int[]>("android.tonemap.availableToneMapModes", int[].class);
	public static final Key<Integer> TONEMAP_MAX_CURVE_POINTS =
	    new Key<Integer>("android.tonemap.maxCurvePoints", Integer.class);
	public static final Key<int[]> HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES =
	    new Key<int[]>("android.hotPixel.availableHotPixelModes", int[].class);
	public static final Key<int[]> COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES =
	    new Key<int[]>("android.colorCorrection.availableAberrationModes", int[].class);
	public static final Key<int[]> SHADING_AVAILABLE_MODES =
	    new Key<int[]>("android.shading.availableModes", int[].class);

	public static final Key<Range<Integer>> CONTROL_POST_RAW_SENSITIVITY_BOOST_RANGE =
	    new Key("android.control.postRawSensitivityBoostRange", Range.class);

	/* the lens's geometric model; the *_MAXIMUM_RESOLUTION twins describe the
	 * same lens against the full-resolution sensor mode */
	public static final Key<float[]> LENS_DISTORTION =
	    new Key<float[]>("android.lens.distortion", float[].class);
	public static final Key<float[]> LENS_DISTORTION_MAXIMUM_RESOLUTION =
	    new Key<float[]>("android.lens.distortionMaximumResolution", float[].class);
	public static final Key<float[]> LENS_INTRINSIC_CALIBRATION =
	    new Key<float[]>("android.lens.intrinsicCalibration", float[].class);
	public static final Key<float[]> LENS_INTRINSIC_CALIBRATION_MAXIMUM_RESOLUTION =
	    new Key<float[]>("android.lens.intrinsicCalibrationMaximumResolution", float[].class);
	public static final Key<float[]> LENS_POSE_ROTATION =
	    new Key<float[]>("android.lens.poseRotation", float[].class);
	public static final Key<float[]> LENS_POSE_TRANSLATION =
	    new Key<float[]>("android.lens.poseTranslation", float[].class);
	public static final Key<Integer> LENS_POSE_REFERENCE =
	    new Key<Integer>("android.lens.poseReference", Integer.class);
	public static final Key<Float> LENS_INFO_HYPERFOCAL_DISTANCE =
	    new Key<Float>("android.lens.info.hyperfocalDistance", Float.class);

	public static final Key<long[]> SCALER_AVAILABLE_STREAM_USE_CASES =
	    new Key<long[]>("android.scaler.availableStreamUseCases", long[].class);
	/* synthesised like SCALER_STREAM_CONFIGURATION_MAP, from the
	 * maximum-resolution stream tables; null on a camera without them */
	public static final Key<StreamConfigurationMap> SCALER_STREAM_CONFIGURATION_MAP_MAXIMUM_RESOLUTION =
	    new Key<StreamConfigurationMap>("android.scaler.streamConfigurationMapMaximumResolution",
	        StreamConfigurationMap.class);

	public static final Key<Rect> SENSOR_INFO_ACTIVE_ARRAY_SIZE_MAXIMUM_RESOLUTION =
	    new Key<Rect>("android.sensor.info.activeArraySizeMaximumResolution", Rect.class);
	public static final Key<Rect> SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE_MAXIMUM_RESOLUTION =
	    new Key<Rect>("android.sensor.info.preCorrectionActiveArraySizeMaximumResolution", Rect.class);
	public static final Key<Size> SENSOR_INFO_PIXEL_ARRAY_SIZE_MAXIMUM_RESOLUTION =
	    new Key<Size>("android.sensor.info.pixelArraySizeMaximumResolution", Size.class);
	public static final Key<Integer> SENSOR_MAX_ANALOG_SENSITIVITY =
	    new Key<Integer>("android.sensor.maxAnalogSensitivity", Integer.class);
	public static final Key<Rect[]> SENSOR_OPTICAL_BLACK_REGIONS =
	    new Key<Rect[]>("android.sensor.opticalBlackRegions", Rect[].class);
	/* the DNG colour-calibration pairs: illuminant 1 is an int in Java, 2 a byte */
	public static final Key<Integer> SENSOR_REFERENCE_ILLUMINANT1 =
	    new Key<Integer>("android.sensor.referenceIlluminant1", Integer.class);
	public static final Key<Byte> SENSOR_REFERENCE_ILLUMINANT2 =
	    new Key<Byte>("android.sensor.referenceIlluminant2", Byte.class);
	public static final Key<ColorSpaceTransform> SENSOR_CALIBRATION_TRANSFORM1 =
	    new Key<ColorSpaceTransform>("android.sensor.calibrationTransform1", ColorSpaceTransform.class);
	public static final Key<ColorSpaceTransform> SENSOR_CALIBRATION_TRANSFORM2 =
	    new Key<ColorSpaceTransform>("android.sensor.calibrationTransform2", ColorSpaceTransform.class);
	public static final Key<ColorSpaceTransform> SENSOR_COLOR_TRANSFORM1 =
	    new Key<ColorSpaceTransform>("android.sensor.colorTransform1", ColorSpaceTransform.class);
	public static final Key<ColorSpaceTransform> SENSOR_COLOR_TRANSFORM2 =
	    new Key<ColorSpaceTransform>("android.sensor.colorTransform2", ColorSpaceTransform.class);
	public static final Key<ColorSpaceTransform> SENSOR_FORWARD_MATRIX1 =
	    new Key<ColorSpaceTransform>("android.sensor.forwardMatrix1", ColorSpaceTransform.class);
	public static final Key<ColorSpaceTransform> SENSOR_FORWARD_MATRIX2 =
	    new Key<ColorSpaceTransform>("android.sensor.forwardMatrix2", ColorSpaceTransform.class);

	/* built from android.info.deviceStateOrientations, which pairs a folded-state
	 * bitmask with the sensor orientation that state implies */
	public static final Key<DeviceStateSensorOrientationMap> INFO_DEVICE_STATE_SENSOR_ORIENTATION_MAP =
	    new Key<DeviceStateSensorOrientationMap>("android.info.deviceStateSensorOrientationMap",
	        DeviceStateSensorOrientationMap.class);

	private static final Map<String, Key<?>> KEYS_BY_NAME = keysByName();

	private static Map<String, Key<?>> keysByName() {
		Map<String, Key<?>> map = new HashMap<String, Key<?>>();

		for (java.lang.reflect.Field field : CameraCharacteristics.class.getDeclaredFields()) {
			if (field.getType() != Key.class)
				continue;
			try {
				Key<?> key = (Key<?>)field.get(null);
				map.put(key.getName(), key);
			} catch (IllegalAccessException e) {
				/* every Key constant is declared in this class, private or not */
			}
		}
		return map;
	}
}
