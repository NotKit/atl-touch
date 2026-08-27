package android.atl;

/**
 * Runtime entry points the native launcher can only reach through Java.
 */
public class ATLRuntime {

	private ATLRuntime() {
	}

	/**
	 * Loads a native library into the class loader that owns the framework.
	 *
	 * System.load() registers a library with the class loader of its calling
	 * class, and JNI method lookup only searches the loader of the class that
	 * declares the method. A launcher calling System.load() straight from JNI
	 * has no Java caller, so the library would land on the boot loader and the
	 * framework's own natives would never resolve. ART's two-argument
	 * Runtime.loadLibrary(String, ClassLoader) has no equivalent on a stock JVM,
	 * so the call has to come from a framework class instead.
	 */
	public static void loadNativeLibrary(String path) {
		System.load(path);
	}

	/**
	 * Installs the process-wide defaults ART's RuntimeInit installs before any
	 * app code runs and a stock JVM does not have.
	 *
	 * So far there is one: the default uncaught exception handler. Apps chain
	 * their own crash reporting onto it and assume it is already there -- Kotlin
	 * casts Thread.getDefaultUncaughtExceptionHandler() to a non-null type, so on
	 * a bare JVM onCreate dies before it ever builds a UI, and the window comes
	 * up blank with nothing in the log.
	 *
	 * A handler the caller installed itself (through -javaagent, say) is left
	 * alone; this only fills in a null.
	 */
	public static void installProcessDefaults(String processName, int pid) {
		if (Thread.getDefaultUncaughtExceptionHandler() == null)
			Thread.setDefaultUncaughtExceptionHandler(new FatalExceptionHandler(processName, pid));
	}

	/**
	 * What ART's RuntimeInit.LoggingHandler and KillApplicationHandler do
	 * between them: the same three lines, then the process goes away with a
	 * non-zero status.
	 *
	 * It ends the process rather than only printing because an app that
	 * delegates to the platform handler is delegating the dying part; on ART
	 * nothing runs after this. halt() rather than exit() for the same reason --
	 * ART kills the process outright and runs no shutdown hook.
	 *
	 * Not a lambda on purpose: dx rejects invokedynamic, and this class is
	 * dexed for the ART launcher too.
	 */
	private static class FatalExceptionHandler implements Thread.UncaughtExceptionHandler {
		private final String processName;
		private final int pid;

		FatalExceptionHandler(String processName, int pid) {
			this.processName = processName;
			this.pid = pid;
		}

		public void uncaughtException(Thread thread, Throwable throwable) {
			try {
				System.err.println("FATAL EXCEPTION: " + thread.getName());
				System.err.println("Process: " + processName + ", PID: " + pid);
				throwable.printStackTrace();
				System.err.flush();
			} catch (Throwable ignored) {
				/* the handler itself must not be the reason nothing is reported */
			} finally {
				Runtime.getRuntime().halt(10); /* the status ART's KillApplicationHandler exits with */
			}
		}
	}
}
