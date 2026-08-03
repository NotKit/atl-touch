package android.opengl;

public class EGLExt {
	public static final int EGL_CONTEXT_FLAGS_KHR = 0x30FC;
	public static final int EGL_CONTEXT_MAJOR_VERSION_KHR = 0x3098;
	public static final int EGL_CONTEXT_MINOR_VERSION_KHR = 0x30FB;
	public static final int EGL_OPENGL_ES3_BIT_KHR = 0x0040;
	public static final int EGL_RECORDABLE_ANDROID = 0x3142;

	private EGLExt() {}

	public static boolean eglPresentationTimeANDROID(EGLDisplay dpy, EGLSurface sur, long time) {
		return native_eglPresentationTimeANDROID(dpy.getNativeHandle(), sur.getNativeHandle(), time);
	}

	private static native boolean native_eglPresentationTimeANDROID(long dpy, long surface, long time);
}
