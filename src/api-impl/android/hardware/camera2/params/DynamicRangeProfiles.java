package android.hardware.camera2.params;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** The dynamic range profiles a camera can output; ATL's backends are SDR only. */
public final class DynamicRangeProfiles {

	public static final long STANDARD = 0x1;
	public static final long HLG10 = 0x2;
	public static final long HDR10 = 0x4;
	public static final long HDR10_PLUS = 0x8;
	public static final long DOLBY_VISION_10B_HDR_REF = 0x10;
	public static final long DOLBY_VISION_8B_HDR_REF = 0x400;
	public static final long PUBLIC_MAX = 0x1000;

	public Set<Long> getSupportedProfiles() {
		return Collections.singleton(Long.valueOf(STANDARD));
	}

	public Set<Long> getProfileCaptureRequestConstraints(long profile) {
		return new HashSet<Long>();
	}

	public boolean isExtraLatencyPresent(long profile) {
		return false;
	}
}
