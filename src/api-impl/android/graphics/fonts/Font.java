package android.graphics.fonts;

/**
 * A font object wrapping a native minikin::Font.
 *
 * Minimal subset of the AOSP class: currently only carries the native
 * pointer for consumers like android.graphics.text.PositionedGlyphs.
 * Replace with the full AOSP lift once Font.Builder is needed.
 */
public class Font {
	private final long mNativePtr;

	/** @hide */
	public Font(long nativePtr) {
		mNativePtr = nativePtr;
	}

	/** @hide */
	public long getNativePtr() {
		return mNativePtr;
	}

	@Override
	public boolean equals(Object o) {
		if (o == this)
			return true;
		if (!(o instanceof Font))
			return false;
		return ((Font)o).mNativePtr == mNativePtr;
	}

	@Override
	public int hashCode() {
		return Long.hashCode(mNativePtr);
	}
}
