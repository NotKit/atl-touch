/*
**
** Copyright 2012, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

package android.opengl;

import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * EGL 1.4
 *
 * The natives return raw EGL handles and the wrappers box them, so that a
 * handle of 0 always comes back as the matching EGL_NO_* singleton: apps
 * (webrtc's EglBase14Impl among them) compare surfaces with != instead of
 * equals().
 */
public class EGL14 {
	public static final int EGL_DEFAULT_DISPLAY = 0;
	public static final int EGL_FALSE = 0;
	public static final int EGL_TRUE = 1;
	public static final int EGL_ALPHA_SIZE = 0x3021;
	public static final int EGL_BAD_ACCESS = 0x3002;
	public static final int EGL_BAD_ALLOC = 0x3003;
	public static final int EGL_BAD_ATTRIBUTE = 0x3004;
	public static final int EGL_BAD_CONFIG = 0x3005;
	public static final int EGL_BAD_CONTEXT = 0x3006;
	public static final int EGL_BAD_CURRENT_SURFACE = 0x3007;
	public static final int EGL_BAD_DISPLAY = 0x3008;
	public static final int EGL_BAD_MATCH = 0x3009;
	public static final int EGL_BAD_NATIVE_PIXMAP = 0x300A;
	public static final int EGL_BAD_NATIVE_WINDOW = 0x300B;
	public static final int EGL_BAD_PARAMETER = 0x300C;
	public static final int EGL_BAD_SURFACE = 0x300D;
	public static final int EGL_BLUE_SIZE = 0x3022;
	public static final int EGL_BUFFER_DESTROYED = 0x3095;
	public static final int EGL_BUFFER_PRESERVED = 0x3094;
	public static final int EGL_BUFFER_SIZE = 0x3020;
	public static final int EGL_CLIENT_APIS = 0x308D;
	public static final int EGL_COLOR_BUFFER_TYPE = 0x303F;
	public static final int EGL_CONFIG_CAVEAT = 0x3027;
	public static final int EGL_CONFIG_ID = 0x3028;
	public static final int EGL_CONFORMANT = 0x3042;
	public static final int EGL_CONTEXT_CLIENT_TYPE = 0x3097;
	public static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
	public static final int EGL_CONTEXT_LOST = 0x300E;
	public static final int EGL_CORE_NATIVE_ENGINE = 0x305B;
	public static final int EGL_DEPTH_SIZE = 0x3025;
	public static final int EGL_DONT_CARE = -1;
	public static final int EGL_DRAW = 0x3059;
	public static final int EGL_EXTENSIONS = 0x3055;
	public static final int EGL_GREEN_SIZE = 0x3023;
	public static final int EGL_HEIGHT = 0x3056;
	public static final int EGL_HORIZONTAL_RESOLUTION = 0x3090;
	public static final int EGL_LARGEST_PBUFFER = 0x3058;
	public static final int EGL_LEVEL = 0x3029;
	public static final int EGL_LUMINANCE_BUFFER = 0x308F;
	public static final int EGL_LUMINANCE_SIZE = 0x303D;
	public static final int EGL_MATCH_NATIVE_PIXMAP = 0x3041;
	public static final int EGL_MAX_PBUFFER_HEIGHT = 0x302A;
	public static final int EGL_MAX_PBUFFER_PIXELS = 0x302B;
	public static final int EGL_MAX_PBUFFER_WIDTH = 0x302C;
	public static final int EGL_MAX_SWAP_INTERVAL = 0x303C;
	public static final int EGL_MIN_SWAP_INTERVAL = 0x303B;
	public static final int EGL_MIPMAP_LEVEL = 0x3083;
	public static final int EGL_MIPMAP_TEXTURE = 0x3082;
	public static final int EGL_MULTISAMPLE_RESOLVE = 0x3099;
	public static final int EGL_MULTISAMPLE_RESOLVE_BOX = 0x309B;
	public static final int EGL_MULTISAMPLE_RESOLVE_BOX_BIT = 0x0200;
	public static final int EGL_MULTISAMPLE_RESOLVE_DEFAULT = 0x309A;
	public static final int EGL_NATIVE_RENDERABLE = 0x302D;
	public static final int EGL_NATIVE_VISUAL_ID = 0x302E;
	public static final int EGL_NATIVE_VISUAL_TYPE = 0x302F;
	public static final int EGL_NONE = 0x3038;
	public static final int EGL_NON_CONFORMANT_CONFIG = 0x3051;
	public static final int EGL_NOT_INITIALIZED = 0x3001;
	public static final int EGL_NO_TEXTURE = 0x305C;
	public static final int EGL_OPENGL_API = 0x30A2;
	public static final int EGL_OPENGL_BIT = 0x0008;
	public static final int EGL_OPENGL_ES2_BIT = 0x0004;
	public static final int EGL_OPENGL_ES_API = 0x30A0;
	public static final int EGL_OPENGL_ES_BIT = 0x0001;
	public static final int EGL_OPENVG_API = 0x30A1;
	public static final int EGL_OPENVG_BIT = 0x0002;
	public static final int EGL_OPENVG_IMAGE = 0x3096;
	public static final int EGL_PBUFFER_BIT = 0x0001;
	public static final int EGL_PIXEL_ASPECT_RATIO = 0x3092;
	public static final int EGL_PIXMAP_BIT = 0x0002;
	public static final int EGL_READ = 0x305A;
	public static final int EGL_RED_SIZE = 0x3024;
	public static final int EGL_RENDERABLE_TYPE = 0x3040;
	public static final int EGL_RENDER_BUFFER = 0x3086;
	public static final int EGL_RGB_BUFFER = 0x308E;
	public static final int EGL_SAMPLES = 0x3031;
	public static final int EGL_SAMPLE_BUFFERS = 0x3032;
	public static final int EGL_SINGLE_BUFFER = 0x3085;
	public static final int EGL_SLOW_CONFIG = 0x3050;
	public static final int EGL_STENCIL_SIZE = 0x3026;
	public static final int EGL_SUCCESS = 0x3000;
	public static final int EGL_SURFACE_TYPE = 0x3033;
	public static final int EGL_SWAP_BEHAVIOR = 0x3093;
	public static final int EGL_TEXTURE_2D = 0x305F;
	public static final int EGL_TEXTURE_FORMAT = 0x3080;
	public static final int EGL_TEXTURE_RGB = 0x305D;
	public static final int EGL_TEXTURE_RGBA = 0x305E;
	public static final int EGL_TEXTURE_TARGET = 0x3081;
	public static final int EGL_TRANSPARENT_BLUE_VALUE = 0x3035;
	public static final int EGL_TRANSPARENT_GREEN_VALUE = 0x3036;
	public static final int EGL_TRANSPARENT_RED_VALUE = 0x3037;
	public static final int EGL_TRANSPARENT_RGB = 0x3052;
	public static final int EGL_TRANSPARENT_TYPE = 0x3034;
	public static final int EGL_VENDOR = 0x3053;
	public static final int EGL_VERSION = 0x3054;
	public static final int EGL_VERTICAL_RESOLUTION = 0x3091;
	public static final int EGL_WIDTH = 0x3057;
	public static final int EGL_WINDOW_BIT = 0x0004;

	public static final EGLContext EGL_NO_CONTEXT = new EGLContext(0);
	public static final EGLDisplay EGL_NO_DISPLAY = new EGLDisplay(0);
	public static final EGLSurface EGL_NO_SURFACE = new EGLSurface(0);

	private EGL14() {}

	static EGLDisplay display(long handle) {
		return handle == 0 ? EGL_NO_DISPLAY : new EGLDisplay(handle);
	}

	static EGLContext context(long handle) {
		return handle == 0 ? EGL_NO_CONTEXT : new EGLContext(handle);
	}

	static EGLSurface surface(long handle) {
		return handle == 0 ? EGL_NO_SURFACE : new EGLSurface(handle);
	}

	public static EGLDisplay eglGetDisplay(int display_id) {
		return display(native_eglGetDisplay(display_id));
	}

	public static boolean eglInitialize(EGLDisplay dpy, int[] major, int majorOffset, int[] minor, int minorOffset) {
		if (major != null && major.length - majorOffset < 1)
			throw new IllegalArgumentException("length - majorOffset < 1");
		if (minor != null && minor.length - minorOffset < 1)
			throw new IllegalArgumentException("length - minorOffset < 1");
		int[] version = new int[2];
		if (!native_eglInitialize(dpy.getNativeHandle(), version))
			return false;
		if (major != null)
			major[majorOffset] = version[0];
		if (minor != null)
			minor[minorOffset] = version[1];
		return true;
	}

	public static boolean eglTerminate(EGLDisplay dpy) {
		return native_eglTerminate(dpy.getNativeHandle());
	}

	public static String eglQueryString(EGLDisplay dpy, int name) {
		return native_eglQueryString(dpy.getNativeHandle(), name);
	}

	public static boolean eglChooseConfig(EGLDisplay dpy, int[] attrib_list, int attribListOffset,
	    EGLConfig[] configs, int configsOffset, int config_size, int[] num_config, int num_configOffset) {
		long[] handles = configs != null ? new long[config_size] : null;
		int[] num = new int[1];
		if (!native_eglChooseConfig(dpy.getNativeHandle(), slice(attrib_list, attribListOffset), handles, config_size, num))
			return false;
		if (configs != null) {
			for (int i = 0; i < num[0] && configsOffset + i < configs.length; i++)
				configs[configsOffset + i] = new EGLConfig(handles[i]);
		}
		if (num_config != null)
			num_config[num_configOffset] = num[0];
		return true;
	}

	public static boolean eglGetConfigs(EGLDisplay dpy, EGLConfig[] configs, int configsOffset,
	    int config_size, int[] num_config, int num_configOffset) {
		return eglChooseConfig(dpy, new int[] {EGL_NONE}, 0, configs, configsOffset, config_size, num_config, num_configOffset);
	}

	public static boolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, int attribute, int[] value, int offset) {
		int[] out = new int[1];
		if (!native_eglGetConfigAttrib(dpy.getNativeHandle(), config.getNativeHandle(), attribute, out))
			return false;
		value[offset] = out[0];
		return true;
	}

	public static EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, int[] attrib_list, int offset) {
		return context(native_eglCreateContext(dpy.getNativeHandle(), config.getNativeHandle(),
		    share_context == null ? 0 : share_context.getNativeHandle(), slice(attrib_list, offset)));
	}

	public static boolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
		return native_eglDestroyContext(dpy.getNativeHandle(), ctx.getNativeHandle());
	}

	public static EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, Object win, int[] attrib_list, int offset) {
		Surface surface;
		if (win instanceof SurfaceView) {
			surface = ((SurfaceView)win).getHolder().getSurface();
		} else if (win instanceof SurfaceHolder) {
			surface = ((SurfaceHolder)win).getSurface();
		} else if (win instanceof Surface) {
			surface = (Surface)win;
		} else {
			throw new UnsupportedOperationException("eglCreateWindowSurface() can only be called with an instance of Surface, SurfaceView or SurfaceHolder at the moment.");
		}
		return surface(native_eglCreateWindowSurface(dpy.getNativeHandle(), config.getNativeHandle(), surface, slice(attrib_list, offset)));
	}

	public static EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, int[] attrib_list, int offset) {
		return surface(native_eglCreatePbufferSurface(dpy.getNativeHandle(), config.getNativeHandle(), slice(attrib_list, offset)));
	}

	public static boolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
		return native_eglDestroySurface(dpy.getNativeHandle(), surface.getNativeHandle());
	}

	public static boolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface, int attribute, int[] value, int offset) {
		int[] out = new int[1];
		if (!native_eglQuerySurface(dpy.getNativeHandle(), surface.getNativeHandle(), attribute, out))
			return false;
		value[offset] = out[0];
		return true;
	}

	public static boolean eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, int attribute, int value) {
		return native_eglSurfaceAttrib(dpy.getNativeHandle(), surface.getNativeHandle(), attribute, value);
	}

	public static boolean eglQueryContext(EGLDisplay dpy, EGLContext ctx, int attribute, int[] value, int offset) {
		int[] out = new int[1];
		if (!native_eglQueryContext(dpy.getNativeHandle(), ctx.getNativeHandle(), attribute, out))
			return false;
		value[offset] = out[0];
		return true;
	}

	public static boolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
		return native_eglMakeCurrent(dpy.getNativeHandle(), draw.getNativeHandle(), read.getNativeHandle(), ctx.getNativeHandle());
	}

	public static boolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
		return native_eglSwapBuffers(dpy.getNativeHandle(), surface.getNativeHandle());
	}

	public static boolean eglSwapInterval(EGLDisplay dpy, int interval) {
		return native_eglSwapInterval(dpy.getNativeHandle(), interval);
	}

	public static EGLDisplay eglGetCurrentDisplay() {
		return display(native_eglGetCurrentDisplay());
	}

	public static EGLContext eglGetCurrentContext() {
		return context(native_eglGetCurrentContext());
	}

	public static EGLSurface eglGetCurrentSurface(int readdraw) {
		return surface(native_eglGetCurrentSurface(readdraw));
	}

	public static native int eglGetError();
	public static native boolean eglBindAPI(int api);
	public static native int eglQueryAPI();
	public static native boolean eglWaitClient();
	public static native boolean eglWaitGL();
	public static native boolean eglWaitNative(int engine);
	public static native boolean eglReleaseThread();

	private static int[] slice(int[] array, int offset) {
		if (array == null)
			return null;
		if (offset == 0)
			return array;
		int[] out = new int[array.length - offset];
		System.arraycopy(array, offset, out, 0, out.length);
		return out;
	}

	private static native long native_eglGetDisplay(int display_id);
	private static native boolean native_eglInitialize(long dpy, int[] version);
	private static native boolean native_eglTerminate(long dpy);
	private static native String native_eglQueryString(long dpy, int name);
	private static native boolean native_eglChooseConfig(long dpy, int[] attrib_list, long[] configs, int config_size, int[] num_config);
	private static native boolean native_eglGetConfigAttrib(long dpy, long config, int attribute, int[] value);
	private static native long native_eglCreateContext(long dpy, long config, long share_context, int[] attrib_list);
	private static native boolean native_eglDestroyContext(long dpy, long ctx);
	private static native long native_eglCreateWindowSurface(long dpy, long config, Surface surface, int[] attrib_list);
	private static native long native_eglCreatePbufferSurface(long dpy, long config, int[] attrib_list);
	private static native boolean native_eglDestroySurface(long dpy, long surface);
	private static native boolean native_eglQuerySurface(long dpy, long surface, int attribute, int[] value);
	private static native boolean native_eglSurfaceAttrib(long dpy, long surface, int attribute, int value);
	private static native boolean native_eglQueryContext(long dpy, long ctx, int attribute, int[] value);
	private static native boolean native_eglMakeCurrent(long dpy, long draw, long read, long ctx);
	private static native boolean native_eglSwapBuffers(long dpy, long surface);
	private static native boolean native_eglSwapInterval(long dpy, int interval);
	private static native long native_eglGetCurrentDisplay();
	private static native long native_eglGetCurrentContext();
	private static native long native_eglGetCurrentSurface(int readdraw);
}
