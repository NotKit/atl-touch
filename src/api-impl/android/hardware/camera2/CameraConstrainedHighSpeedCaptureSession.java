package android.hardware.camera2;

import android.hardware.camera2.impl.CameraDeviceNative;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.Executor;

/**
 * A high speed session. No ATL backend has a constrained high speed mode, so
 * this is an ordinary session that still answers the high speed API: a request
 * list is the one request the app gave, since there is no batching to expand
 * it into.
 */
public class CameraConstrainedHighSpeedCaptureSession extends CameraCaptureSession {

	CameraConstrainedHighSpeedCaptureSession(CameraDevice device, CameraDeviceNative nativeDevice,
	    CameraCaptureSession.StateCallback callback, Executor executor) {
		super(device, nativeDevice, callback, executor);
	}

	public List<CaptureRequest> createHighSpeedRequestList(CaptureRequest request)
	    throws CameraAccessException {
		if (request == null)
			throw new IllegalArgumentException("request must not be null");
		if (request.getTargets().isEmpty())
			throw new IllegalArgumentException("a high speed request needs a target");
		return Collections.singletonList(request);
	}
}
