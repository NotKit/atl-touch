package android.opengl;

/**
 * Only the OES_EGL_image_external bits apps actually reach for. ATL has no
 * external textures: android_opengl_GLES20.c maps GL_TEXTURE_EXTERNAL_OES onto
 * GL_TEXTURE_2D, which is what SurfaceTexture uploads into.
 */
public class GLES11Ext {
	public static final int GL_TEXTURE_EXTERNAL_OES = 0x8D65;
	public static final int GL_TEXTURE_BINDING_EXTERNAL_OES = 0x8D67;
	public static final int GL_SAMPLER_EXTERNAL_OES = 0x8D66;
	public static final int GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES = 0x8D68;

	private GLES11Ext() {}
}
