package android.hardware.camera2;

import android.hardware.camera2.impl.CameraDeviceNative;
import android.hardware.camera2.impl.CameraMetadataNative;
import android.hardware.camera2.impl.HandlerExecutor;
import android.hardware.camera2.params.InputConfiguration;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.SessionConfiguration;
import android.os.Handler;
import android.view.Surface;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;

/**
 * An open camera: one backend session, and at most one capture session on it.
 *
 * Unlike AOSP this is not an abstract class with a hidden implementation —
 * there is only ever one implementation, and apps only ever hold the type.
 * State callbacks are delivered through the app's Handler, or inline when it
 * did not give one (the same rule CameraManager's availability replay uses).
 */
public class CameraDevice implements AutoCloseable {

	public static final int TEMPLATE_PREVIEW = 1;
	public static final int TEMPLATE_STILL_CAPTURE = 2;
	public static final int TEMPLATE_RECORD = 3;
	public static final int TEMPLATE_VIDEO_SNAPSHOT = 4;
	public static final int TEMPLATE_ZERO_SHUTTER_LAG = 5;
	public static final int TEMPLATE_MANUAL = 6;

	public static final int AUDIO_RESTRICTION_NONE = 0;
	public static final int AUDIO_RESTRICTION_VIBRATION = 1;
	public static final int AUDIO_RESTRICTION_VIBRATION_SOUND = 3;

	public static abstract class StateCallback {
		public static final int ERROR_CAMERA_IN_USE = 1;
		public static final int ERROR_MAX_CAMERAS_IN_USE = 2;
		public static final int ERROR_CAMERA_DISABLED = 3;
		public static final int ERROR_CAMERA_DEVICE = 4;
		public static final int ERROR_CAMERA_SERVICE = 5;

		public abstract void onOpened(CameraDevice camera);

		public abstract void onDisconnected(CameraDevice camera);

		public abstract void onError(CameraDevice camera, int error);

		public void onClosed(CameraDevice camera) {
		}
	}

	private final String cameraId;
	private final CameraCharacteristics characteristics;
	private final StateCallback stateCallback;
	private final Executor executor;

	/** ATL_DEBUG_CAMERA2: who opens, configures and closes the camera. */
	private static final boolean DEBUG_LIFECYCLE = System.getenv("ATL_DEBUG_CAMERA2") != null;

	private CameraDeviceNative device;
	private CameraCaptureSession session;
	private boolean closed;
	private int audioRestriction = AUDIO_RESTRICTION_NONE;

	CameraDevice(String cameraId, CameraCharacteristics characteristics,
	    StateCallback callback, Executor executor) {
		this.cameraId = cameraId;
		this.characteristics = characteristics;
		this.stateCallback = callback;
		this.executor = executor;
	}

	/* the backend session opens synchronously; only the callback is deferred */
	void open() {
		final CameraDevice self = this;

		device = new CameraDeviceNative(listener);
		if (!device.open(cameraId)) {
			device = null;
			HandlerExecutor.run(executor, new Runnable() {
				@Override
				public void run() {
					stateCallback.onError(self, StateCallback.ERROR_CAMERA_DEVICE);
				}
			});
			return;
		}
		CameraManager.notifyOpened(cameraId);
		HandlerExecutor.run(executor, new Runnable() {
			@Override
			public void run() {
				stateCallback.onOpened(self);
			}
		});
	}

	public String getId() {
		return cameraId;
	}

	CameraCharacteristics getCharacteristics() {
		return characteristics;
	}

	/**
	 * The settings a HAL template would carry. ATL fills them from the camera's
	 * own characteristics rather than from a HAL-provided template, so a mode
	 * the camera does not have is never requested.
	 */
	public CaptureRequest.Builder createCaptureRequest(int templateType) throws CameraAccessException {
		checkOpen();
		if (templateType < TEMPLATE_PREVIEW || templateType > TEMPLATE_MANUAL)
			throw new IllegalArgumentException("unknown template " + templateType);

		CameraMetadataNative settings = CameraMetadataNative.create();

		if (templateType != TEMPLATE_MANUAL) {
			settings.set("android.control.mode", CameraMetadata.CONTROL_MODE_AUTO);
			settings.set("android.control.aeMode", CameraMetadata.CONTROL_AE_MODE_ON);
			settings.set("android.control.awbMode", CameraMetadata.CONTROL_AWB_MODE_AUTO);
			settings.set("android.control.afMode", defaultAfMode(templateType));
		} else {
			settings.set("android.control.mode", CameraMetadata.CONTROL_MODE_OFF);
			settings.set("android.control.aeMode", CameraMetadata.CONTROL_AE_MODE_OFF);
			settings.set("android.control.awbMode", CameraMetadata.CONTROL_AWB_MODE_OFF);
			settings.set("android.control.afMode", CameraMetadata.CONTROL_AF_MODE_OFF);
		}
		settings.set("android.control.captureIntent", captureIntent(templateType));
		return new CaptureRequest.Builder(settings);
	}

	private int defaultAfMode(int templateType) {
		int wanted = templateType == TEMPLATE_RECORD || templateType == TEMPLATE_VIDEO_SNAPSHOT
		    ? CameraMetadata.CONTROL_AF_MODE_CONTINUOUS_VIDEO
		    : CameraMetadata.CONTROL_AF_MODE_CONTINUOUS_PICTURE;
		int[] available = characteristics.get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES);

		if (available != null)
			for (int mode : available)
				if (mode == wanted)
					return wanted;
		return CameraMetadata.CONTROL_AF_MODE_OFF;
	}

	private static int captureIntent(int templateType) {
		switch (templateType) {
		case TEMPLATE_STILL_CAPTURE:
			return CameraMetadata.CONTROL_CAPTURE_INTENT_STILL_CAPTURE;
		case TEMPLATE_RECORD:
			return CameraMetadata.CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
		case TEMPLATE_VIDEO_SNAPSHOT:
			return CameraMetadata.CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT;
		case TEMPLATE_ZERO_SHUTTER_LAG:
			return CameraMetadata.CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG;
		case TEMPLATE_MANUAL:
			return CameraMetadata.CONTROL_CAPTURE_INTENT_MANUAL;
		default:
			return CameraMetadata.CONTROL_CAPTURE_INTENT_PREVIEW;
		}
	}

	public void createCaptureSession(List<Surface> outputs,
	    CameraCaptureSession.StateCallback callback, Handler handler) throws CameraAccessException {
		configureSession(outputs, null, callback, HandlerExecutor.of(handler), false);
	}

	public void createCaptureSessionByOutputConfigurations(List<OutputConfiguration> outputConfigurations,
	    CameraCaptureSession.StateCallback callback, Handler handler) throws CameraAccessException {
		configureSession(surfacesOf(outputConfigurations), outputConfigurations, callback,
		    HandlerExecutor.of(handler), false);
	}

	public void createCaptureSession(SessionConfiguration config) throws CameraAccessException {
		if (config == null)
			throw new IllegalArgumentException("config must not be null");
		List<OutputConfiguration> outputs = config.getOutputConfigurations();
		configureSession(surfacesOf(outputs), outputs, config.getStateCallback(),
		    config.getExecutor(), config.getSessionType() == SessionConfiguration.SESSION_HIGH_SPEED,
		    config.getInputConfiguration());
	}

	/** Nothing ATL configures fails after the fact, so a legal configuration is supported. */
	public boolean isSessionConfigurationSupported(SessionConfiguration config)
	    throws CameraAccessException {
		if (config == null)
			throw new IllegalArgumentException("config must not be null");
		return !config.getOutputConfigurations().isEmpty();
	}

	public void createConstrainedHighSpeedCaptureSession(List<Surface> outputs,
	    CameraCaptureSession.StateCallback callback, Handler handler) throws CameraAccessException {
		configureSession(outputs, null, callback, HandlerExecutor.of(handler), true);
	}

	public void createReprocessableCaptureSession(InputConfiguration inputConfig, List<Surface> outputs,
	    CameraCaptureSession.StateCallback callback, Handler handler) throws CameraAccessException {
		configureSession(outputs, null, callback, HandlerExecutor.of(handler), false, inputConfig);
	}

	public void createReprocessableCaptureSessionByConfigurations(InputConfiguration inputConfig,
	    List<OutputConfiguration> outputs, CameraCaptureSession.StateCallback callback, Handler handler)
	    throws CameraAccessException {
		configureSession(surfacesOf(outputs), outputs, callback, HandlerExecutor.of(handler), false,
		    inputConfig);
	}

	/**
	 * The settings of the capture whose image is being reprocessed, so the
	 * reprocessed result describes the frame it came from rather than the
	 * camera's state now.
	 */
	public CaptureRequest.Builder createReprocessCaptureRequest(TotalCaptureResult inputResult)
	    throws CameraAccessException {
		checkOpen();
		if (inputResult == null)
			throw new IllegalArgumentException("inputResult must not be null");
		if (session == null || !session.isReprocessable())
			throw new IllegalArgumentException("this camera has no reprocessing session");

		return new CaptureRequest.Builder(inputResult.copySettings(), true);
	}

	/** ATL has no audio to restrict, but an app may set and read the mode back. */
	public void setCameraAudioRestriction(int mode) throws CameraAccessException {
		if (mode != AUDIO_RESTRICTION_NONE && mode != AUDIO_RESTRICTION_VIBRATION &&
		    mode != AUDIO_RESTRICTION_VIBRATION_SOUND)
			throw new IllegalArgumentException("unknown audio restriction " + mode);
		audioRestriction = mode;
	}

	public int getCameraAudioRestriction() throws CameraAccessException {
		return audioRestriction;
	}

	private static List<Surface> surfacesOf(List<OutputConfiguration> outputConfigurations) {
		List<Surface> surfaces = new ArrayList<Surface>();

		if (outputConfigurations == null)
			return surfaces;
		for (OutputConfiguration configuration : outputConfigurations)
			surfaces.addAll(configuration.getSurfaces());
		return surfaces;
	}

	private void configureSession(List<Surface> outputs, List<OutputConfiguration> configurations,
	    CameraCaptureSession.StateCallback callback, Executor sessionExecutor, boolean highSpeed)
	    throws CameraAccessException {
		configureSession(outputs, configurations, callback, sessionExecutor, highSpeed, null);
	}

	/* the one path every createXxxSession() ends up in */
	private void configureSession(List<Surface> outputs, List<OutputConfiguration> configurations,
	    CameraCaptureSession.StateCallback callback, Executor sessionExecutor, boolean highSpeed,
	    InputConfiguration inputConfig) throws CameraAccessException {
		checkOpen();
		if (outputs == null || outputs.isEmpty())
			throw new IllegalArgumentException("a capture session needs at least one output");
		if (callback == null)
			throw new IllegalArgumentException("callback must not be null");

		if (DEBUG_LIFECYCLE)
			trace("configureSession of camera " + cameraId + " with " + outputs.size()
			    + " output(s)" + (inputConfig != null ? " and an input" : ""));
		if (session != null)
			session.closeWithoutCallback();
		session = highSpeed
		    ? new CameraConstrainedHighSpeedCaptureSession(this, device, callback, sessionExecutor)
		    : new CameraCaptureSession(this, device, callback, sessionExecutor);
		session.setOutputConfigurations(configurations);
		session.setInputConfiguration(inputConfig);
		session.configure(outputs);
	}

	@Override
	public void close() {
		if (closed)
			return;
		closed = true;
		if (DEBUG_LIFECYCLE)
			trace("close of camera " + cameraId);

		if (session != null) {
			session.closeWithoutCallback();
			session = null;
		}
		if (device != null) {
			device.close();
			device = null;
			CameraManager.notifyClosed(cameraId);
		}
		final CameraDevice self = this;
		if (DEBUG_LIFECYCLE)
			System.err.println("camera2: onClosed posted for camera " + cameraId
			    + (executor == null ? " (inline: no executor)" : ""));
		HandlerExecutor.run(executor, new Runnable() {
			@Override
			public void run() {
				if (DEBUG_LIFECYCLE)
					System.err.println("camera2: onClosed delivered for camera " + cameraId
					    + " on " + Thread.currentThread().getName());
				stateCallback.onClosed(self);
			}
		});
	}

	boolean isClosed() {
		return closed;
	}

	/* the app's own frames, so a teardown nobody asked for can be told from one
	 * the app's state machine drove */
	private static void trace(String what) {
		StringBuilder text = new StringBuilder("camera2: ").append(what);

		for (StackTraceElement frame : new Throwable().getStackTrace())
			text.append("\n    ").append(frame);
		System.err.println(text);
	}

	private void checkOpen() throws CameraAccessException {
		if (closed || device == null)
			throw new IllegalStateException("CameraDevice " + cameraId + " is closed");
	}

	@Override
	public String toString() {
		return "CameraDevice(" + cameraId + ")";
	}

	/* backend events; they arrive on the main loop, not on a backend thread */
	private final CameraDeviceNative.Listener listener = new CameraDeviceNative.Listener() {
		@Override
		public void onCaptureStarted(int requestId, long frameNumber, long timestamp) {
			CameraCaptureSession current = session;

			if (android.media.ImageReader.DEBUG)
				android.util.Log.i("ImageReaderDbg", "capture started: request " + requestId + " frame " + frameNumber + " ts=" + timestamp);
			if (current != null)
				current.dispatchCaptureStarted(requestId, frameNumber, timestamp);
		}

		@Override
		public void onCaptureCompleted(int requestId, long frameNumber, CameraMetadataNative result) {
			CameraCaptureSession current = session;

			if (android.media.ImageReader.DEBUG)
				android.util.Log.i("ImageReaderDbg", "capture completed: request " + requestId + " frame " + frameNumber
				    + " ts=" + (result != null ? result.get("android.sensor.timestamp", Long.class) : null));
			if (current != null)
				current.dispatchCaptureCompleted(requestId, frameNumber, result);
			else if (result != null)
				result.close();
		}

		@Override
		public void onCaptureFailed(int requestId, long frameNumber) {
			CameraCaptureSession current = session;

			if (current != null)
				current.dispatchCaptureFailed(requestId, frameNumber);
		}

		@Override
		public void onCaptureBufferLost(int requestId, long frameNumber, int stream) {
			CameraCaptureSession current = session;

			if (current != null)
				current.dispatchCaptureBufferLost(requestId, frameNumber, stream);
		}

		@Override
		public void onDeviceError(final int error) {
			final CameraDevice self = CameraDevice.this;
			CameraCaptureSession current = session;

			/* whatever was in flight will never produce a frame now */
			if (current != null)
				current.dispatchDeviceError();
			HandlerExecutor.run(executor, new Runnable() {
				@Override
				public void run() {
					stateCallback.onError(self, StateCallback.ERROR_CAMERA_DEVICE);
				}
			});
		}
	};
}
