package android.opengl;

/**
 * Wrapper class for native EGLSync objects.
 */
public class EGLSync extends EGLObjectHandle {
	EGLSync(long handle) {
		super(handle);
	}

	@Override
	public boolean equals(Object o) {
		if (this == o)
			return true;
		if (!(o instanceof EGLSync))
			return false;

		EGLSync that = (EGLSync)o;
		return getNativeHandle() == that.getNativeHandle();
	}
}
