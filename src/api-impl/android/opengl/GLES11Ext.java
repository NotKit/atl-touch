package android.opengl;

/**
 * Only the OES_EGL_image_external bits apps actually reach for. The target is
 * real: a SurfaceTexture's texture is an external texture, filled through an
 * EGLImage (android_graphics_SurfaceTexture.c), so samplerExternalOES works.
 */
public class GLES11Ext {
	public static final int GL_TEXTURE_EXTERNAL_OES = 0x8D65;
	public static final int GL_TEXTURE_BINDING_EXTERNAL_OES = 0x8D67;
	public static final int GL_SAMPLER_EXTERNAL_OES = 0x8D66;
	public static final int GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES = 0x8D68;

	private GLES11Ext() {}
}
