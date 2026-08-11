package android.content;

public interface ComponentCallbacks {
	public default void onConfigurationChanged(android.content.res.Configuration a0) { }

	public default void onLowMemory() { }
}
