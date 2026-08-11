package android.hardware.display;

public class VirtualDisplay {

	public static abstract class Callback {
		public void onPaused() {}

		public void onResumed() {}

		public void onStopped() {}
	}


	public void release() { }

	public void resize(int a0, int a1, int a2) { }

	public void setSurface(android.view.Surface a0) { }
}
