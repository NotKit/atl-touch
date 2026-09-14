package android.hardware.camera2;

import android.hardware.camera2.impl.CameraDeviceNative;
import android.hardware.camera2.impl.CameraMetadataNative;
import android.hardware.camera2.impl.HandlerExecutor;
import android.hardware.camera2.params.InputConfiguration;
import android.hardware.camera2.params.OutputConfiguration;
import android.os.Handler;
import android.view.Surface;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executor;

/**
 * A configured set of outputs and the requests running against them.
 *
 * An output is a SurfaceTexture-backed Surface (a preview) or an
 * ImageReader-backed one (pixels the app reads); they all see the same
 * frames. Each submitted request gets a sequence id, which is what the backend
 * reports frames against, so results find their way back to the right
 * CaptureCallback.
 */
public class CameraCaptureSession implements AutoCloseable {

	public static abstract class StateCallback {
		public abstract void onConfigured(CameraCaptureSession session);

		public abstract void onConfigureFailed(CameraCaptureSession session);

		public void onReady(CameraCaptureSession session) {
		}

		public void onActive(CameraCaptureSession session) {
		}

		public void onCaptureQueueEmpty(CameraCaptureSession session) {
		}

		public void onClosed(CameraCaptureSession session) {
		}

		public void onSurfacePrepared(CameraCaptureSession session, Surface surface) {
		}
	}

	public static abstract class CaptureCallback {
		public void onCaptureStarted(CameraCaptureSession session, CaptureRequest request,
		    long timestamp, long frameNumber) {
		}

		public void onCaptureProgressed(CameraCaptureSession session, CaptureRequest request,
		    CaptureResult partialResult) {
		}

		public void onCaptureCompleted(CameraCaptureSession session, CaptureRequest request,
		    TotalCaptureResult result) {
		}

		public void onCaptureFailed(CameraCaptureSession session, CaptureRequest request,
		    CaptureFailure failure) {
		}

		public void onCaptureSequenceCompleted(CameraCaptureSession session, int sequenceId,
		    long frameNumber) {
		}

		public void onCaptureSequenceAborted(CameraCaptureSession session, int sequenceId) {
		}

		public void onCaptureBufferLost(CameraCaptureSession session, CaptureRequest request,
		    Surface target, long frameNumber) {
		}
	}

	/* one submitted request and where its results go */
	private static final class Sequence {
		final CaptureRequest request;
		final CaptureCallback callback;
		final Executor executor;
		final boolean repeating;
		long lastFrameNumber = -1;

		Sequence(CaptureRequest request, CaptureCallback callback, Executor executor, boolean repeating) {
			this.request = request;
			this.callback = callback;
			this.executor = executor;
			this.repeating = repeating;
		}
	}

	private final CameraDevice device;
	private final CameraDeviceNative nativeDevice;
	private final StateCallback stateCallback;
	private final Executor executor;

	private final Map<Integer, Sequence> sequences = new HashMap<Integer, Sequence>();
	private List<Surface> outputs;
	private List<OutputConfiguration> outputConfigurations;
	private InputConfiguration inputConfiguration;
	private Surface inputSurface;
	private int nextSequenceId = 1;
	private int repeatingId = -1;
	private boolean closed;

	CameraCaptureSession(CameraDevice device, CameraDeviceNative nativeDevice,
	    StateCallback callback, Executor executor) {
		this.device = device;
		this.nativeDevice = nativeDevice;
		this.stateCallback = callback;
		this.executor = executor;
	}

	/* the configurations this session was created from, if it was created that way */
	void setOutputConfigurations(List<OutputConfiguration> configurations) {
		outputConfigurations = configurations;
	}

	/* a reprocessing session's input stream, created before the outputs so
	 * getInputSurface() is valid as soon as onConfigured runs */
	void setInputConfiguration(InputConfiguration configuration) {
		inputConfiguration = configuration;
		inputSurface = null;
		if (configuration == null) {
			/* the device keeps the input across configurations, so an ordinary
			 * session after a reprocessable one has to say it wants none */
			nativeDevice.clearInputSurface();
			return;
		}

		Surface surface = new Surface();
		if (!nativeDevice.createInputSurface(surface, configuration.getWidth(),
		        configuration.getHeight(), configuration.getFormat(),
		        configuration.isMultiResolution()))
			throw new IllegalArgumentException("this camera cannot reprocess " + configuration);
		inputSurface = surface;
	}

	/* one stream per output; a configuration's physical camera id goes with it */
	void configure(List<Surface> outputs) {
		String[] physicalIds = new String[outputs.size()];

		if (outputConfigurations != null) {
			for (OutputConfiguration configuration : outputConfigurations) {
				String physicalId = configuration.getPhysicalCameraId();

				if (physicalId == null)
					continue;
				for (Surface surface : configuration.getSurfaces()) {
					int index = outputs.indexOf(surface);

					if (index >= 0)
						physicalIds[index] = physicalId;
				}
			}
		}
		if (nativeDevice.configure(outputs, physicalIds) == null) {
			postState(new Runnable() {
				@Override
				public void run() {
					stateCallback.onConfigureFailed(CameraCaptureSession.this);
				}
			});
			return;
		}
		this.outputs = new ArrayList<Surface>(outputs);
		postState("onConfigured", new Runnable() {
			@Override
			public void run() {
				stateCallback.onConfigured(CameraCaptureSession.this);
				stateCallback.onReady(CameraCaptureSession.this);
			}
		});
	}

	public CameraDevice getDevice() {
		return device;
	}

	public boolean isReprocessable() {
		return inputSurface != null;
	}

	public Surface getInputSurface() {
		return inputSurface;
	}

	public int capture(CaptureRequest request, CaptureCallback listener, Handler handler)
	    throws CameraAccessException {
		checkOpen();
		if (request != null && request.isReprocess())
			return reprocess(request, listener, handler);

		int id = submit(request, listener, handler, false);

		if (!nativeDevice.capture(id, request.getSettings(), targetMask(request))) {
			forget(id);
			throw new CameraAccessException(CameraAccessException.CAMERA_ERROR,
			    "the camera backend refused the capture");
		}
		return id;
	}

	/**
	 * A reprocess capture takes the image the app queued on the input surface
	 * rather than a frame from the camera, so it completes even with no
	 * repeating request running.
	 */
	private int reprocess(CaptureRequest request, CaptureCallback listener, Handler handler)
	    throws CameraAccessException {
		if (inputSurface == null)
			throw new IllegalArgumentException("this session has no reprocessing input");

		int id = submit(request, listener, handler, false);

		if (!nativeDevice.reprocess(id, request.getSettings(), targetMask(request))) {
			forget(id);
			throw new CameraAccessException(CameraAccessException.CAMERA_ERROR,
			    "no image queued on the reprocessing input");
		}
		return id;
	}

	/** Each request of the burst is submitted in turn; the last id is returned. */
	public int captureBurst(List<CaptureRequest> requests, CaptureCallback listener, Handler handler)
	    throws CameraAccessException {
		checkOpen();
		if (requests == null || requests.isEmpty())
			throw new IllegalArgumentException("a burst needs at least one request");

		int id = 0;
		for (CaptureRequest request : requests)
			id = capture(request, listener, handler);
		return id;
	}

	public int setRepeatingRequest(CaptureRequest request, CaptureCallback listener, Handler handler)
	    throws CameraAccessException {
		checkOpen();
		int id = submit(request, listener, handler, true);

		synchronized (sequences) {
			if (repeatingId >= 0)
				sequences.remove(repeatingId);
			repeatingId = id;
		}
		if (!nativeDevice.setRepeatingRequest(id, request.getSettings(), targetMask(request))) {
			forget(id);
			throw new CameraAccessException(CameraAccessException.CAMERA_ERROR,
			    "the camera backend refused the repeating request");
		}
		postState(new Runnable() {
			@Override
			public void run() {
				stateCallback.onActive(CameraCaptureSession.this);
			}
		});
		return id;
	}

	/** No burst repeats under ATL; the first request stands in for the list. */
	public int setRepeatingBurst(List<CaptureRequest> requests, CaptureCallback listener, Handler handler)
	    throws CameraAccessException {
		if (requests == null || requests.isEmpty())
			throw new IllegalArgumentException("a burst needs at least one request");
		return setRepeatingRequest(requests.get(0), listener, handler);
	}

	public void stopRepeating() throws CameraAccessException {
		checkOpen();
		stopRepeatingInternal();
		postState(new Runnable() {
			@Override
			public void run() {
				stateCallback.onReady(CameraCaptureSession.this);
			}
		});
	}

	/**
	 * Drops everything in flight. A one-shot capture that had not taken its
	 * frame yet is reported as a failure — the frame it was waiting for is
	 * gone, and the app would otherwise wait for a result that never comes.
	 */
	public void abortCaptures() throws CameraAccessException {
		checkOpen();
		Set<Integer> failedIds = new HashSet<Integer>();

		for (int id : nativeDevice.abortCaptures())
			failedIds.add(Integer.valueOf(id));
		stopRepeatingInternal();

		List<Map.Entry<Integer, Sequence>> aborted;
		synchronized (sequences) {
			aborted = new ArrayList<Map.Entry<Integer, Sequence>>(sequences.entrySet());
			sequences.clear();
		}
		for (Map.Entry<Integer, Sequence> entry : aborted) {
			final int sequenceId = entry.getKey().intValue();
			final Sequence sequence = entry.getValue();

			if (sequence.callback == null)
				continue;
			final boolean failed = failedIds.contains(Integer.valueOf(sequenceId));
			HandlerExecutor.run(sequence.executor, new Runnable() {
				@Override
				public void run() {
					if (failed)
						sequence.callback.onCaptureFailed(CameraCaptureSession.this,
						    sequence.request, new CaptureFailure(sequence.request,
						        CaptureFailure.REASON_FLUSHED, true, sequenceId,
						        sequence.lastFrameNumber));
					sequence.callback.onCaptureSequenceAborted(CameraCaptureSession.this, sequenceId);
				}
			});
		}
		postState(new Runnable() {
			@Override
			public void run() {
				stateCallback.onReady(CameraCaptureSession.this);
			}
		});
	}

	/**
	 * Deferred outputs are already part of the session here, since ATL
	 * configures the backend stream from the surfaces it was given; this only
	 * refuses a configuration the session was not created with.
	 */
	public void finalizeOutputConfigurations(List<OutputConfiguration> deferredOutputs)
	    throws CameraAccessException {
		checkOpen();
		if (deferredOutputs == null || deferredOutputs.isEmpty())
			throw new IllegalArgumentException("finalizeOutputConfigurations needs an output");

		for (OutputConfiguration configuration : deferredOutputs) {
			for (Surface surface : configuration.getSurfaces())
				if (outputs == null || !outputs.contains(surface))
					throw new IllegalArgumentException(
					    "the session was not configured with that surface");
			if (configuration.getSurfaces().isEmpty())
				throw new IllegalArgumentException("the configuration still has no surface");
		}
	}

	public void prepare(Surface surface) throws CameraAccessException {
		checkOpen();
		final Surface prepared = surface;
		postState(new Runnable() {
			@Override
			public void run() {
				stateCallback.onSurfacePrepared(CameraCaptureSession.this, prepared);
			}
		});
	}

	@Override
	public void close() {
		closeInternal();
	}

	/*
	 * A session is closed when the app closes it, when the device configures
	 * another one over it, and when the device itself closes - and onClosed is
	 * reported in all three, which is what StateCallback.onClosed documents.
	 * ATL used to keep it to the first, so an app that waits to be told the old
	 * session is gone before it goes on (Google Camera does, between modes)
	 * waited for ever.
	 */
	void closeWithoutCallback() {
		closeInternal();
	}

	private void closeInternal() {
		if (closed)
			return;
		closed = true;
		stopRepeatingInternal();
		synchronized (sequences) {
			sequences.clear();
		}
		outputs = null;
		postState("onClosed", new Runnable() {
			@Override
			public void run() {
				stateCallback.onClosed(CameraCaptureSession.this);
			}
		});
	}

	private void stopRepeatingInternal() {
		Sequence sequence;
		final int id;

		synchronized (sequences) {
			id = repeatingId;
			repeatingId = -1;
			sequence = id >= 0 ? sequences.remove(id) : null;
		}
		nativeDevice.stopRepeating();
		if (sequence == null || sequence.callback == null)
			return;

		final Sequence done = sequence;
		HandlerExecutor.run(sequence.executor, new Runnable() {
			@Override
			public void run() {
				done.callback.onCaptureSequenceCompleted(CameraCaptureSession.this, id,
				    done.lastFrameNumber);
			}
		});
	}

	/** which of the configured outputs this request aimed at, one bit each */
	private int targetMask(CaptureRequest request) {
		int mask = 0;

		for (int i = 0; i < outputs.size(); i++)
			if (request.containsTarget(outputs.get(i)))
				mask |= 1 << i;
		return mask;
	}

	private int submit(CaptureRequest request, CaptureCallback listener, Handler handler, boolean repeating) {
		if (request == null)
			throw new IllegalArgumentException("request must not be null");
		if (outputs == null)
			throw new IllegalStateException("the session has no configured output");
		for (Surface target : request.getTargets())
			if (!outputs.contains(target))
				throw new IllegalArgumentException("request targets a surface the session was not configured with");

		synchronized (sequences) {
			int id = nextSequenceId++;

			sequences.put(id, new Sequence(request, listener, HandlerExecutor.of(handler), repeating));
			return id;
		}
	}

	private void forget(int sequenceId) {
		synchronized (sequences) {
			sequences.remove(sequenceId);
			if (repeatingId == sequenceId)
				repeatingId = -1;
		}
	}

	private void postState(Runnable command) {
		HandlerExecutor.run(executor, command);
	}

	/** ATL_DEBUG_CAMERA2: the session state an app is waiting for. */
	private static final boolean DEBUG_LIFECYCLE = System.getenv("ATL_DEBUG_CAMERA2") != null;

	private void postState(final String what, final Runnable command) {
		if (!DEBUG_LIFECYCLE) {
			HandlerExecutor.run(executor, command);
			return;
		}
		System.err.println("camera2: session " + what + " posted"
		    + (executor == null ? " (inline: no executor)" : ""));
		HandlerExecutor.run(executor, new Runnable() {
			@Override
			public void run() {
				System.err.println("camera2: session " + what + " delivered on "
				    + Thread.currentThread().getName());
				command.run();
			}
		});
	}

	private void checkOpen() throws CameraAccessException {
		if (closed)
			throw new IllegalStateException("the capture session is closed");
		if (device.isClosed())
			throw new IllegalStateException("CameraDevice " + device.getId() + " is closed");
	}

	/* backend results, on the main loop */
	void dispatchCaptureStarted(int sequenceId, final long frameNumber, final long timestamp) {
		final Sequence sequence;

		synchronized (sequences) {
			sequence = sequences.get(sequenceId);
		}
		if (sequence == null || sequence.callback == null)
			return;

		HandlerExecutor.run(sequence.executor, new Runnable() {
			@Override
			public void run() {
				sequence.callback.onCaptureStarted(CameraCaptureSession.this, sequence.request,
				    timestamp, frameNumber);
			}
		});
	}

	/* the device failed: nothing in flight will ever be completed */
	void dispatchDeviceError() {
		List<Map.Entry<Integer, Sequence>> lost;

		synchronized (sequences) {
			lost = new ArrayList<Map.Entry<Integer, Sequence>>(sequences.entrySet());
			sequences.clear();
			repeatingId = -1;
		}
		for (Map.Entry<Integer, Sequence> entry : lost) {
			final int sequenceId = entry.getKey().intValue();
			final Sequence sequence = entry.getValue();

			if (sequence.callback == null)
				continue;
			HandlerExecutor.run(sequence.executor, new Runnable() {
				@Override
				public void run() {
					sequence.callback.onCaptureFailed(CameraCaptureSession.this, sequence.request,
					    new CaptureFailure(sequence.request, CaptureFailure.REASON_ERROR, true,
					        sequenceId, sequence.lastFrameNumber));
				}
			});
		}
	}

	/* the HAL dropped this frame: the request failed, and a one-shot is done */
	void dispatchCaptureFailed(final int sequenceId, final long frameNumber) {
		final Sequence sequence;

		synchronized (sequences) {
			sequence = sequences.get(sequenceId);
			if (sequence != null && !sequence.repeating)
				sequences.remove(sequenceId);
		}
		if (sequence == null || sequence.callback == null)
			return;

		HandlerExecutor.run(sequence.executor, new Runnable() {
			@Override
			public void run() {
				sequence.callback.onCaptureFailed(CameraCaptureSession.this, sequence.request,
				    new CaptureFailure(sequence.request, CaptureFailure.REASON_ERROR, true,
				        sequenceId, frameNumber));
			}
		});
	}

	/* one output's buffer of the frame will never arrive; the request goes on */
	void dispatchCaptureBufferLost(final int sequenceId, final long frameNumber, int stream) {
		final Sequence sequence;
		final Surface target;

		synchronized (sequences) {
			sequence = sequences.get(sequenceId);
		}
		if (sequence == null || sequence.callback == null || outputs == null ||
		    stream < 0 || stream >= outputs.size())
			return;
		target = outputs.get(stream);

		HandlerExecutor.run(sequence.executor, new Runnable() {
			@Override
			public void run() {
				sequence.callback.onCaptureBufferLost(CameraCaptureSession.this, sequence.request,
				    target, frameNumber);
			}
		});
	}

	void dispatchCaptureCompleted(final int sequenceId, final long frameNumber, CameraMetadataNative result) {
		final Sequence sequence;

		synchronized (sequences) {
			sequence = sequences.get(sequenceId);
			if (sequence != null) {
				sequence.lastFrameNumber = frameNumber;
				if (!sequence.repeating)
					sequences.remove(sequenceId);
			}
		}
		if (sequence == null || sequence.callback == null || result == null) {
			if (result != null)
				result.close(); /* nobody to hand the metadata to */
			return;
		}

		final TotalCaptureResult total =
		    new TotalCaptureResult(result, sequence.request, frameNumber, sequenceId, device.getId());
		HandlerExecutor.run(sequence.executor, new Runnable() {
			@Override
			public void run() {
				sequence.callback.onCaptureCompleted(CameraCaptureSession.this, sequence.request, total);
				if (!sequence.repeating)
					sequence.callback.onCaptureSequenceCompleted(CameraCaptureSession.this,
					    sequenceId, frameNumber);
			}
		});
	}

	@Override
	public String toString() {
		return "CameraCaptureSession(" + device.getId() + ")";
	}
}
