package android.hardware.camera2;

/**
 * Why a capture produced no result: it was aborted before it took its frame
 * (REASON_FLUSHED), or the device failed while it was in flight
 * (REASON_ERROR). Either way no image came out, so wasImageCaptured() is false.
 */
public class CaptureFailure {
	public static final int REASON_ERROR = 0;
	public static final int REASON_FLUSHED = 1;

	private final CaptureRequest request;
	private final int reason;
	private final boolean dropped;
	private final int sequenceId;
	private final long frameNumber;

	CaptureFailure(CaptureRequest request, int reason, boolean dropped, int sequenceId, long frameNumber) {
		this.request = request;
		this.reason = reason;
		this.dropped = dropped;
		this.sequenceId = sequenceId;
		this.frameNumber = frameNumber;
	}

	public CaptureRequest getRequest() {
		return request;
	}

	public int getReason() {
		return reason;
	}

	public boolean wasImageCaptured() {
		return !dropped;
	}

	public int getSequenceId() {
		return sequenceId;
	}

	public long getFrameNumber() {
		return frameNumber;
	}

	public String getPhysicalCameraId() {
		return null;
	}
}
