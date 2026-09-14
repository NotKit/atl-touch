package java.util.concurrent;

/**
 * java.util.concurrent.Future with the API 34 (Java 19) methods.
 *
 * ART's own core-oj predates them, and java.* cannot be shadowed from
 * api-impl.jar, so this whole interface replaces the runtime's copy from the
 * front of the boot class path.  It has to repeat the original abstract
 * methods: the first boot class path entry that defines a class wins outright,
 * ART never merges the two.  The default bodies are AOSP's.
 */
public interface Future<V> {

	boolean cancel(boolean mayInterruptIfRunning);

	boolean isCancelled();

	boolean isDone();

	V get() throws InterruptedException, ExecutionException;

	V get(long timeout, TimeUnit unit) throws InterruptedException, ExecutionException, TimeoutException;

	default V resultNow() {
		if (!isDone())
			throw new IllegalStateException("Task has not completed");
		boolean interrupted = false;
		try {
			while (true) {
				try {
					return get();
				} catch (InterruptedException e) {
					interrupted = true;
				} catch (ExecutionException e) {
					throw new IllegalStateException("Task completed with exception");
				} catch (CancellationException e) {
					throw new IllegalStateException("Task was cancelled");
				}
			}
		} finally {
			if (interrupted)
				Thread.currentThread().interrupt();
		}
	}

	default Throwable exceptionNow() {
		if (!isDone())
			throw new IllegalStateException("Task has not completed");
		if (isCancelled())
			throw new IllegalStateException("Task was cancelled");
		boolean interrupted = false;
		try {
			while (true) {
				try {
					get();
					throw new IllegalStateException("Task completed with a result");
				} catch (InterruptedException e) {
					interrupted = true;
				} catch (ExecutionException e) {
					return e.getCause();
				}
			}
		} finally {
			if (interrupted)
				Thread.currentThread().interrupt();
		}
	}

	enum State {
		RUNNING,
		SUCCESS,
		FAILED,
		CANCELLED
	}

	default State state() {
		if (!isDone())
			return State.RUNNING;
		if (isCancelled())
			return State.CANCELLED;
		boolean interrupted = false;
		try {
			while (true) {
				try {
					get();
					return State.SUCCESS;
				} catch (InterruptedException e) {
					interrupted = true;
				} catch (ExecutionException e) {
					return State.FAILED;
				}
			}
		} finally {
			if (interrupted)
				Thread.currentThread().interrupt();
		}
	}
}
