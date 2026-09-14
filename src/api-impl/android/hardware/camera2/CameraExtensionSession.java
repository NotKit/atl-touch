package android.hardware.camera2;

import android.util.Size;

import java.util.concurrent.Executor;

/**
 * A capture session running a vendor extension (night, bokeh, HDR).
 *
 * ATL has no extension implementation - CameraExtensionCharacteristics reports
 * none supported, so no session can be created. The class exists because apps
 * that build against SDK 31+ subclass ExtensionCaptureCallback while merely
 * deciding whether to use extensions, and a missing superclass takes them down
 * before they get to ask.
 */
public abstract class CameraExtensionSession implements AutoCloseable {

	public abstract CameraDevice getDevice();

	public abstract int capture(CaptureRequest request, Executor executor,
	                            ExtensionCaptureCallback listener) throws CameraAccessException;

	public abstract int setRepeatingRequest(CaptureRequest request, Executor executor,
	                                       ExtensionCaptureCallback listener) throws CameraAccessException;

	public abstract void stopRepeating() throws CameraAccessException;

	@Override
	public abstract void close() throws CameraAccessException;

	public StillCaptureLatency getRealtimeStillCaptureLatency() throws CameraAccessException {
		return null;
	}

	/** how long a still capture is expected to take, in milliseconds */
	public static final class StillCaptureLatency {
		private final long captureLatency;
		private final long processingLatency;

		public StillCaptureLatency(long captureLatency, long processingLatency) {
			this.captureLatency = captureLatency;
			this.processingLatency = processingLatency;
		}

		public long getCaptureLatency() {
			return captureLatency;
		}

		public long getProcessingLatency() {
			return processingLatency;
		}

		@Override
		public String toString() {
			return "StillCaptureLatency(" + captureLatency + "ms, " + processingLatency + "ms)";
		}
	}

	public static abstract class StateCallback {
		public abstract void onConfigured(CameraExtensionSession session);

		public abstract void onConfigureFailed(CameraExtensionSession session);

		public void onClosed(CameraExtensionSession session) {}
	}

	public static abstract class ExtensionCaptureCallback {
		public void onCaptureStarted(CameraExtensionSession session, CaptureRequest request,
		                             long timestamp) {}

		public void onCaptureProcessStarted(CameraExtensionSession session, CaptureRequest request) {}

		public void onCaptureFailed(CameraExtensionSession session, CaptureRequest request) {}

		public void onCaptureFailed(CameraExtensionSession session, CaptureRequest request,
		                            int failure) {}

		public void onCaptureSequenceCompleted(CameraExtensionSession session, int sequenceId) {}

		public void onCaptureSequenceAborted(CameraExtensionSession session, int sequenceId) {}

		public void onCaptureResultAvailable(CameraExtensionSession session, CaptureRequest request,
		                                     TotalCaptureResult result) {}

		public void onCaptureProcessProgressed(CameraExtensionSession session, CaptureRequest request,
		                                       int progress) {}
	}
}
