package android.opengl;
import android.graphics.Bitmap;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGL11;

public class GLUtils {
	/** Name of an EGL error code, or its hex value when the code is unknown. */
	public static String getEGLErrorString(int error) {
		switch (error) {
		case EGL10.EGL_SUCCESS:
			return "EGL_SUCCESS";
		case EGL10.EGL_NOT_INITIALIZED:
			return "EGL_NOT_INITIALIZED";
		case EGL10.EGL_BAD_ACCESS:
			return "EGL_BAD_ACCESS";
		case EGL10.EGL_BAD_ALLOC:
			return "EGL_BAD_ALLOC";
		case EGL10.EGL_BAD_ATTRIBUTE:
			return "EGL_BAD_ATTRIBUTE";
		case EGL10.EGL_BAD_CONFIG:
			return "EGL_BAD_CONFIG";
		case EGL10.EGL_BAD_CONTEXT:
			return "EGL_BAD_CONTEXT";
		case EGL10.EGL_BAD_CURRENT_SURFACE:
			return "EGL_BAD_CURRENT_SURFACE";
		case EGL10.EGL_BAD_DISPLAY:
			return "EGL_BAD_DISPLAY";
		case EGL10.EGL_BAD_MATCH:
			return "EGL_BAD_MATCH";
		case EGL10.EGL_BAD_NATIVE_PIXMAP:
			return "EGL_BAD_NATIVE_PIXMAP";
		case EGL10.EGL_BAD_NATIVE_WINDOW:
			return "EGL_BAD_NATIVE_WINDOW";
		case EGL10.EGL_BAD_PARAMETER:
			return "EGL_BAD_PARAMETER";
		case EGL10.EGL_BAD_SURFACE:
			return "EGL_BAD_SURFACE";
		case EGL11.EGL_CONTEXT_LOST:
			return "EGL_CONTEXT_LOST";
		default:
			return "0x" + Integer.toHexString(error);
		}
	}

	public static void texImage2D(int target, int level, Bitmap bitmap, int border) {
		if (native_texImage2D(target, level, -1, bitmap, -1, border) != 0) {
			throw new IllegalArgumentException("invalid Bitmap format");
		}
	}

	private static native int native_texImage2D(int target, int level, int internalformat,
	                                            Bitmap bitmap, int type, int border);
}
