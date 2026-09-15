package android.hardware.camera2;

import android.graphics.Rect;
import android.hardware.camera2.impl.CameraMetadataNative;
import android.hardware.camera2.params.ColorSpaceTransform;
import android.hardware.camera2.params.MeteringRectangle;
import android.hardware.camera2.params.RggbChannelVector;
import android.hardware.camera2.params.TonemapCurve;
import android.location.Location;
import android.util.Range;
import android.util.Size;
import android.view.Surface;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * One capture's settings plus the outputs it is aimed at.
 *
 * The settings live in the same native metadata bag the characteristics use,
 * so a key writes and reads back through one bridge, and a key the camera has
 * no tag for is dropped rather than thrown.
 */
public final class CaptureRequest extends CameraMetadata<CaptureRequest.Key<?>> {

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
			return "CaptureRequest.Key(" + name + ")";
		}
	}

	/* AOSP's name for the bag, because camera apps reach it by reflection */
	private final CameraMetadataNative mLogicalCameraSettings;
	private final List<Surface> targets;
	private final Object tag;
	private final boolean reprocess;

	private CaptureRequest(CameraMetadataNative mLogicalCameraSettings, Set<Surface> targets, Object tag,
	    boolean reprocess) {
		this.mLogicalCameraSettings = mLogicalCameraSettings;
		this.targets = Collections.unmodifiableList(new ArrayList<Surface>(targets));
		this.tag = tag;
		this.reprocess = reprocess;
	}

	/** True for a request built by createReprocessCaptureRequest(). */
	public boolean isReprocess() {
		return reprocess;
	}

	@SuppressWarnings("unchecked")
	public <T> T get(Key<T> key) {
		return (T)mLogicalCameraSettings.get(key.getName(), key.getType());
	}

	@Override
	CameraMetadataNative getAtlBag() {
		return mLogicalCameraSettings;
	}

	@Override
	public List<Key<?>> getKeys() {
		List<Key<?>> list = new ArrayList<Key<?>>();

		for (int tag : mLogicalCameraSettings.getTags()) {
			String name = mLogicalCameraSettings.getTagName(tag);

			if (name != null)
				list.add(new Key<Object>(name, Object.class));
		}
		return Collections.unmodifiableList(list);
	}

	public List<Surface> getTargets() {
		return targets;
	}

	public boolean containsTarget(Surface surface) {
		return targets.contains(surface);
	}

	public Object getTag() {
		return tag;
	}

	/** the mLogicalCameraSettings themselves, for the session that submits this request */
	CameraMetadataNative getSettings() {
		return mLogicalCameraSettings;
	}

	@Override
	public String toString() {
		return "CaptureRequest(" + targets.size() + " target(s))";
	}

	public static final class Builder {
		private final CameraMetadataNative settings;
		private final Set<Surface> targets = new LinkedHashSet<Surface>();
		private final boolean reprocess;
		private Object tag;

		Builder(CameraMetadataNative settings) {
			this(settings, false);
		}

		Builder(CameraMetadataNative settings, boolean reprocess) {
			this.settings = settings;
			this.reprocess = reprocess;
		}

		public void addTarget(Surface outputTarget) {
			if (outputTarget == null)
				throw new IllegalArgumentException("target must not be null");
			targets.add(outputTarget);
		}

		public void removeTarget(Surface outputTarget) {
			targets.remove(outputTarget);
		}

		public <T> void set(Key<T> key, T value) {
			settings.set(key.getName(), value);
		}

		public <T> Builder setPhysicalCameraKey(Key<T> key, T value, String physicalId) {
			throw new UnsupportedOperationException("per-physical request settings are not supported");
		}

		@SuppressWarnings("unchecked")
		public <T> T get(Key<T> key) {
			return (T)settings.get(key.getName(), key.getType());
		}

		public void setTag(Object tag) {
			this.tag = tag;
		}

		/** the built request is independent of further Builder changes */
		public CaptureRequest build() {
			return new CaptureRequest(settings.copy(), targets, tag, reprocess);
		}
	}

	public static final Key<Integer> CONTROL_MODE =
	    new Key<Integer>("android.control.mode", Integer.class);
	public static final Key<Integer> CONTROL_CAPTURE_INTENT =
	    new Key<Integer>("android.control.captureIntent", Integer.class);
	public static final Key<Integer> CONTROL_AE_MODE =
	    new Key<Integer>("android.control.aeMode", Integer.class);
	public static final Key<Boolean> CONTROL_AE_LOCK =
	    new Key<Boolean>("android.control.aeLock", Boolean.class);
	public static final Key<Integer> CONTROL_AE_ANTIBANDING_MODE =
	    new Key<Integer>("android.control.aeAntibandingMode", Integer.class);
	public static final Key<Integer> CONTROL_AE_EXPOSURE_COMPENSATION =
	    new Key<Integer>("android.control.aeExposureCompensation", Integer.class);
	public static final Key<Integer> CONTROL_AE_PRECAPTURE_TRIGGER =
	    new Key<Integer>("android.control.aePrecaptureTrigger", Integer.class);
	public static final Key<Range<Integer>> CONTROL_AE_TARGET_FPS_RANGE =
	    new Key("android.control.aeTargetFpsRange", Range.class);
	public static final Key<Integer> CONTROL_AF_MODE =
	    new Key<Integer>("android.control.afMode", Integer.class);
	public static final Key<Integer> CONTROL_AF_TRIGGER =
	    new Key<Integer>("android.control.afTrigger", Integer.class);
	public static final Key<Integer> CONTROL_AWB_MODE =
	    new Key<Integer>("android.control.awbMode", Integer.class);
	public static final Key<Boolean> CONTROL_AWB_LOCK =
	    new Key<Boolean>("android.control.awbLock", Boolean.class);
	public static final Key<Integer> CONTROL_EFFECT_MODE =
	    new Key<Integer>("android.control.effectMode", Integer.class);
	public static final Key<Integer> CONTROL_SCENE_MODE =
	    new Key<Integer>("android.control.sceneMode", Integer.class);
	public static final Key<Integer> CONTROL_VIDEO_STABILIZATION_MODE =
	    new Key<Integer>("android.control.videoStabilizationMode", Integer.class);
	public static final Key<Boolean> CONTROL_ENABLE_ZSL =
	    new Key<Boolean>("android.control.enableZsl", Boolean.class);
	public static final Key<Float> CONTROL_ZOOM_RATIO =
	    new Key<Float>("android.control.zoomRatio", Float.class);

	public static final Key<Integer> FLASH_MODE =
	    new Key<Integer>("android.flash.mode", Integer.class);

	public static final Key<Integer> JPEG_ORIENTATION =
	    new Key<Integer>("android.jpeg.orientation", Integer.class);
	public static final Key<Byte> JPEG_QUALITY =
	    new Key<Byte>("android.jpeg.quality", Byte.class);
	public static final Key<Byte> JPEG_THUMBNAIL_QUALITY =
	    new Key<Byte>("android.jpeg.thumbnailQuality", Byte.class);
	public static final Key<Size> JPEG_THUMBNAIL_SIZE =
	    new Key<Size>("android.jpeg.thumbnailSize", Size.class);
	public static final Key<Location> JPEG_GPS_LOCATION =
	    new Key<Location>("android.jpeg.gpsLocation", Location.class);

	public static final Key<Float> LENS_FOCUS_DISTANCE =
	    new Key<Float>("android.lens.focusDistance", Float.class);
	public static final Key<Float> LENS_FOCAL_LENGTH =
	    new Key<Float>("android.lens.focalLength", Float.class);
	public static final Key<Float> LENS_APERTURE =
	    new Key<Float>("android.lens.aperture", Float.class);
	public static final Key<Integer> LENS_OPTICAL_STABILIZATION_MODE =
	    new Key<Integer>("android.lens.opticalStabilizationMode", Integer.class);

	public static final Key<Rect> SCALER_CROP_REGION =
	    new Key<Rect>("android.scaler.cropRegion", Rect.class);

	public static final Key<Long> SENSOR_EXPOSURE_TIME =
	    new Key<Long>("android.sensor.exposureTime", Long.class);
	public static final Key<Long> SENSOR_FRAME_DURATION =
	    new Key<Long>("android.sensor.frameDuration", Long.class);
	public static final Key<Integer> SENSOR_SENSITIVITY =
	    new Key<Integer>("android.sensor.sensitivity", Integer.class);

	public static final Key<Integer> STATISTICS_FACE_DETECT_MODE =
	    new Key<Integer>("android.statistics.faceDetectMode", Integer.class);
	public static final Key<Integer> NOISE_REDUCTION_MODE =
	    new Key<Integer>("android.noiseReduction.mode", Integer.class);
	public static final Key<Integer> HOT_PIXEL_MODE =
	    new Key<Integer>("android.hotPixel.mode", Integer.class);
	public static final Key<Integer> EDGE_MODE =
	    new Key<Integer>("android.edge.mode", Integer.class);
	public static final Key<Integer> TONEMAP_MODE =
	    new Key<Integer>("android.tonemap.mode", Integer.class);
	public static final Key<TonemapCurve> TONEMAP_CURVE =
	    new Key<TonemapCurve>("android.tonemap.curve", TonemapCurve.class);
	public static final Key<Integer> TONEMAP_PRESET_CURVE =
	    new Key<Integer>("android.tonemap.presetCurve", Integer.class);
	public static final Key<Integer> COLOR_CORRECTION_MODE =
	    new Key<Integer>("android.colorCorrection.mode", Integer.class);
	public static final Key<Integer> COLOR_CORRECTION_ABERRATION_MODE =
	    new Key<Integer>("android.colorCorrection.aberrationMode", Integer.class);
	public static final Key<Integer> DISTORTION_CORRECTION_MODE =
	    new Key<Integer>("android.distortionCorrection.mode", Integer.class);
	public static final Key<Boolean> BLACK_LEVEL_LOCK =
	    new Key<Boolean>("android.blackLevel.lock", Boolean.class);

	public static final Key<MeteringRectangle[]> CONTROL_AE_REGIONS =
	    new Key<MeteringRectangle[]>("android.control.aeRegions", MeteringRectangle[].class);
	public static final Key<MeteringRectangle[]> CONTROL_AF_REGIONS =
	    new Key<MeteringRectangle[]>("android.control.afRegions", MeteringRectangle[].class);
	public static final Key<MeteringRectangle[]> CONTROL_AWB_REGIONS =
	    new Key<MeteringRectangle[]>("android.control.awbRegions", MeteringRectangle[].class);

	public static final Key<ColorSpaceTransform> COLOR_CORRECTION_TRANSFORM =
	    new Key<ColorSpaceTransform>("android.colorCorrection.transform", ColorSpaceTransform.class);
	public static final Key<RggbChannelVector> COLOR_CORRECTION_GAINS =
	    new Key<RggbChannelVector>("android.colorCorrection.gains", RggbChannelVector.class);

	public static final Key<Integer> SENSOR_TEST_PATTERN_MODE =
	    new Key<Integer>("android.sensor.testPatternMode", Integer.class);
	public static final Key<Integer> STATISTICS_LENS_SHADING_MAP_MODE =
	    new Key<Integer>("android.statistics.lensShadingMapMode", Integer.class);
	public static final Key<Integer> STATISTICS_OIS_DATA_MODE =
	    new Key<Integer>("android.statistics.oisDataMode", Integer.class);
	public static final Key<Boolean> STATISTICS_HOT_PIXEL_MAP_MODE =
	    new Key<Boolean>("android.statistics.hotPixelMapMode", Boolean.class);

	public static final Key<Integer> CONTROL_EXTENDED_SCENE_MODE =
	    new Key<Integer>("android.control.extendedSceneMode", Integer.class);
	public static final Key<Integer> SENSOR_PIXEL_MODE =
	    new Key<Integer>("android.sensor.pixelMode", Integer.class);
	/* not a public AOSP key and not in the NDK tag table: no camera has the
	 * tag, so it writes nowhere and reads back null */
	public static final Key<Boolean> LOGICAL_MULTI_CAMERA_ADDITIONAL_RESULTS =
	    new Key<Boolean>("android.logicalMultiCamera.additionalResults", Boolean.class);

	private static final Map<String, Key<?>> KEYS_BY_NAME = keysByName();

	/** the typed constant for a key name, or null for one ATL has no constant for */
	static Key<?> keyForName(String name) {
		return KEYS_BY_NAME.get(name);
	}

	private static Map<String, Key<?>> keysByName() {
		Map<String, Key<?>> map = new HashMap<String, Key<?>>();

		for (java.lang.reflect.Field field : CaptureRequest.class.getDeclaredFields()) {
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
