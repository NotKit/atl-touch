package android.hardware.camera2.impl;

import android.os.Handler;

import java.util.concurrent.Executor;

/**
 * The Handler overloads of the camera2 API in Executor terms.
 *
 * A null Handler means "run it here", the way the headless callers and
 * CameraManager's own replay do; AOSP would use the calling thread's Looper.
 */
public final class HandlerExecutor implements Executor {
	private final Handler handler;

	private HandlerExecutor(Handler handler) {
		this.handler = handler;
	}

	/** null when the callbacks should run inline. */
	public static Executor of(Handler handler) {
		return handler == null ? null : new HandlerExecutor(handler);
	}

	public static void run(Executor executor, Runnable command) {
		if (executor != null)
			executor.execute(command);
		else
			command.run();
	}

	@Override
	public void execute(Runnable command) {
		handler.post(command);
	}
}
