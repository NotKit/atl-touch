package android.hardware.camera2.params;

import java.util.HashMap;
import java.util.Map;

/**
 * The sensor orientation a foldable reports in each of its device states.
 *
 * Built from android.info.deviceStateOrientations, whose entries are
 * (device state bitmask, orientation in degrees) long pairs.
 */
public final class DeviceStateSensorOrientationMap {

	public static final long NORMAL = 0;
	public static final long FOLDED = 4;

	private final Map<Long, Integer> orientations = new HashMap<Long, Integer>();

	public DeviceStateSensorOrientationMap(long[] elements) {
		if (elements == null)
			return;
		for (int i = 0; i + 1 < elements.length; i += 2)
			orientations.put(Long.valueOf(elements[i]), Integer.valueOf((int)elements[i + 1]));
	}

	/** -1 for a state the camera says nothing about, which is what AOSP returns. */
	public int getSensorOrientation(long deviceState) {
		Integer orientation = orientations.get(Long.valueOf(deviceState));

		return orientation == null ? -1 : orientation.intValue();
	}

	@Override
	public String toString() {
		return "DeviceStateSensorOrientationMap(" + orientations + ")";
	}
}
