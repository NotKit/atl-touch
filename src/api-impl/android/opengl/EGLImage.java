package android.opengl;

/**
 * Wrapper class for native EGLImage objects.
 */
public class EGLImage extends EGLObjectHandle {
	EGLImage(long handle) {
		super(handle);
	}

	@Override
	public boolean equals(Object o) {
		if (this == o)
			return true;
		if (!(o instanceof EGLImage))
			return false;

		EGLImage that = (EGLImage)o;
		return getNativeHandle() == that.getNativeHandle();
	}
}
