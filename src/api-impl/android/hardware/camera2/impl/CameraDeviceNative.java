package android.hardware.camera2.impl;

import android.view.Surface;

import java.util.List;

/**
 * The native half of an open camera2 device: one backend session, its
 * configured outputs, and the request whose frames are being delivered.
 *
 * Every callback arrives on the main loop (the camera backend's own threads
 * never reach Java), so the listener runs where the app's Handler-less
 * callbacks are expected to run.
 */
public final class CameraDeviceNative {

	public interface Listener {
		void onCaptureStarted(int requestId, long frameNumber, long timestamp);
		/** the result is owned by the listener from here on */
		void onCaptureCompleted(int requestId, long frameNumber, CameraMetadataNative result);
		void onCaptureFailed(int requestId, long frameNumber);
		/* stream is the index of the output, in the order configure() was given */
		void onCaptureBufferLost(int requestId, long frameNumber, int stream);
		void onDeviceError(int error);
	}

	private final Listener listener;
	private long nativePtr;

	public CameraDeviceNative(Listener listener) {
		this.listener = listener;
	}

	/** false when the backend cannot open this camera. */
	public boolean open(String cameraId) {
		nativePtr = native_open(cameraId, this);
		return nativePtr != 0;
	}

	/**
	 * Points the session at its outputs. Returns the size the single backend
	 * stream was configured with (the largest output's), or null if one of the
	 * surfaces is not something the backend can fill.
	 */
	/* physicalIds has one entry per output, null for the logical camera */
	public int[] configure(List<Surface> outputs, String[] physicalIds) {
		if (nativePtr == 0)
			return null;
		return native_configure(nativePtr, outputs.toArray(new Surface[outputs.size()]), physicalIds);
	}

	/**
	 * targets is a bit per configured output, in the order configure() was
	 * given them: a frame only reaches the surfaces its request aimed at.
	 */
	public boolean setRepeatingRequest(int requestId, CameraMetadataNative request, int targets) {
		return nativePtr != 0 && native_setRepeatingRequest(nativePtr, requestId, request.getPtr(), targets);
	}

	public boolean capture(int requestId, CameraMetadataNative request, int targets) {
		return nativePtr != 0 && native_capture(nativePtr, requestId, request.getPtr(), targets);
	}

	/**
	 * Gives the session an input queue and points the Surface at it. The
	 * images an ImageWriter puts in there are what a reprocess capture takes
	 * its frame from.
	 */
	public boolean createInputSurface(Surface surface, int width, int height, int format,
	    boolean multiResolution) {
		return nativePtr != 0 &&
		    native_createInputSurface(nativePtr, surface, width, height, format, multiResolution);
	}

	/** Drops the input a previous session was given, if any. */
	public void clearInputSurface() {
		if (nativePtr != 0)
			native_clearInputSurface(nativePtr);
	}

	/** false when the input queue is empty: nothing to reprocess. */
	public boolean reprocess(int requestId, CameraMetadataNative request, int targets) {
		return nativePtr != 0 && native_reprocess(nativePtr, requestId, request.getPtr(), targets);
	}

	public void stopRepeating() {
		if (nativePtr != 0)
			native_stopRepeating(nativePtr);
	}

	/** the ids of the one-shot requests that were dropped before taking a frame */
	public int[] abortCaptures() {
		int[] aborted = nativePtr == 0 ? null : native_abortCaptures(nativePtr);

		return aborted == null ? new int[0] : aborted;
	}

	public synchronized void close() {
		if (nativePtr != 0) {
			native_close(nativePtr);
			nativePtr = 0;
		}
	}

	/* called from native, on the main loop */
	private void dispatchCaptureStarted(int requestId, long frameNumber, long timestamp) {
		listener.onCaptureStarted(requestId, frameNumber, timestamp);
	}

	private void dispatchCaptureCompleted(int requestId, long frameNumber, long resultPtr) {
		listener.onCaptureCompleted(requestId, frameNumber, CameraMetadataNative.adopt(resultPtr));
	}

	private void dispatchCaptureFailed(int requestId, long frameNumber) {
		listener.onCaptureFailed(requestId, frameNumber);
	}

	private void dispatchCaptureBufferLost(int requestId, long frameNumber, int stream) {
		listener.onCaptureBufferLost(requestId, frameNumber, stream);
	}

	private void dispatchError(int error) {
		listener.onDeviceError(error);
	}

	private static native long native_open(String cameraId, CameraDeviceNative self);
	private static native int[] native_configure(long ptr, Surface[] surfaces, String[] physicalIds);
	private static native boolean native_setRepeatingRequest(long ptr, int requestId, long requestPtr,
	    int targets);
	private static native boolean native_capture(long ptr, int requestId, long requestPtr, int targets);
	private static native boolean native_createInputSurface(long ptr, Surface surface, int width,
	    int height, int format, boolean multiResolution);
	private static native void native_clearInputSurface(long ptr);
	private static native boolean native_reprocess(long ptr, int requestId, long requestPtr, int targets);
	private static native void native_stopRepeating(long ptr);
	private static native int[] native_abortCaptures(long ptr);
	private static native void native_close(long ptr);
}
