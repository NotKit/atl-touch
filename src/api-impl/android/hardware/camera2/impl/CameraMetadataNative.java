package android.hardware.camera2.impl;

import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.params.BlackLevelPattern;
import android.hardware.camera2.params.ColorSpaceTransform;
import android.hardware.camera2.params.DeviceStateSensorOrientationMap;
import android.hardware.camera2.params.DynamicRangeProfiles;
import android.hardware.camera2.params.Face;
import android.hardware.camera2.params.LensShadingMap;
import android.hardware.camera2.params.MeteringRectangle;
import android.hardware.camera2.params.OisSample;
import android.hardware.camera2.params.RggbChannelVector;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.util.Pair;
import android.util.Range;
import android.util.Rational;
import android.util.Size;
import android.util.SizeF;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * A bag of camera2 metadata entries living in native code
 * (src/api-impl-jni/camera/camera2_metadata.c), plus the marshaling from the
 * HAL's (tag, type, values) to the Java types the Key constants declare.
 *
 * A tag ATL has no name for still reads back by id, and a key whose Java type
 * we do not know reads back as the natural array, so a vendor key never throws.
 */
public final class CameraMetadataNative {
	/* which key set getAvailableKeys() asks for; camera2_metadata.h */
	public static final int KEYS_CHARACTERISTICS = 0;
	public static final int KEYS_REQUEST = 1;
	public static final int KEYS_RESULT = 2;

	/* camera2_metadata.h value types */
	public static final int TYPE_BYTE = 0;
	public static final int TYPE_INT32 = 1;
	public static final int TYPE_FLOAT = 2;
	public static final int TYPE_INT64 = 3;
	public static final int TYPE_DOUBLE = 4;
	public static final int TYPE_RATIONAL = 5;

	/* ATL_CAMERA2_TAG_INVALID; android.colorCorrection.mode is tag 0 */
	public static final int TAG_INVALID = -1;

	private long nativePtr;

	private CameraMetadataNative(long nativePtr) {
		this.nativePtr = nativePtr;
	}

	/** null when no backend can serve camera2 at all. */
	public static String[] getCameraIdList() {
		return native_getCameraIdList();
	}

	/** null when the backend does not know this camera. */
	public static CameraMetadataNative getStaticMetadata(String cameraId) {
		long ptr = native_getStaticMetadata(cameraId);

		return ptr == 0 ? null : new CameraMetadataNative(ptr);
	}

	/** An empty bag, for a capture request being built. */
	public static CameraMetadataNative create() {
		return new CameraMetadataNative(native_create());
	}

	/** Takes ownership of a bag native code built (a capture result). */
	public static CameraMetadataNative adopt(long ptr) {
		return ptr == 0 ? null : new CameraMetadataNative(ptr);
	}

	public synchronized CameraMetadataNative copy() {
		return new CameraMetadataNative(native_copy(ptr()));
	}

	/** the bag itself, for the native camera2 device */
	public synchronized long getPtr() {
		return ptr();
	}

	/** the tags of one of the KEYS_* sets; empty when the backend has no list. */
	public static int[] getAvailableKeys(String cameraId, int which) {
		int[] keys = native_getAvailableKeys(cameraId, which);

		return keys == null ? new int[0] : keys;
	}

	public synchronized void close() {
		if (nativePtr != 0) {
			native_free(nativePtr);
			nativePtr = 0;
		}
	}

	@Override
	protected void finalize() throws Throwable {
		try {
			close();
		} finally {
			super.finalize();
		}
	}

	private synchronized long ptr() {
		if (nativePtr == 0)
			throw new IllegalStateException("metadata is closed");
		return nativePtr;
	}

	public int[] getTags() {
		int[] tags = native_getTags(ptr());

		return tags == null ? new int[0] : tags;
	}

	/** TAG_INVALID when neither the generated table nor a vendor entry names it. */
	public int getTag(String name) {
		return native_getTag(ptr(), name);
	}

	/** null when nothing names the tag. */
	public String getTagName(int tag) {
		return native_getTagName(ptr(), tag);
	}

	/** TYPE_*, or -1 when the tag is not present. */
	public int getType(int tag) {
		return native_getType(ptr(), tag);
	}

	public byte[] readBytes(int tag) {
		return native_readBytes(ptr(), tag);
	}

	public int[] readInts(int tag) {
		return native_readInts(ptr(), tag);
	}

	public float[] readFloats(int tag) {
		return native_readFloats(ptr(), tag);
	}

	public long[] readLongs(int tag) {
		return native_readLongs(ptr(), tag);
	}

	public double[] readDoubles(int tag) {
		return native_readDoubles(ptr(), tag);
	}

	/**
	 * The value of a key, marshaled to the type its Key constant declares.
	 * null when the camera does not report the key; a type of null or Object
	 * yields the raw values as an array.
	 */
	public Object get(String name, Class<?> type) {
		int tag;

		/* a key the HAL spreads over several tags has no tag of its own */
		if (type == StreamConfigurationMap.class)
			return streamConfigurationMap(name.endsWith(MAXIMUM_RESOLUTION)
			    ? MAXIMUM_RESOLUTION : "");
		if (type == DeviceStateSensorOrientationMap.class)
			return deviceStateSensorOrientationMap();
		if (type == Face[].class)
			return faces();
		if (type == OisSample[].class)
			return oisSamples();
		if (type == LensShadingMap.class)
			return lensShadingMap();
		if (type == DynamicRangeProfiles.class)
			return new DynamicRangeProfiles();
		if (name.startsWith(MAX_REGIONS) && name.length() > MAX_REGIONS.length())
			return maxRegions(name.substring(MAX_REGIONS.length()));

		tag = getTag(name);
		if (tag == TAG_INVALID)
			return null;
		return get(tag, type);
	}

	public Object get(int tag, Class<?> type) {
		int nativeType = getType(tag);

		if (nativeType < 0)
			return null;
		if (type == null || type == Object.class)
			return raw(tag, nativeType);

		/* an enum is a byte in the HAL and an int in Java, so the int
		 * readers widen bytes; every "int[]" key is a byte[] key here */
		if (type == Integer.class || type == int.class) {
			int[] v = readInts(tag);
			return v == null || v.length == 0 ? null : Integer.valueOf(v[0]);
		}
		if (type == Byte.class || type == byte.class) {
			byte[] v = readBytes(tag);
			return v == null || v.length == 0 ? null : Byte.valueOf(v[0]);
		}
		if (type == Boolean.class || type == boolean.class) {
			byte[] v = readBytes(tag);
			return v == null || v.length == 0 ? null : Boolean.valueOf(v[0] != 0);
		}
		if (type == Long.class || type == long.class) {
			long[] v = readLongs(tag);
			return v == null || v.length == 0 ? null : Long.valueOf(v[0]);
		}
		if (type == Float.class || type == float.class) {
			float[] v = readFloats(tag);
			return v == null || v.length == 0 ? null : Float.valueOf(v[0]);
		}
		if (type == Double.class || type == double.class) {
			double[] v = readDoubles(tag);
			return v == null || v.length == 0 ? null : Double.valueOf(v[0]);
		}
		if (type == int[].class)
			return readInts(tag);
		if (type == byte[].class)
			return readBytes(tag);
		if (type == float[].class)
			return readFloats(tag);
		if (type == long[].class)
			return readLongs(tag);
		if (type == double[].class)
			return readDoubles(tag);
		if (type == String.class) {
			byte[] v = readBytes(tag);
			return v == null ? null : cString(v);
		}
		if (type == Rational.class) {
			int[] v = readInts(tag);
			return v == null || v.length < 2 ? null : new Rational(v[0], v[1]);
		}
		if (type == Rational[].class) {
			int[] v = readInts(tag);
			if (v == null)
				return null;
			Rational[] out = new Rational[v.length / 2];
			for (int i = 0; i < out.length; i++)
				out[i] = new Rational(v[i * 2], v[i * 2 + 1]);
			return out;
		}
		if (type == Size.class) {
			int[] v = readInts(tag);
			return v == null || v.length < 2 ? null : new Size(v[0], v[1]);
		}
		if (type == Size[].class) {
			int[] v = readInts(tag);
			if (v == null)
				return null;
			Size[] out = new Size[v.length / 2];
			for (int i = 0; i < out.length; i++)
				out[i] = new Size(v[i * 2], v[i * 2 + 1]);
			return out;
		}
		if (type == SizeF.class) {
			float[] v = readFloats(tag);
			return v == null || v.length < 2 ? null : new SizeF(v[0], v[1]);
		}
		/* the HAL says (x, y, width, height), Rect wants the two corners */
		if (type == Rect.class) {
			int[] v = readInts(tag);
			return v == null || v.length < 4 ? null : new Rect(v[0], v[1], v[0] + v[2], v[1] + v[3]);
		}
		if (type == Rect[].class) {
			int[] v = readInts(tag);
			if (v == null)
				return null;
			Rect[] out = new Rect[v.length / 4];
			for (int i = 0; i < out.length; i++)
				out[i] = new Rect(v[i * 4], v[i * 4 + 1], v[i * 4] + v[i * 4 + 2],
				    v[i * 4 + 1] + v[i * 4 + 3]);
			return out;
		}
		/* the HAL's region is (x, y, width, height, weight) */
		if (type == MeteringRectangle.class) {
			int[] v = readInts(tag);
			return v == null || v.length < 5 ? null
			    : new MeteringRectangle(v[0], v[1], v[2], v[3], v[4]);
		}
		if (type == MeteringRectangle[].class) {
			int[] v = readInts(tag);
			if (v == null)
				return null;
			MeteringRectangle[] out = new MeteringRectangle[v.length / 5];
			for (int i = 0; i < out.length; i++)
				out[i] = new MeteringRectangle(v[i * 5], v[i * 5 + 1], v[i * 5 + 2],
				    v[i * 5 + 3], v[i * 5 + 4]);
			return out;
		}
		if (type == ColorSpaceTransform.class) {
			int[] v = readInts(tag);
			return v == null || v.length < 18 ? null : new ColorSpaceTransform(v);
		}
		if (type == BlackLevelPattern.class) {
			int[] v = readInts(tag);
			return v == null || v.length < 4 ? null : new BlackLevelPattern(v);
		}
		if (type == RggbChannelVector.class) {
			float[] v = readFloats(tag);
			return v == null || v.length < 4 ? null : new RggbChannelVector(v[0], v[1], v[2], v[3]);
		}
		if (type == Point[].class) {
			int[] v = readInts(tag);
			if (v == null)
				return null;
			Point[] out = new Point[v.length / 2];
			for (int i = 0; i < out.length; i++)
				out[i] = new Point(v[i * 2], v[i * 2 + 1]);
			return out;
		}
		/* a Pair key is two values of the tag's own type in one entry
		 * (LENS_FOCUS_RANGE floats, SENSOR_NOISE_PROFILE doubles) */
		if (type == Pair.class)
			return pair(tag, nativeType, 0);
		if (type == Pair[].class) {
			int count = nativeType == TYPE_DOUBLE ? readDoubles(tag).length : readFloats(tag).length;
			Pair<?, ?>[] out = new Pair<?, ?>[count / 2];
			for (int i = 0; i < out.length; i++)
				out[i] = pair(tag, nativeType, i);
			return out;
		}
		if (type == Range.class)
			return range(tag, nativeType, 0);
		if (type == Range[].class) {
			int count = nativeType == TYPE_INT64 ? readLongs(tag).length : readInts(tag).length;
			Range<?>[] out = new Range<?>[count / 2];
			for (int i = 0; i < out.length; i++)
				out[i] = range(tag, nativeType, i);
			return out;
		}

		/* a key whose Java type we do not model: hand back the values */
		return raw(tag, nativeType);
	}

	/**
	 * Writes a key, narrowing the value to whatever type the tag is declared
	 * with in native code. A key this camera has no tag for is dropped rather
	 * than thrown, the same way an unknown key reads back as null.
	 */
	public void set(String name, Object value) {
		int tag = getTag(name);

		if (tag == TAG_INVALID)
			return;
		if (value == null) {
			native_erase(ptr(), tag);
			return;
		}
		if (value instanceof String) {
			byte[] utf8 = ((String)value).getBytes(StandardCharsets.UTF_8);
			byte[] terminated = new byte[utf8.length + 1];
			System.arraycopy(utf8, 0, terminated, 0, utf8.length);
			native_writeBytes(ptr(), tag, terminated);
			return;
		}
		if (value instanceof byte[]) {
			native_writeBytes(ptr(), tag, (byte[])value);
			return;
		}

		double[] fractional = asDoubles(value);
		if (fractional != null) {
			native_writeDoubles(ptr(), tag, fractional);
			return;
		}
		long[] integral = asLongs(value);
		if (integral != null)
			native_writeLongs(ptr(), tag, integral);
	}

	private static double[] asDoubles(Object value) {
		if (value instanceof Float || value instanceof Double)
			return new double[] {((Number)value).doubleValue()};
		if (value instanceof RggbChannelVector) {
			float[] out = new float[RggbChannelVector.COUNT];
			((RggbChannelVector)value).copyTo(out, 0);
			return asDoubles(out);
		}
		if (value instanceof float[]) {
			float[] v = (float[])value;
			double[] out = new double[v.length];
			for (int i = 0; i < v.length; i++)
				out[i] = v[i];
			return out;
		}
		if (value instanceof double[])
			return (double[])value;
		return null;
	}

	/* the HAL's own layouts: a Rect is (x, y, width, height), a Rational and a
	 * Range are both a pair */
	private static long[] asLongs(Object value) {
		if (value instanceof Number)
			return new long[] {((Number)value).longValue()};
		if (value instanceof Boolean)
			return new long[] {((Boolean)value).booleanValue() ? 1 : 0};
		if (value instanceof int[]) {
			int[] v = (int[])value;
			long[] out = new long[v.length];
			for (int i = 0; i < v.length; i++)
				out[i] = v[i];
			return out;
		}
		if (value instanceof long[])
			return (long[])value;
		if (value instanceof boolean[]) {
			boolean[] v = (boolean[])value;
			long[] out = new long[v.length];
			for (int i = 0; i < v.length; i++)
				out[i] = v[i] ? 1 : 0;
			return out;
		}
		if (value instanceof Rect) {
			Rect r = (Rect)value;
			return new long[] {r.left, r.top, r.width(), r.height()};
		}
		if (value instanceof Size) {
			Size s = (Size)value;
			return new long[] {s.getWidth(), s.getHeight()};
		}
		if (value instanceof Rational) {
			Rational r = (Rational)value;
			return new long[] {r.getNumerator(), r.getDenominator()};
		}
		if (value instanceof Range) {
			Range<?> r = (Range<?>)value;
			Object lower = r.getLower(), upper = r.getUpper();
			if (lower instanceof Number && upper instanceof Number)
				return new long[] {((Number)lower).longValue(), ((Number)upper).longValue()};
		}
		if (value instanceof MeteringRectangle)
			return asLongs(new MeteringRectangle[] {(MeteringRectangle)value});
		if (value instanceof MeteringRectangle[]) {
			MeteringRectangle[] v = (MeteringRectangle[])value;
			long[] out = new long[v.length * 5];
			for (int i = 0; i < v.length; i++) {
				out[i * 5] = v[i].getX();
				out[i * 5 + 1] = v[i].getY();
				out[i * 5 + 2] = v[i].getWidth();
				out[i * 5 + 3] = v[i].getHeight();
				out[i * 5 + 4] = v[i].getMeteringWeight();
			}
			return out;
		}
		if (value instanceof ColorSpaceTransform) {
			int[] elements = new int[18];
			((ColorSpaceTransform)value).copyElements(elements, 0);
			return asLongs(elements);
		}
		if (value instanceof BlackLevelPattern) {
			int[] offsets = new int[BlackLevelPattern.COUNT];
			((BlackLevelPattern)value).copyTo(offsets, 0);
			return asLongs(offsets);
		}
		return null;
	}

	/* ---- keys the HAL spreads over several tags ---- */

	private static final String MAX_REGIONS = "android.control.maxRegions";
	/* the suffix every full-resolution-sensor-mode twin of a tag carries */
	private static final String MAXIMUM_RESOLUTION = "MaximumResolution";

	/** android.control.maxRegions is (AE, AWB, AF); the three keys index into it. */
	private Integer maxRegions(String which) {
		int[] regions = ints(MAX_REGIONS);
		int index = which.equals("Ae") ? 0 : which.equals("Awb") ? 1 : which.equals("Af") ? 2 : -1;

		if (regions == null || index < 0 || index >= regions.length)
			return null;
		return Integer.valueOf(regions[index]);
	}

	/**
	 * The stream tables, either the ordinary set or the maximum-resolution
	 * (SENSOR_PIXEL_MODE_MAXIMUM_RESOLUTION) one. A camera without the second
	 * set answers null for it rather than handing back the first.
	 */
	private StreamConfigurationMap streamConfigurationMap(String suffix) {
		int[] configurations = ints("android.scaler.availableStreamConfigurations" + suffix);

		if (configurations == null)
			return null;
		return new StreamConfigurationMap(configurations,
		    longs("android.scaler.availableMinFrameDurations" + suffix),
		    longs("android.scaler.availableStallDurations" + suffix), null, null);
	}

	private DeviceStateSensorOrientationMap deviceStateSensorOrientationMap() {
		long[] orientations = longs("android.info.deviceStateOrientations");

		return orientations == null ? null : new DeviceStateSensorOrientationMap(orientations);
	}

	/* the null answer is a wall for a camera app; say so once */
	private static boolean facesReported = false;

	/**
	 * SIMPLE face detection reports bounds and scores; ids and landmarks are FULL.
	 *
	 * A result that names the mode always answers with an array, empty when the
	 * mode is OFF or the HAL left the per-face tags out, as AOSP does. Only
	 * metadata with none of the five face tags at all - a request, or a
	 * characteristics bag - has no faces to report and answers null. Google
	 * Camera reads this on every shot and throws "STATISTICS_FACES not present
	 * in metadata" on a null, so a frame with nobody in it must still say so.
	 */
	private Face[] faces() {
		int[] mode = ints("android.statistics.faceDetectMode");
		int[] rectangles = ints("android.statistics.faceRectangles");
		byte[] scores = bytes("android.statistics.faceScores");
		int[] ids = ints("android.statistics.faceIds");
		int[] landmarks = ints("android.statistics.faceLandmarks");

		if (mode == null && rectangles == null && scores == null && ids == null && landmarks == null) {
			if (!facesReported) {
				facesReported = true;
				android.util.Log.i("CameraMetadata",
				    "no face tags in this metadata, STATISTICS_FACES is null");
			}
			return null;
		}
		if (mode != null && mode.length > 0
		    && mode[0] == CameraMetadata.STATISTICS_FACE_DETECT_MODE_OFF)
			return new Face[0];
		if (rectangles == null)
			return new Face[0];

		List<Face> out = new ArrayList<Face>();
		for (int i = 0; i + 3 < rectangles.length; i += 4) {
			int face = i / 4;
			Face.Builder builder = new Face.Builder();

			builder.setBounds(new Rect(rectangles[i], rectangles[i + 1],
			    rectangles[i] + rectangles[i + 2], rectangles[i + 1] + rectangles[i + 3]));
			if (scores != null && face < scores.length)
				builder.setScore(Math.max(Face.SCORE_MIN,
				    Math.min(Face.SCORE_MAX, scores[face] & 0xff)));
			if (landmarks != null && landmarks.length >= (face + 1) * 6) {
				builder.setLeftEyePosition(new android.graphics.Point(landmarks[face * 6],
				    landmarks[face * 6 + 1]));
				builder.setRightEyePosition(new android.graphics.Point(landmarks[face * 6 + 2],
				    landmarks[face * 6 + 3]));
				builder.setMouthPosition(new android.graphics.Point(landmarks[face * 6 + 4],
				    landmarks[face * 6 + 5]));
				if (ids != null && face < ids.length)
					builder.setId(ids[face]);
			}
			out.add(builder.build());
		}
		return out.toArray(new Face[out.size()]);
	}

	private OisSample[] oisSamples() {
		long[] timestamps = longs("android.statistics.oisTimestamps");
		float[] x = floats("android.statistics.oisXShifts");
		float[] y = floats("android.statistics.oisYShifts");

		if (timestamps == null || x == null || y == null)
			return null;

		int count = Math.min(timestamps.length, Math.min(x.length, y.length));
		OisSample[] out = new OisSample[count];
		for (int i = 0; i < count; i++)
			out[i] = new OisSample(timestamps[i], x[i], y[i]);
		return out;
	}

	/* the map size travels with the map, so a result describes its own grid */
	private LensShadingMap lensShadingMap() {
		float[] gains = floats("android.statistics.lensShadingMap");
		int[] size = ints("android.lens.info.shadingMapSize");

		if (gains == null || size == null || size.length < 2)
			return null;
		return new LensShadingMap(gains, size[1], size[0]);
	}

	private int[] ints(String name) {
		int tag = getTag(name);

		return tag == TAG_INVALID || getType(tag) < 0 ? null : readInts(tag);
	}

	private long[] longs(String name) {
		int tag = getTag(name);

		return tag == TAG_INVALID || getType(tag) < 0 ? null : readLongs(tag);
	}

	private float[] floats(String name) {
		int tag = getTag(name);

		return tag == TAG_INVALID || getType(tag) < 0 ? null : readFloats(tag);
	}

	private byte[] bytes(String name) {
		int tag = getTag(name);

		return tag == TAG_INVALID || getType(tag) < 0 ? null : readBytes(tag);
	}

	private Object raw(int tag, int nativeType) {
		switch (nativeType) {
		case TYPE_BYTE:
			return readBytes(tag);
		case TYPE_INT32:
		case TYPE_RATIONAL:
			return readInts(tag);
		case TYPE_FLOAT:
			return readFloats(tag);
		case TYPE_INT64:
			return readLongs(tag);
		case TYPE_DOUBLE:
			return readDoubles(tag);
		default:
			return null;
		}
	}

	private Pair<?, ?> pair(int tag, int nativeType, int index) {
		if (nativeType == TYPE_DOUBLE) {
			double[] v = readDoubles(tag);
			if (v == null || v.length < index * 2 + 2)
				return null;
			return new Pair<Double, Double>(Double.valueOf(v[index * 2]),
			    Double.valueOf(v[index * 2 + 1]));
		}
		float[] v = readFloats(tag);
		if (v == null || v.length < index * 2 + 2)
			return null;
		return new Pair<Float, Float>(Float.valueOf(v[index * 2]), Float.valueOf(v[index * 2 + 1]));
	}

	private Range<?> range(int tag, int nativeType, int index) {
		if (nativeType == TYPE_INT64) {
			long[] v = readLongs(tag);
			if (v == null || v.length < index * 2 + 2)
				return null;
			return new Range<Long>(Long.valueOf(v[index * 2]), Long.valueOf(v[index * 2 + 1]));
		}
		int[] v = readInts(tag);
		if (v == null || v.length < index * 2 + 2)
			return null;
		return new Range<Integer>(Integer.valueOf(v[index * 2]), Integer.valueOf(v[index * 2 + 1]));
	}

	private static String cString(byte[] v) {
		int end = 0;

		while (end < v.length && v[end] != 0)
			end++;
		return new String(v, 0, end, StandardCharsets.UTF_8);
	}

	private static native String[] native_getCameraIdList();
	private static native long native_getStaticMetadata(String cameraId);
	private static native int[] native_getAvailableKeys(String cameraId, int which);
	private static native long native_create();
	private static native long native_copy(long ptr);
	private static native void native_free(long ptr);
	private static native void native_erase(long ptr, int tag);
	private static native void native_writeLongs(long ptr, int tag, long[] values);
	private static native void native_writeDoubles(long ptr, int tag, double[] values);
	private static native void native_writeBytes(long ptr, int tag, byte[] values);
	private static native int[] native_getTags(long ptr);
	private static native int native_getTag(long ptr, String name);
	private static native String native_getTagName(long ptr, int tag);
	private static native int native_getType(long ptr, int tag);
	private static native byte[] native_readBytes(long ptr, int tag);
	private static native int[] native_readInts(long ptr, int tag);
	private static native float[] native_readFloats(long ptr, int tag);
	private static native long[] native_readLongs(long ptr, int tag);
	private static native double[] native_readDoubles(long ptr, int tag);
}
