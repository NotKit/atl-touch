package android.media.projection;

public class MediaProjection {

	public static abstract class Callback {
	}

	public void registerCallback(android.media.projection.MediaProjection.Callback a0, android.os.Handler a1) { }

	public void stop() { }

	public void unregisterCallback(android.media.projection.MediaProjection.Callback a0) { }

	public android.hardware.display.VirtualDisplay createVirtualDisplay(java.lang.String a0, int a1, int a2, int a3, int a4, android.view.Surface a5, android.hardware.display.VirtualDisplay.Callback a6, android.os.Handler a7) { return null; }
}
