package android.view;

public class DragEvent {
	public static final int ACTION_DRAG_ENDED = 4;
	public static final int ACTION_DRAG_ENTERED = 5;
	public static final int ACTION_DRAG_EXITED = 6;
	public static final int ACTION_DRAG_LOCATION = 2;
	public static final int ACTION_DRAG_STARTED = 1;
	public static final int ACTION_DROP = 3;

	public android.content.ClipData getClipData() { return null; }

	public android.content.ClipDescription getClipDescription() { return null; }

	public float getX() { return 0.0f; }

	public float getY() { return 0.0f; }

	public int getAction() { return 0; }
}
