package android.opengl;

/**
 * OpenGL ES 3.1.
 *
 * Everything 3.1 shares with 3.0 and 2.0 comes from the superclasses, as in
 * AOSP, so this class is the 3.1-only surface: compute dispatch, the image
 * units a compute shader writes through, and the memory barrier between the
 * two. PhotonCamera does its whole raw pipeline in compute shaders, and the
 * class not existing at all killed the processing thread the moment a burst
 * was ready to merge.
 *
 * The constants are for reading, not for apps: javac inlines a static final int
 * into the caller's dex, so an app never looks one up here.
 */
public class GLES31 extends GLES30 {

	public static final int GL_COMPUTE_SHADER = 0x91B9;
	public static final int GL_SHADER_STORAGE_BUFFER = 0x90D2;
	public static final int GL_ATOMIC_COUNTER_BUFFER = 0x92C0;

	public static final int GL_READ_ONLY = 0x88B8;
	public static final int GL_WRITE_ONLY = 0x88B9;
	public static final int GL_READ_WRITE = 0x88BA;

	public static final int GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT = 0x00000001;
	public static final int GL_ELEMENT_ARRAY_BARRIER_BIT = 0x00000002;
	public static final int GL_UNIFORM_BARRIER_BIT = 0x00000004;
	public static final int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
	public static final int GL_SHADER_IMAGE_ACCESS_BARRIER_BIT = 0x00000020;
	public static final int GL_COMMAND_BARRIER_BIT = 0x00000040;
	public static final int GL_PIXEL_BUFFER_BARRIER_BIT = 0x00000080;
	public static final int GL_TEXTURE_UPDATE_BARRIER_BIT = 0x00000100;
	public static final int GL_BUFFER_UPDATE_BARRIER_BIT = 0x00000200;
	public static final int GL_FRAMEBUFFER_BARRIER_BIT = 0x00000400;
	public static final int GL_TRANSFORM_FEEDBACK_BARRIER_BIT = 0x00000800;
	public static final int GL_ATOMIC_COUNTER_BARRIER_BIT = 0x00001000;
	public static final int GL_SHADER_STORAGE_BARRIER_BIT = 0x00002000;
	public static final int GL_ALL_BARRIER_BITS = 0xFFFFFFFF;

	public static final int GL_ALL_SHADER_BITS = 0xFFFFFFFF;
	public static final int GL_COMPUTE_SHADER_BIT = 0x00000020;

	// C function void glDispatchCompute ( GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z )

	public static native void glDispatchCompute(
	    int num_groups_x,
	    int num_groups_y,
	    int num_groups_z);

	// C function void glMemoryBarrier ( GLbitfield barriers )

	public static native void glMemoryBarrier(int barriers);

	// C function void glBindImageTexture ( GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format )

	public static native void glBindImageTexture(
	    int unit,
	    int texture,
	    int level,
	    boolean layered,
	    int layer,
	    int access,
	    int format);
}
