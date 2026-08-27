package android.os;

/**
 * Thrown when an application performs a networking operation on its main
 * thread.
 *
 * ATL never throws this itself -- nothing here installs a StrictMode network
 * policy on the main looper, so the check that produces it on Android does not
 * run. The type is needed because app code throws and catches it directly
 * (coil's HttpUriFetcher does), and with the class absent that throw becomes a
 * NoClassDefFoundError instead of the diagnostic it was meant to be.
 */
public class NetworkOnMainThreadException extends RuntimeException {
}
