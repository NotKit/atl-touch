package android.atl;

import dalvik.system.PathClassLoader;

/**
 * Builds the class loader an app's dex is loaded through.
 *
 * This exists only to keep the {@link PathClassLoader} reference out of
 * {@link ATLLoadedApp}: dalvik.system is an ART class, and a JVM linking
 * ATLLoadedApp would have to resolve it even on the launchers that never
 * take this path (they hand us an apk already on the system class path).
 */
final class ATLAppClassLoader {

	private ATLAppClassLoader() {
	}

	/* our own loader is the boot one (api-impl.jar sits on the boot class path), so the
	 * apk's class loader context stays PCL[] and ART keeps accepting its oat file */
	static ClassLoader create(String classLoaderPath, String nativePath) {
		return new PathClassLoader(classLoaderPath, nativePath, ATLLoadedApp.class.getClassLoader());
	}
}
