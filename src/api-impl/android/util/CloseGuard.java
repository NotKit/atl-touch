package android.util;

/**
 * The public wrapper (API 30) around dalvik's leak detector.
 *
 * ATL does not report leaks, so every method is a no-op - but the class has to
 * exist: a resource wrapper builds one in its constructor, and on a finalizer
 * thread the missing class becomes an exception nobody catches.
 */
public final class CloseGuard {

	public CloseGuard() {}

	public void open(String closeMethodName) {}

	public void openWithCallSite(String closeMethodName, String callSite) {}

	public void close() {}

	public void warnIfOpen() {}
}
