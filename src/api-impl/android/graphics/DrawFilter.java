package android.graphics;

/**
 * A DrawFilter subclass can be installed in a Canvas. When it is present, it
 * can modify the paint that is used to draw (temporarily). With this, a filter
 * can disable/enable antialiasing, or change the color for everything this is
 * drawn.
 */
public class DrawFilter {
	// this is set by subclasses, but don't make it public
	public long mNativeInt; // pointer to native object
}
