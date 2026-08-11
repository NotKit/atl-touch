package android.view;

public class SurfaceControl {
	public static final int BUFFER_TRANSFORM_IDENTITY = 0;
	public static final int BUFFER_TRANSFORM_MIRROR_HORIZONTAL = 1;
	public static final int BUFFER_TRANSFORM_MIRROR_HORIZONTAL_ROTATE_90 = 5;
	public static final int BUFFER_TRANSFORM_MIRROR_VERTICAL = 2;
	public static final int BUFFER_TRANSFORM_MIRROR_VERTICAL_ROTATE_90 = 6;
	public static final int BUFFER_TRANSFORM_ROTATE_180 = 3;
	public static final int BUFFER_TRANSFORM_ROTATE_270 = 7;
	public static final int BUFFER_TRANSFORM_ROTATE_90 = 4;

	public boolean isValid() { return false; }

	public static class Builder { }

	public static class Transaction { 
	public android.view.SurfaceControl.Transaction reparent(android.view.SurfaceControl a0, android.view.SurfaceControl a1) { return null; }

	public android.view.SurfaceControl.Transaction setVisibility(android.view.SurfaceControl a0, boolean a1) { return null; }

	public void apply() { }

	public void close() { }

	public android.view.SurfaceControl.Transaction setBufferSize(android.view.SurfaceControl a0, int a1, int a2) { return null; }
}

	public void release() { }
}
