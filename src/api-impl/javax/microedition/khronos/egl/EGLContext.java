package javax.microedition.khronos.egl;

import javax.microedition.khronos.opengles.GL;

public class EGLContext {
	private static final EGL EGL_INSTANCE = new com.google.android.gles_jni.EGLImpl();
	private static final GL GL_INSTANCE = new com.google.android.gles_jni.GLImpl(); // FIXME - not all GLs are created equal
	public long native_egl_context = 0;

	public static EGL getEGL() {
		return EGL_INSTANCE;
	}

	// FIXME - not all GLs are created equal
	public GL getGL() {
		return GL_INSTANCE;
	}

	public EGLContext(long native_egl_context) {
		this.native_egl_context = native_egl_context;
	}

	/* the handle is the identity, as it is for EGLSurfaceImpl: apps compare
	 * their context with the one eglGetCurrentContext() hands back */
	@Override
	public boolean equals(Object o) {
		if (this == o)
			return true;
		if (o == null || getClass() != o.getClass())
			return false;
		return native_egl_context == ((EGLContext)o).native_egl_context;
	}

	@Override
	public int hashCode() {
		return Long.hashCode(native_egl_context);
	}
}
