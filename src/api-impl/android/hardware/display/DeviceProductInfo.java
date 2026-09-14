package android.hardware.display;

/**
 * What a display says about itself over EDID. ATL's display is the host
 * window, which has no such identity, so the sink is "internal" and every
 * other field is unknown - what apps do with this is decide whether they are
 * on a built-in panel or an external screen.
 */
public final class DeviceProductInfo {

	public static final int CONNECTION_TO_SINK_UNKNOWN = 0;
	public static final int CONNECTION_TO_SINK_BUILT_IN = 1;
	public static final int CONNECTION_TO_SINK_DIRECT = 2;
	public static final int CONNECTION_TO_SINK_TRANSITIVE = 3;

	public String getName() {
		return null;
	}

	public String getManufacturerPnpId() {
		return null;
	}

	public String getProductId() {
		return null;
	}

	public int getModelYear() {
		return -1;
	}

	public int getConnectionToSinkType() {
		return CONNECTION_TO_SINK_BUILT_IN;
	}
}
