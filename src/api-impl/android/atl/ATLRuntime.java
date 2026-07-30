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
}
