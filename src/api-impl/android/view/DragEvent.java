package android.view;

import android.content.ClipData;
import android.content.ClipDescription;

/**
 * Drag-and-drop event. ATL has no drag and drop, so one is never delivered;
 * this exists because a view that declares onDragEvent(DragEvent) makes the
 * runtime load the parameter type as soon as anything reflects over its
 * declared methods.
 */
public class DragEvent {
	public static final int ACTION_DRAG_STARTED = 1;
	public static final int ACTION_DRAG_LOCATION = 2;
	public static final int ACTION_DROP = 3;
	public static final int ACTION_DRAG_ENDED = 4;
	public static final int ACTION_DRAG_ENTERED = 5;
	public static final int ACTION_DRAG_EXITED = 6;

	public DragEvent() {}

	public int getAction() {
		return 0;
	}

	public float getX() {
		return 0.0f;
	}

	public float getY() {
		return 0.0f;
	}

	public ClipData getClipData() {
		return null;
	}

	public ClipDescription getClipDescription() {
		return null;
	}

	public Object getLocalState() {
		return null;
	}

	public boolean getResult() {
		return false;
	}
}
