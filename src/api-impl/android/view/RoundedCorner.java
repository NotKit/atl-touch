package android.view;

import android.graphics.Point;

/**
 * One rounded corner of a display. ATL's window has none, so nothing ever
 * hands one of these out - the class exists because apps name it while asking.
 */
public final class RoundedCorner {

	public static final int POSITION_TOP_LEFT = 0;
	public static final int POSITION_TOP_RIGHT = 1;
	public static final int POSITION_BOTTOM_RIGHT = 2;
	public static final int POSITION_BOTTOM_LEFT = 3;

	private final int position;
	private final int radius;
	private final Point center;

	public RoundedCorner(int position) {
		this(position, 0, 0, 0);
	}

	public RoundedCorner(int position, int radius, int centerX, int centerY) {
		this.position = position;
		this.radius = radius;
		this.center = new Point(centerX, centerY);
	}

	public int getPosition() {
		return position;
	}

	public int getRadius() {
		return radius;
	}

	public Point getCenter() {
		return new Point(center);
	}
}
