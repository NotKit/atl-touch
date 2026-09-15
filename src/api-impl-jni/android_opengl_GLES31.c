#include "defines.h"
#include <GLES3/gl31.h>
#include <jni.h>

#include "generated_headers/android_opengl_GLES31.h"

/* GLES 3.1 only: everything 3.1 shares with 3.0 and 2.0 is inherited from
 * android.opengl.GLES30 and served by the files for those. */

JNIEXPORT void JNICALL Java_android_opengl_GLES31_glDispatchCompute(JNIEnv *env, jclass this, jint num_groups_x, jint num_groups_y, jint num_groups_z)
{
	glDispatchCompute((GLuint)num_groups_x, (GLuint)num_groups_y, (GLuint)num_groups_z);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES31_glMemoryBarrier(JNIEnv *env, jclass this, jint barriers)
{
	glMemoryBarrier((GLbitfield)barriers);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES31_glBindImageTexture(JNIEnv *env, jclass this, jint unit, jint texture, jint level, jboolean layered, jint layer, jint access, jint format)
{
	glBindImageTexture((GLuint)unit, (GLuint)texture, (GLint)level, (GLboolean)layered,
	                   (GLint)layer, (GLenum)access, (GLenum)format);
}
