package android.hardware.camera2;

import android.hardware.camera2.impl.CameraMetadataNative;
import android.hardware.camera2.impl.HandlerExecutor;
import android.os.Handler;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executor;

/**
 * Camera enumeration on top of the camera backend's camera2 ops.
 *
 * A backend that cannot serve camera2 at all makes every call throw
 * CameraAccessException: an empty id list means "camera2 works, this device
 * just has no cameras", which is what webrtc's Camera2Enumerator.isSupported()
 * reads as success before it stops looking at android.hardware.Camera.
 */
public class CameraManager {

	public static abstract class AvailabilityCallback {
		public void onCameraAvailable(String cameraId) {
		}

		public void onCameraUnavailable(String cameraId) {
		}

		public void onCameraAccessPrioritiesChanged() {
		}
	}

	public static abstract class TorchCallback {
		public void onTorchModeUnavailable(String cameraId) {
		}

		public void onTorchModeChanged(String cameraId, boolean enabled) {
		}
	}

	/*
	 * Availability is per camera, not per manager: an app that opens the camera
	 * through one CameraManager and listens through another still has to hear
	 * that it went away, so the registry and the open set are static.
	 */
	private static final Map<AvailabilityCallback, Executor> availabilityCallbacks =
	    new LinkedHashMap<AvailabilityCallback, Executor>();
	private static final Set<String> openCameras = new HashSet<String>();

	/** ATL_DEBUG_CAMERA2: who listens for a camera coming and going. */
	private static final boolean DEBUG_LIFECYCLE = System.getenv("ATL_DEBUG_CAMERA2") != null;

	public String[] getCameraIdList() throws CameraAccessException {
		String[] ids = CameraMetadataNative.getCameraIdList();

		if (ids == null)
			throw new CameraAccessException(CameraAccessException.CAMERA_DISABLED,
			    "no camera backend serves camera2");
		return ids;
	}

	public String[] getCameraIdListNoLazy() throws CameraAccessException {
		return getCameraIdList();
	}

	public CameraCharacteristics getCameraCharacteristics(String cameraId)
	    throws CameraAccessException {
		if (cameraId == null)
			throw new IllegalArgumentException("cameraId must not be null");
		/* keeps "no camera2" distinguishable from "no such camera" */
		getCameraIdList();

		CameraMetadataNative metadata = CameraMetadataNative.getStaticMetadata(cameraId);
		if (metadata == null)
			throw new IllegalArgumentException("unknown camera id " + cameraId);
		return new CameraCharacteristics(cameraId, metadata);
	}

	public CameraExtensionCharacteristics getCameraExtensionCharacteristics(String cameraId)
	    throws CameraAccessException {
		getCameraCharacteristics(cameraId); /* rejects a camera that does not exist */
		return new CameraExtensionCharacteristics(cameraId);
	}

	public void openCamera(String cameraId, CameraDevice.StateCallback callback, Handler handler)
	    throws CameraAccessException {
		openCamera(cameraId, HandlerExecutor.of(handler), callback);
	}

	public void openCamera(String cameraId, Executor executor, CameraDevice.StateCallback callback)
	    throws CameraAccessException {
		if (callback == null)
			throw new IllegalArgumentException("callback must not be null");

		CameraCharacteristics characteristics = getCameraCharacteristics(cameraId);

		new CameraDevice(cameraId, characteristics, callback, executor).open();
	}

	/**
	 * Cameras never come and go under ATL, so registering only replays the
	 * current list. Without a handler the replay is synchronous, which is what
	 * the headless tests use.
	 */
	public void registerAvailabilityCallback(AvailabilityCallback callback, Handler handler) {
		register(callback, HandlerExecutor.of(handler));
	}

	public void registerAvailabilityCallback(Executor executor, AvailabilityCallback callback) {
		if (executor == null)
			throw new IllegalArgumentException("executor must not be null");
		register(callback, executor);
	}

	private void register(AvailabilityCallback callback, Executor executor) {
		if (callback == null)
			return;

		if (DEBUG_LIFECYCLE)
			System.err.println("camera2: availability callback registered by "
			    + callback.getClass().getName());
		synchronized (availabilityCallbacks) {
			availabilityCallbacks.put(callback, executor);
		}
		announce(callback, executor);
	}

	public void unregisterAvailabilityCallback(AvailabilityCallback callback) {
		synchronized (availabilityCallbacks) {
			availabilityCallbacks.remove(callback);
		}
	}

	/*
	 * A camera that is open is unavailable to everyone, and becomes available
	 * again when it is closed - the transition an app waits for before it opens
	 * the camera itself. ATL used to announce every camera as available once and
	 * never say anything again, and an app that closes the camera and waits to
	 * be told it may reopen (Google Camera does, on a mode switch) waited for
	 * ever.
	 */
	static void notifyOpened(String cameraId) {
		synchronized (availabilityCallbacks) {
			if (!openCameras.add(cameraId))
				return;
		}
		deliver(cameraId, false);
	}

	static void notifyClosed(String cameraId) {
		synchronized (availabilityCallbacks) {
			if (!openCameras.remove(cameraId))
				return;
		}
		deliver(cameraId, true);
	}

	private static void deliver(final String cameraId, final boolean available) {
		Map<AvailabilityCallback, Executor> listeners;

		synchronized (availabilityCallbacks) {
			listeners = new LinkedHashMap<AvailabilityCallback, Executor>(availabilityCallbacks);
		}
		if (DEBUG_LIFECYCLE)
			System.err.println("camera2: camera " + cameraId + " is now "
			    + (available ? "available" : "unavailable") + " to " + listeners.size()
			    + " listener(s)");
		for (Map.Entry<AvailabilityCallback, Executor> entry : listeners.entrySet()) {
			final AvailabilityCallback callback = entry.getKey();

			HandlerExecutor.run(entry.getValue(), new Runnable() {
				@Override
				public void run() {
					if (available)
						callback.onCameraAvailable(cameraId);
					else
						callback.onCameraUnavailable(cameraId);
				}
			});
		}
	}

	public void registerTorchCallback(TorchCallback callback, Handler handler) {
	}

	public void registerTorchCallback(Executor executor, TorchCallback callback) {
	}

	public void unregisterTorchCallback(TorchCallback callback) {
	}

	private void announce(final AvailabilityCallback callback, Executor executor) {
		String[] ids;

		try {
			ids = getCameraIdList();
		} catch (CameraAccessException e) {
			return; /* no camera2: nothing is available, and nothing changes */
		}

		for (String id : ids) {
			final String cameraId = id;
			final boolean available;

			synchronized (availabilityCallbacks) {
				available = !openCameras.contains(cameraId);
			}
			Runnable deliver = new Runnable() {
				@Override
				public void run() {
					if (available)
						callback.onCameraAvailable(cameraId);
					else
						callback.onCameraUnavailable(cameraId);
				}
			};
			HandlerExecutor.run(executor, deliver);
		}
	}


}
