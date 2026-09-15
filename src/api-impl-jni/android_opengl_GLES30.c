#include "defines.h"
#include <GLES3/gl3.h>
#include <jni.h>

#include "generated_headers/android_opengl_GLES30.h"

/* GLES 3.0 only: everything GLES 3.0 shares with 2.0 is inherited from
 * android.opengl.GLES20 and served by android_opengl_GLES20.c. */

JNIEXPORT void JNICALL Java_android_opengl_GLES30_glReadBuffer(JNIEnv *env, jclass this, jint mode)
{
	glReadBuffer((GLenum)mode);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES30_glTexStorage2D(JNIEnv *env, jclass this, jint target, jint levels, jint internalformat, jint width, jint height)
{
	glTexStorage2D((GLenum)target, (GLsizei)levels, (GLenum)internalformat, (GLsizei)width, (GLsizei)height);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES30_glBindBufferBase(JNIEnv *env, jclass this, jint target, jint index, jint buffer)
{
	glBindBufferBase((GLenum)target, (GLuint)index, (GLuint)buffer);
}

/* the mapped range as a direct ByteBuffer, which is what the caller casts it
 * to; NULL when the map fails, as in AOSP */
JNIEXPORT jobject JNICALL Java_android_opengl_GLES30_glMapBufferRange(JNIEnv *env, jclass this, jint target, jint offset, jint length, jint access)
{
	void *mapped = glMapBufferRange((GLenum)target, (GLintptr)offset, (GLsizeiptr)length, (GLbitfield)access);

	if (!mapped)
		return NULL;
	return (*env)->NewDirectByteBuffer(env, mapped, (jlong)length);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_GLES30_glUnmapBuffer(JNIEnv *env, jclass this, jint target)
{
	return glUnmapBuffer((GLenum)target) == GL_TRUE;
}

JNIEXPORT void JNICALL Java_android_opengl_GLES30_glUniform1ui(JNIEnv *env, jclass this, jint location, jint v0)
{
	glUniform1ui((GLint)location, (GLuint)v0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES30_glUniform2ui(JNIEnv *env, jclass this, jint location, jint v0, jint v1)
{
	glUniform2ui((GLint)location, (GLuint)v0, (GLuint)v1);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES30_glUniform3ui(JNIEnv *env, jclass this, jint location, jint v0, jint v1, jint v2)
{
	glUniform3ui((GLint)location, (GLuint)v0, (GLuint)v1, (GLuint)v2);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES30_glUniform4ui(JNIEnv *env, jclass this, jint location, jint v0, jint v1, jint v2, jint v3)
{
	glUniform4ui((GLint)location, (GLuint)v0, (GLuint)v1, (GLuint)v2, (GLuint)v3);
}
