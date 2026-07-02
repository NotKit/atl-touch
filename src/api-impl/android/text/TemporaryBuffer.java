package android.text;

/**
 * A simple char[] recycler, as used by the text system for transient copies.
 */
public class TemporaryBuffer {
	private static char[] sTemp = null;

	public static char[] obtain(int len) {
		char[] buf;
		synchronized (TemporaryBuffer.class) {
			buf = sTemp;
			sTemp = null;
		}
		if (buf == null || buf.length < len)
			buf = new char[len < 1000 ? 1000 : len];
		return buf;
	}

	public static void recycle(char[] temp) {
		if (temp.length > 1000)
			return;
		synchronized (TemporaryBuffer.class) {
			sTemp = temp;
		}
	}
}
