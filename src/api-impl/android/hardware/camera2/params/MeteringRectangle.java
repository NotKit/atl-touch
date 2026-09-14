package android.hardware.camera2.params;

import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;

/**
 * One weighted metering region, in active-array coordinates.
 *
 * The HAL lays a region out as (x, y, width, height, weight), which is what
 * CameraMetadataNative writes and reads; Rect's two-corner form is only the
 * Java view of the first four.
 */
public final class MeteringRectangle {

	public static final int METERING_WEIGHT_DONT_CARE = 0;
	public static final int METERING_WEIGHT_MIN = 0;
	public static final int METERING_WEIGHT_MAX = 1000;

	private final int x;
	private final int y;
	private final int width;
	private final int height;
	private final int weight;

	public MeteringRectangle(int x, int y, int width, int height, int meteringWeight) {
		if (x < 0 || y < 0 || width < 0 || height < 0)
			throw new IllegalArgumentException("a metering rectangle cannot be negative");
		if (meteringWeight < METERING_WEIGHT_MIN)
			throw new IllegalArgumentException("meteringWeight must not be negative");
		this.x = x;
		this.y = y;
		this.width = width;
		this.height = height;
		this.weight = meteringWeight;
	}

	public MeteringRectangle(Point xy, Size dimensions, int meteringWeight) {
		this(xy.x, xy.y, dimensions.getWidth(), dimensions.getHeight(), meteringWeight);
	}

	public MeteringRectangle(Rect rect, int meteringWeight) {
		this(rect.left, rect.top, rect.width(), rect.height(), meteringWeight);
	}

	public int getX() {
		return x;
	}

	public int getY() {
		return y;
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	public int getMeteringWeight() {
		return weight;
	}

	public Point getUpperLeftPoint() {
		return new Point(x, y);
	}

	public Size getSize() {
		return new Size(width, height);
	}

	public Rect getRect() {
		return new Rect(x, y, x + width, y + height);
	}

	@Override
	public boolean equals(Object other) {
		if (!(other instanceof MeteringRectangle))
			return false;
		MeteringRectangle o = (MeteringRectangle)other;
		return x == o.x && y == o.y && width == o.width && height == o.height && weight == o.weight;
	}

	@Override
	public int hashCode() {
		return ((((x * 31 + y) * 31 + width) * 31 + height) * 31) + weight;
	}

	@Override
	public String toString() {
		return "MeteringRectangle(x=" + x + ", y=" + y + ", width=" + width +
		    ", height=" + height + ", weight=" + weight + ")";
	}
}
