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

	public static final int GL_UNIFORM_BUFFER = 0x8A11;
	public static final int GL_TRANSFORM_FEEDBACK_BUFFER = 0x8C8E;
	public static final int GL_MAP_READ_BIT = 0x0001;
	public static final int GL_MAP_WRITE_BIT = 0x0002;
	public static final int GL_MAP_INVALIDATE_RANGE_BIT = 0x0004;
	public static final int GL_MAP_INVALIDATE_BUFFER_BIT = 0x0008;
	public static final int GL_MAP_FLUSH_EXPLICIT_BIT = 0x0010;
	public static final int GL_MAP_UNSYNCHRONIZED_BIT = 0x0020;

	// C function void glReadBuffer ( GLenum mode )

	public static native void glReadBuffer(int mode);

	// C function void glTexStorage2D ( GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height )

	public static native void glTexStorage2D(
	    int target,
	    int levels,
	    int internalformat,
	    int width,
	    int height);

	// C function void glBindBufferBase ( GLenum target, GLuint index, GLuint buffer )

	public static native void glBindBufferBase(
	    int target,
	    int index,
	    int buffer);

	// C function GLvoid * glMapBufferRange ( GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access )

	public static native java.nio.Buffer glMapBufferRange(
	    int target,
	    int offset,
	    int length,
	    int access);

	// C function GLboolean glUnmapBuffer ( GLenum target )

	public static native boolean glUnmapBuffer(int target);

	// C function void glUniform1ui ( GLint location, GLuint v0 )

	public static native void glUniform1ui(
	    int location,
	    int v0);

	// C function void glUniform2ui ( GLint location, GLuint v0, GLuint v1 )

	public static native void glUniform2ui(
	    int location,
	    int v0,
	    int v1);

	// C function void glUniform3ui ( GLint location, GLuint v0, GLuint v1, GLuint v2 )

	public static native void glUniform3ui(
	    int location,
	    int v0,
	    int v1,
	    int v2);

	// C function void glUniform4ui ( GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3 )

	public static native void glUniform4ui(
	    int location,
	    int v0,
	    int v1,
	    int v2,
	    int v3);
}
