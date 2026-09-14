package android.opengl;

/**
 * android.opengl.EGL15 (API 29): the EGL 1.5 additions on top of {@link EGL14}.
 *
 * Only the fence-sync half is implemented, which is what an app that pipelines
 * GPU work across threads needs (Google Camera's viewfinder effects fence every
 * frame). The platform-display and image entry points are EGL 1.5 spellings of
 * things ATL's window and camera paths already do their own way.
 */
public class EGL15 {
	public static final int EGL_CONTEXT_MAJOR_VERSION = 0x3098;
	public static final int EGL_CONTEXT_MINOR_VERSION = 0x30FB;
	public static final int EGL_CONTEXT_OPENGL_PROFILE_MASK = 0x30FD;
	public static final int EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT = 0x00000001;
	public static final int EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT = 0x00000002;
	public static final int EGL_CONTEXT_OPENGL_DEBUG = 0x31B0;
	public static final int EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE = 0x31B1;
	public static final int EGL_CONTEXT_OPENGL_ROBUST_ACCESS = 0x31B2;
	public static final int EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY = 0x31BD;
	public static final int EGL_NO_RESET_NOTIFICATION = 0x31BE;
	public static final int EGL_LOSE_CONTEXT_ON_RESET = 0x31BF;
	public static final int EGL_OPENGL_ES3_BIT = 0x00000040;

	public static final int EGL_SYNC_FENCE = 0x30F9;
	public static final int EGL_SYNC_TYPE = 0x30F7;
	public static final int EGL_SYNC_STATUS = 0x30F1;
	public static final int EGL_SYNC_CONDITION = 0x30F8;
	public static final int EGL_SIGNALED = 0x30F2;
	public static final int EGL_UNSIGNALED = 0x30F3;
	public static final int EGL_SYNC_FLUSH_COMMANDS_BIT = 0x0001;
	public static final long EGL_FOREVER = 0xFFFFFFFFFFFFFFFFL;
	public static final int EGL_TIMEOUT_EXPIRED = 0x30F5;
	public static final int EGL_CONDITION_SATISFIED = 0x30F6;
	public static final int EGL_SYNC_PRIOR_COMMANDS_COMPLETE = 0x30F0;
	public static final int EGL_SYNC_CL_EVENT = 0x30FE;
	public static final int EGL_SYNC_CL_EVENT_COMPLETE = 0x30FF;
	public static final int EGL_CL_EVENT_HANDLE = 0x309C;

	public static final int EGL_GL_COLORSPACE = 0x309D;
	public static final int EGL_GL_COLORSPACE_SRGB = 0x3089;
	public static final int EGL_GL_COLORSPACE_LINEAR = 0x308A;
	public static final int EGL_GL_RENDERBUFFER = 0x30B9;
	public static final int EGL_GL_TEXTURE_2D = 0x30B1;
	public static final int EGL_GL_TEXTURE_LEVEL = 0x30BC;
	public static final int EGL_IMAGE_PRESERVED = 0x30D2;

	public static final EGLSync EGL_NO_SYNC = new EGLSync(0);
	public static final EGLImage EGL_NO_IMAGE = new EGLImage(0);

	private static EGLSync sync(long handle) {
		return handle == 0 ? EGL_NO_SYNC : new EGLSync(handle);
	}

	public static EGLSync eglCreateSync(EGLDisplay dpy, int type, long[] attrib_list, int offset) {
		return sync(native_eglCreateSync(dpy.getNativeHandle(), type, slice(attrib_list, offset)));
	}

	public static boolean eglDestroySync(EGLDisplay dpy, EGLSync sync) {
		return native_eglDestroySync(dpy.getNativeHandle(), sync.getNativeHandle());
	}

	public static int eglClientWaitSync(EGLDisplay dpy, EGLSync sync, int flags, long timeout) {
		return native_eglClientWaitSync(dpy.getNativeHandle(), sync.getNativeHandle(), flags, timeout);
	}

	public static boolean eglWaitSync(EGLDisplay dpy, EGLSync sync, int flags) {
		return native_eglWaitSync(dpy.getNativeHandle(), sync.getNativeHandle(), flags);
	}

	public static boolean eglGetSyncAttrib(EGLDisplay dpy, EGLSync sync, int attribute,
	    long[] value, int offset) {
		long[] out = new long[1];

		if (!native_eglGetSyncAttrib(dpy.getNativeHandle(), sync.getNativeHandle(), attribute, out))
			return false;
		value[offset] = out[0];
		return true;
	}

	/* the attribute lists are EGLAttrib (pointer-sized) in EGL 1.5, and the
	 * offset form is the only one the Java API has */
	private static long[] slice(long[] list, int offset) {
		if (list == null)
			return null;
		if (offset == 0)
			return list;
		long[] out = new long[list.length - offset];
		System.arraycopy(list, offset, out, 0, out.length);
		return out;
	}

	private static native long native_eglCreateSync(long dpy, int type, long[] attrib_list);
	private static native boolean native_eglDestroySync(long dpy, long sync);
	private static native int native_eglClientWaitSync(long dpy, long sync, int flags, long timeout);
	private static native boolean native_eglWaitSync(long dpy, long sync, int flags);
	private static native boolean native_eglGetSyncAttrib(long dpy, long sync, int attribute, long[] value);
}
