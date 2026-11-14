package android.view;

import android.graphics.Point;
import android.graphics.Rect;

public class ScrollCaptureSession {
	private final Surface surface;
	private final Rect scrollBounds;
	private final Point positionInWindow;

	public ScrollCaptureSession(Surface surface, Rect scrollBounds, Point positionInWindow) {
		this.surface = surface;
		this.scrollBounds = new Rect(scrollBounds);
		this.positionInWindow = new Point(positionInWindow.x, positionInWindow.y);
	}

	public Surface getSurface() {
		return surface;
	}

	public Rect getScrollBounds() {
		return new Rect(scrollBounds);
	}

	public Point getPositionInWindow() {
		return new Point(positionInWindow.x, positionInWindow.y);
	}
}
