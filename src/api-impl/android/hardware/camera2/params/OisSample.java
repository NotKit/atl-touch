package android.hardware.camera2.params;

/** One optical-image-stabilisation shift, in pixels, at a sensor timestamp. */
public final class OisSample {

	private final long timestamp;
	private final float xShift;
	private final float yShift;

	public OisSample(long timestamp, float xShift, float yShift) {
		this.timestamp = timestamp;
		this.xShift = xShift;
		this.yShift = yShift;
	}

	public long getTimestamp() {
		return timestamp;
	}

	public float getXshift() {
		return xShift;
	}

	public float getYshift() {
		return yShift;
	}

	@Override
	public boolean equals(Object other) {
		if (!(other instanceof OisSample))
			return false;
		OisSample o = (OisSample)other;
		return timestamp == o.timestamp && xShift == o.xShift && yShift == o.yShift;
	}

	@Override
	public int hashCode() {
		return (int)(timestamp ^ (timestamp >>> 32)) * 31 +
		    Float.floatToIntBits(xShift) * 31 + Float.floatToIntBits(yShift);
	}

	@Override
	public String toString() {
		return "OisSample(timestamp=" + timestamp + ", shift=(" + xShift + ", " + yShift + "))";
	}
}
