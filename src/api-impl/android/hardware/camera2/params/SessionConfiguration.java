package android.hardware.camera2.params;

import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CaptureRequest;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Executor;

/** Everything CameraDevice.createCaptureSession(SessionConfiguration) needs in one object. */
public final class SessionConfiguration {

	public static final int SESSION_REGULAR = 0;
	public static final int SESSION_HIGH_SPEED = 1;

	private final int sessionType;
	private final List<OutputConfiguration> outputConfigurations;
	private final Executor executor;
	private final CameraCaptureSession.StateCallback stateCallback;

	private InputConfiguration inputConfiguration;
	private CaptureRequest sessionParameters;

	public SessionConfiguration(int sessionType, List<OutputConfiguration> outputs,
	    Executor executor, CameraCaptureSession.StateCallback cb) {
		if (outputs == null)
			throw new IllegalArgumentException("outputs must not be null");
		if (cb == null)
			throw new IllegalArgumentException("a session configuration needs a state callback");
		this.sessionType = sessionType;
		this.outputConfigurations = new ArrayList<OutputConfiguration>(outputs);
		this.executor = executor;
		this.stateCallback = cb;
	}

	public int getSessionType() {
		return sessionType;
	}

	public List<OutputConfiguration> getOutputConfigurations() {
		return Collections.unmodifiableList(outputConfigurations);
	}

	public Executor getExecutor() {
		return executor;
	}

	public CameraCaptureSession.StateCallback getStateCallback() {
		return stateCallback;
	}

	public void setInputConfiguration(InputConfiguration input) {
		if (sessionType == SESSION_HIGH_SPEED)
			throw new UnsupportedOperationException("a high speed session has no input");
		inputConfiguration = input;
	}

	public InputConfiguration getInputConfiguration() {
		return inputConfiguration;
	}

	public void setSessionParameters(CaptureRequest params) {
		sessionParameters = params;
	}

	public CaptureRequest getSessionParameters() {
		return sessionParameters;
	}

	@Override
	public String toString() {
		return "SessionConfiguration(type " + sessionType + ", " +
		    outputConfigurations.size() + " output(s))";
	}
}
