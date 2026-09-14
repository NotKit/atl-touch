package android.opengl;

/**
 * OpenGL ES 3.0.
 *
 * Everything GLES 3.0 shares with 2.0 comes from the superclass, as in AOSP, so
 * this class is the 3.0-only surface. Only what an app has been seen to call is
 * here: Google Camera's viewfinder pipeline reads back through glReadBuffer and
 * allocates its textures with glTexStorage2D, and the class not existing at all
 * killed the thread that draws its preview.
 *
 * The constants are for reading, not for apps: javac inlines a static final int
 * into the caller's dex, so an app never looks one up here.
 */
public class GLES30 extends GLES20 {

	public static final int GL_READ_BUFFER = 0x0C02;
	public static final int GL_DRAW_FRAMEBUFFER = 0x8CA9;
	public static final int GL_READ_FRAMEBUFFER = 0x8CA8;
	public static final int GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
	public static final int GL_COLOR_ATTACHMENT0 = 0x8CE0;
	public static final int GL_NONE = 0;
	public static final int GL_BACK = 0x0405;

	public static final int GL_HALF_FLOAT = 0x140B;
	public static final int GL_RED = 0x1903;
	public static final int GL_RG = 0x8227;
	public static final int GL_R8 = 0x8229;
	public static final int GL_RG8 = 0x822B;
	public static final int GL_R16F = 0x822D;
	public static final int GL_RG16F = 0x822F;
	public static final int GL_RGB8 = 0x8051;
	public static final int GL_RGBA8 = 0x8058;
	public static final int GL_RGB10_A2 = 0x8059;
	public static final int GL_RGBA16F = 0x881A;
	public static final int GL_RGB16F = 0x881B;
	public static final int GL_SRGB8_ALPHA8 = 0x8C43;

	// C function void glReadBuffer ( GLenum mode )

	public static native void glReadBuffer(int mode);

	// C function void glTexStorage2D ( GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height )

	public static native void glTexStorage2D(
	    int target,
	    int levels,
	    int internalformat,
	    int width,
	    int height);
}
