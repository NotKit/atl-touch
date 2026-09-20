#include "defines.h"
#include "util.h"
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

JNIEXPORT void JNICALL Java_android_opengl_GLES30_glProgramParameteri(JNIEnv *env, jclass this, jint program, jint pname, jint value)
{
	glProgramParameteri((GLuint)program, (GLenum)pname, (GLint)value);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES30_glProgramBinary(JNIEnv *env, jclass this, jint program, jint binaryFormat, jobject binary_buf, jint length)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *binary = get_nio_buffer(env, binary_buf, &array_ref, &array);

	glProgramBinary((GLuint)program, (GLenum)binaryFormat, binary, (GLsizei)length);
	release_nio_buffer(env, array_ref, array);
}

/* the int[] overload: length is optional, as in AOSP, binaryFormat is not */
JNIEXPORT void JNICALL Java_android_opengl_GLES30_glGetProgramBinary__II_3II_3IILjava_nio_Buffer_2(JNIEnv *env, jclass this, jint program, jint bufSize, jintArray length_ref, jint lengthOffset, jintArray binaryFormat_ref, jint binaryFormatOffset, jobject binary_buf)
{
	jint *length = length_ref ? (*env)->GetIntArrayElements(env, length_ref, NULL) : NULL;
	jint *binaryFormat = (*env)->GetIntArrayElements(env, binaryFormat_ref, NULL);
	jarray array_ref;
	jbyte *array;
	/* last: get_nio_buffer pins a heap buffer, and nothing may call back
	 * into JNI while that pin is held */
	GLvoid *binary = get_nio_buffer(env, binary_buf, &array_ref, &array);

	glGetProgramBinary((GLuint)program, (GLsizei)bufSize,
	    length ? (GLsizei *)(length + lengthOffset) : NULL,
	    (GLenum *)(binaryFormat + binaryFormatOffset), binary);

	release_nio_buffer(env, array_ref, array);
	(*env)->ReleaseIntArrayElements(env, binaryFormat_ref, binaryFormat, 0);
	if (length)
		(*env)->ReleaseIntArrayElements(env, length_ref, length, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES30_glGetProgramBinary__IILjava_nio_IntBuffer_2Ljava_nio_IntBuffer_2Ljava_nio_Buffer_2(JNIEnv *env, jclass this, jint program, jint bufSize, jobject length_buf, jobject binaryFormat_buf, jobject binary_buf)
{
	jarray length_array_ref, format_array_ref, binary_array_ref;
	jbyte *length_array, *format_array, *binary_array;
	GLvoid *length = get_nio_buffer(env, length_buf, &length_array_ref, &length_array);
	GLvoid *binaryFormat = get_nio_buffer(env, binaryFormat_buf, &format_array_ref, &format_array);
	GLvoid *binary = get_nio_buffer(env, binary_buf, &binary_array_ref, &binary_array);

	glGetProgramBinary((GLuint)program, (GLsizei)bufSize, (GLsizei *)length, (GLenum *)binaryFormat, binary);

	release_nio_buffer(env, binary_array_ref, binary_array);
	release_nio_buffer(env, format_array_ref, format_array);
	release_nio_buffer(env, length_array_ref, length_array);
}
