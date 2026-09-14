package android.hardware.camera2.params;

/** One physical camera's stream size inside a multi-resolution output. */
public final class MultiResolutionStreamInfo {

	private final int width;
	private final int height;
	private final String physicalCameraId;

	public MultiResolutionStreamInfo(int streamWidth, int streamHeight, String physicalCameraId) {
		if (physicalCameraId == null)
			throw new NullPointerException("physicalCameraId must not be null");
		this.width = streamWidth;
		this.height = streamHeight;
		this.physicalCameraId = physicalCameraId;
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	public String getPhysicalCameraId() {
		return physicalCameraId;
	}

	@Override
	public boolean equals(Object other) {
		if (!(other instanceof MultiResolutionStreamInfo))
			return false;
		MultiResolutionStreamInfo o = (MultiResolutionStreamInfo)other;
		return width == o.width && height == o.height && physicalCameraId.equals(o.physicalCameraId);
	}

	@Override
	public int hashCode() {
		return (width * 31 + height) * 31 + physicalCameraId.hashCode();
	}

	@Override
	public String toString() {
		return "MultiResolutionStreamInfo(" + physicalCameraId + " " + width + "x" + height + ")";
	}
}
