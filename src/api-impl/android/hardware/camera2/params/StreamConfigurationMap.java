package android.hardware.camera2.params;

import android.graphics.ImageFormat;
import android.util.Range;
import android.util.Size;
import android.view.Surface;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * The sizes, formats and durations one camera's streams can be configured
 * with, read straight from the characteristics' own tables.
 *
 * The three HAL tables are quadruple arrays keyed the same way — (format,
 * width, height, x) — so every query here is a scan of one of them. A class
 * (SurfaceTexture, MediaRecorder, ...) is an implementation-defined stream,
 * which is what the HAL calls PRIVATE.
 */
public final class StreamConfigurationMap {

	/* android.scaler.availableStreamConfigurations' fourth column */
	private static final int OUTPUT = 0;
	private static final int INPUT = 1;

	private final int[] configurations;
	private final long[] minFrameDurations;
	private final long[] stallDurations;
	private final Size[] highSpeedSizes;
	private final Range<Integer>[] highSpeedFpsRanges;

	/**
	 * Built by CameraCharacteristics out of the metadata bag; the tables are
	 * the HAL's, not copies with a different layout.
	 */
	public StreamConfigurationMap(int[] configurations, long[] minFrameDurations, long[] stallDurations,
	    Size[] highSpeedSizes, Range<Integer>[] highSpeedFpsRanges) {
		this.configurations = configurations == null ? new int[0] : configurations;
		this.minFrameDurations = minFrameDurations == null ? new long[0] : minFrameDurations;
		this.stallDurations = stallDurations == null ? new long[0] : stallDurations;
		this.highSpeedSizes = highSpeedSizes == null ? new Size[0] : highSpeedSizes;
		this.highSpeedFpsRanges = highSpeedFpsRanges == null ? emptyRanges() : highSpeedFpsRanges;
	}

	@SuppressWarnings("unchecked")
	private static Range<Integer>[] emptyRanges() {
		return (Range<Integer>[])new Range<?>[0];
	}

	public int[] getOutputFormats() {
		return formats(OUTPUT);
	}

	public int[] getInputFormats() {
		return formats(INPUT);
	}

	public int[] getValidOutputFormatsForInput(int inputFormat) {
		return isSupportedFor(inputFormat, INPUT) ? getOutputFormats() : new int[0];
	}

	public Size[] getOutputSizes(int format) {
		return sizes(format, OUTPUT);
	}

	public Size[] getInputSizes(int format) {
		return sizes(format, INPUT);
	}

	public <T> Size[] getOutputSizes(Class<T> klass) {
		return isOutputSupportedForClass(klass) ? sizes(ImageFormat.PRIVATE, OUTPUT) : null;
	}

	/** ATL has no second, higher-resolution configuration list. */
	public Size[] getHighResolutionOutputSizes(int format) {
		return new Size[0];
	}

	public boolean isOutputSupportedFor(int format) {
		return isSupportedFor(format, OUTPUT);
	}

	public static <T> boolean isOutputSupportedFor(Class<T> klass) {
		return isOutputSupportedForClass(klass);
	}

	public static boolean isOutputSupportedFor(Surface surface) {
		return surface != null && surface.isValid();
	}

	public long getOutputMinFrameDuration(int format, Size size) {
		return duration(minFrameDurations, format, size);
	}

	public <T> long getOutputMinFrameDuration(Class<T> klass, Size size) {
		return duration(minFrameDurations, ImageFormat.PRIVATE, size);
	}

	public long getOutputStallDuration(int format, Size size) {
		return duration(stallDurations, format, size);
	}

	public <T> long getOutputStallDuration(Class<T> klass, Size size) {
		return duration(stallDurations, ImageFormat.PRIVATE, size);
	}

	public Size[] getHighSpeedVideoSizes() {
		return highSpeedSizes.clone();
	}

	public Range<Integer>[] getHighSpeedVideoFpsRanges() {
		return highSpeedFpsRanges.clone();
	}

	public Range<Integer>[] getHighSpeedVideoFpsRangesFor(Size size) {
		for (Size candidate : highSpeedSizes)
			if (candidate.equals(size))
				return highSpeedFpsRanges.clone();
		throw new IllegalArgumentException(size + " is not a high speed video size");
	}

	public Size[] getHighSpeedVideoSizesFor(Range<Integer> fpsRange) {
		for (Range<Integer> candidate : highSpeedFpsRanges)
			if (candidate.equals(fpsRange))
				return highSpeedSizes.clone();
		throw new IllegalArgumentException(fpsRange + " is not a high speed fps range");
	}

	@Override
	public String toString() {
		return "StreamConfigurationMap(" + configurations.length / 4 + " configuration(s))";
	}

	/* ---- the tables ---- */

	private static <T> boolean isOutputSupportedForClass(Class<T> klass) {
		if (klass == null)
			return false;
		String name = klass.getName();
		return name.equals("android.graphics.SurfaceTexture") ||
		    name.equals("android.view.SurfaceHolder") ||
		    name.equals("android.view.Surface") ||
		    name.equals("android.media.MediaRecorder") ||
		    name.equals("android.media.MediaCodec") ||
		    name.equals("android.media.ImageReader");
	}

	private int[] formats(int direction) {
		Set<Integer> found = new LinkedHashSet<Integer>();

		for (int i = 0; i + 3 < configurations.length; i += 4)
			if (configurations[i + 3] == direction)
				found.add(Integer.valueOf(configurations[i]));

		int[] out = new int[found.size()];
		int at = 0;
		for (Integer format : found)
			out[at++] = format.intValue();
		return out;
	}

	private Size[] sizes(int format, int direction) {
		List<Size> found = new ArrayList<Size>();

		for (int i = 0; i + 3 < configurations.length; i += 4)
			if (configurations[i] == format && configurations[i + 3] == direction)
				found.add(new Size(configurations[i + 1], configurations[i + 2]));
		/* AOSP reports a format the camera cannot do as no list at all */
		return found.isEmpty() ? null : found.toArray(new Size[found.size()]);
	}

	private boolean isSupportedFor(int format, int direction) {
		for (int i = 0; i + 3 < configurations.length; i += 4)
			if (configurations[i] == format && configurations[i + 3] == direction)
				return true;
		return false;
	}

	/* -1 when the camera reports no duration for this stream */
	private long duration(long[] table, int format, Size size) {
		if (size == null)
			throw new IllegalArgumentException("size must not be null");
		for (int i = 0; i + 3 < table.length; i += 4)
			if (table[i] == format && table[i + 1] == size.getWidth() && table[i + 2] == size.getHeight())
				return table[i + 3];
		if (!isSupportedFor(format, OUTPUT))
			throw new IllegalArgumentException(size + " is not an output size for format " + format);
		return 0;
	}

	@Override
	public boolean equals(Object other) {
		return other instanceof StreamConfigurationMap &&
		    Arrays.equals(configurations, ((StreamConfigurationMap)other).configurations);
	}

	@Override
	public int hashCode() {
		return Arrays.hashCode(configurations);
	}
}
