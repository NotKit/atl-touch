#include "defines.h"
#include "util.h"
#include <GLES2/gl2.h>
#include <jni.h>
#include <stdint.h>

#include "../libandroid/native_window.h"

#include "generated_headers/android_opengl_GLES20.h"

JNIEXPORT jstring JNICALL Java_android_opengl_GLES20_glGetString(JNIEnv *env, jclass, jint name)
{
	const char *chars = (const char *)glGetString((GLenum)name);
	return _JSTRING(chars);
}

JNIEXPORT jint JNICALL Java_android_opengl_GLES20_glGetError(JNIEnv *env, jclass)
{
	return (jint)glGetError();
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetIntegerv__I_3II(JNIEnv *env, jclass, jint pname, jintArray params_ref, jint offset)
{
	jint *params = (*env)->GetIntArrayElements(env, params_ref, NULL);
	glGetIntegerv((GLenum)pname, params + offset);
	(*env)->ReleaseIntArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glEnableVertexAttribArray(JNIEnv *env, jclass, jint index)
{
	glEnableVertexAttribArray((GLuint)index);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glVertexAttribPointerBounds(JNIEnv *env, jclass, jint index, jint size, jint type, jboolean normalized, jint stride, jobject pointer, jint count)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *pixels = get_nio_buffer(env, pointer, &array_ref, &array);

	glVertexAttribPointer(index, size, type, normalized, stride, pixels);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDisable(JNIEnv *env, jclass, jint cap)
{
	glDisable((GLenum)cap);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glActiveTexture(JNIEnv *env, jclass, jint texture)
{
	glActiveTexture((GLenum)texture);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glEnable(JNIEnv *env, jclass, jint cap)
{
	glEnable((GLenum)cap);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glFrontFace(JNIEnv *env, jclass, jint mode)
{
	glFrontFace((GLenum)mode);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glViewport(JNIEnv *env, jclass, jint x, jint y, jint width, jint height)
{
	glViewport((GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenTextures__I_3II(JNIEnv *env, jclass, jint n, jintArray textures_ref, jint offset)
{
	jint *textures = (*env)->GetIntArrayElements(env, textures_ref, NULL);
	glGenTextures((GLsizei)n, (GLuint *)textures + offset);
	(*env)->ReleaseIntArrayElements(env, textures_ref, textures, 0);
}

static void throw_unsupported(JNIEnv *env, const char *message)
{
	jclass clazz = (*env)->FindClass(env, "java/lang/UnsupportedOperationException");
	if (clazz)
		(*env)->ThrowNew(env, clazz, message);
}

/* The name buffer AOSP's String-returning overloads allocate for themselves. */
static jstring get_active_name(JNIEnv *env, jint program, jint index, GLenum length_pname, GLint *size, GLenum *type, int uniform)
{
	GLint maxlen = 0;
	glGetProgramiv((GLuint)program, length_pname, &maxlen);
	if (maxlen <= 0)
		maxlen = 256;

	GLchar *name = malloc(maxlen + 1);
	name[0] = '\0';
	if (uniform)
		glGetActiveUniform((GLuint)program, (GLuint)index, maxlen + 1, NULL, size, type, name);
	else
		glGetActiveAttrib((GLuint)program, (GLuint)index, maxlen + 1, NULL, size, type, name);
	jstring output = _JSTRING(name);
	free(name);
	return output;
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBindTexture(JNIEnv *env, jclass, jint target, jint texture)
{
	glBindTexture((GLenum)target, (GLuint)texture);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteTextures__I_3II(JNIEnv *env, jclass, jint n, jintArray textures_ref, jint offset)
{
	jint *textures = (*env)->GetIntArrayElements(env, textures_ref, NULL);
	glDeleteTextures((GLsizei)n, (GLuint *)textures + offset);
	(*env)->ReleaseIntArrayElements(env, textures_ref, textures, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexImage2D(JNIEnv *env, jclass, jint target, jint level, jint internalformat, jint width, jint height, jint border, jint format, jint type, jobject pixels_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *pixels = get_nio_buffer(env, pixels_buf, &array_ref, &array);
	glTexImage2D((GLenum)target, (GLint)level, (GLint)internalformat, (GLsizei)width, (GLsizei)height, (GLint)border, (GLenum)format, (GLenum)type, pixels);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexSubImage2D(JNIEnv *env, jclass, jint target, jint level, jint xoffset, jint yoffset, jint width, jint height, jint format, jint type, jobject pixels_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *pixels = get_nio_buffer(env, pixels_buf, &array_ref, &array);
	glTexSubImage2D((GLenum)target, (GLint)level, (GLint)xoffset, (GLint)yoffset, (GLsizei)width, (GLsizei)height, (GLenum)format, (GLenum)type, pixels);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexParameterf(JNIEnv *env, jclass, jint target, jint pname, jfloat param)
{
	glTexParameterf((GLenum)target, (GLenum)pname, (GLfloat)param);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenBuffers__I_3II(JNIEnv *env, jclass, jint n, jintArray buffers_ref, jint offset)
{
	jint *buffers = (*env)->GetIntArrayElements(env, buffers_ref, NULL);
	glGenBuffers((GLsizei)n, (GLuint *)buffers + offset);
	(*env)->ReleaseIntArrayElements(env, buffers_ref, buffers, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBindBuffer(JNIEnv *env, jclass, jint target, jint buffer)
{
	glBindBuffer((GLenum)target, (GLuint)buffer);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBufferData(JNIEnv *env, jclass, jint target, jint size, jobject data_buf, jint usage)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *data = get_nio_buffer(env, data_buf, &array_ref, &array);
	glBufferData((GLenum)target, (GLsizeiptr)size, data, (GLenum)usage);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDisableVertexAttribArray(JNIEnv *env, jclass, jint index)
{
	glDisableVertexAttribArray((GLuint)index);
}

JNIEXPORT jint JNICALL Java_android_opengl_GLES20_glCreateShader(JNIEnv *env, jclass, jint type)
{
	return (jint)glCreateShader((GLenum)type);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glShaderSource(JNIEnv *env, jclass, jint shader, jstring string)
{
	const char *nativeString = (*env)->GetStringUTFChars(env, string, NULL);
	const char *strings[] = {nativeString};
	glShaderSource(shader, 1, strings, 0);
	(*env)->ReleaseStringUTFChars(env, string, nativeString);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glCompileShader(JNIEnv *env, jclass, jint shader)
{
	glCompileShader((GLuint)shader);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetShaderiv__II_3II(JNIEnv *env, jclass, jint shader, jint pname, jintArray params_ref, jint offset)
{
	jint *params = (*env)->GetIntArrayElements(env, params_ref, NULL);
	glGetShaderiv((GLuint)shader, (GLenum)pname, params + offset);
	(*env)->ReleaseIntArrayElements(env, params_ref, params, 0);
}

JNIEXPORT jint JNICALL Java_android_opengl_GLES20_glCreateProgram(JNIEnv *env, jclass)
{
	return (jint)glCreateProgram();
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glAttachShader(JNIEnv *env, jclass, jint program, jint shader)
{
	glAttachShader((GLuint)program, (GLuint)shader);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBindAttribLocation(JNIEnv *env, jclass, jint program, jint index, jstring name)
{
	const char *nativeName = (*env)->GetStringUTFChars(env, name, NULL);
	glBindAttribLocation((GLuint)program, (GLuint)index, nativeName);
	(*env)->ReleaseStringUTFChars(env, name, nativeName);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glLinkProgram(JNIEnv *env, jclass, jint program)
{
	glLinkProgram((GLuint)program);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetProgramiv__II_3II(JNIEnv *env, jclass, jint program, jint pname, jintArray params_ref, jint offset)
{
	jint *params = (*env)->GetIntArrayElements(env, params_ref, NULL);
	glGetProgramiv((GLuint)program, (GLenum)pname, params + offset);
	(*env)->ReleaseIntArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetActiveAttrib__III_3II_3II_3II_3BI(JNIEnv *env, jclass, jint program, jint index, jint bufsize, jintArray length_ref, jint lengthOffset, jintArray size_ref, jint sizeOffset, jintArray type_ref, jint typeOffset, jbyteArray name_ref, jint nameOffset)
{
	jint *length = (*env)->GetIntArrayElements(env, length_ref, NULL);
	jint *size = (*env)->GetIntArrayElements(env, size_ref, NULL);
	jint *type = (*env)->GetIntArrayElements(env, type_ref, NULL);
	jbyte *name = (*env)->GetByteArrayElements(env, name_ref, NULL);
	glGetActiveAttrib((GLuint)program, (GLuint)index, (GLsizei)bufsize, (GLsizei *)length + lengthOffset, (GLint *)size + sizeOffset, (GLenum *)type + typeOffset, (char *)name + nameOffset);
	(*env)->ReleaseByteArrayElements(env, name_ref, name, 0);
	(*env)->ReleaseIntArrayElements(env, type_ref, type, 0);
	(*env)->ReleaseIntArrayElements(env, size_ref, size, 0);
	(*env)->ReleaseIntArrayElements(env, length_ref, length, 0);
}

JNIEXPORT jint JNICALL Java_android_opengl_GLES20_glGetAttribLocation(JNIEnv *env, jclass, jint program, jstring name)
{
	const char *nativeName = (*env)->GetStringUTFChars(env, name, NULL);
	jint ret = glGetAttribLocation((GLuint)program, nativeName);
	(*env)->ReleaseStringUTFChars(env, name, nativeName);
	return ret;
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetActiveUniform__III_3II_3II_3II_3BI(JNIEnv *env, jclass, jint program, jint index, jint bufsize, jintArray length_ref, jint lengthOffset, jintArray size_ref, jint sizeOffset, jintArray type_ref, jint typeOffset, jbyteArray name_ref, jint nameOffset)
{
	jint *length = (*env)->GetIntArrayElements(env, length_ref, NULL);
	jint *size = (*env)->GetIntArrayElements(env, size_ref, NULL);
	jint *type = (*env)->GetIntArrayElements(env, type_ref, NULL);
	jbyte *name = (*env)->GetByteArrayElements(env, name_ref, NULL);
	glGetActiveUniform((GLuint)program, (GLuint)index, (GLsizei)bufsize, (GLsizei *)length + lengthOffset, (GLint *)size + sizeOffset, (GLenum *)type + typeOffset, (char *)name + nameOffset);
	(*env)->ReleaseByteArrayElements(env, name_ref, name, 0);
	(*env)->ReleaseIntArrayElements(env, type_ref, type, 0);
	(*env)->ReleaseIntArrayElements(env, size_ref, size, 0);
	(*env)->ReleaseIntArrayElements(env, length_ref, length, 0);
}

JNIEXPORT jint JNICALL Java_android_opengl_GLES20_glGetUniformLocation(JNIEnv *env, jclass, jint program, jstring name)
{
	const char *nativeName = (*env)->GetStringUTFChars(env, name, NULL);
	jint ret = glGetUniformLocation((GLuint)program, nativeName);
	(*env)->ReleaseStringUTFChars(env, name, nativeName);
	return ret;
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteShader(JNIEnv *env, jclass, jint shader)
{
	glDeleteShader((GLuint)shader);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUseProgram(JNIEnv *env, jclass, jint program)
{
	glUseProgram((GLuint)program);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glVertexAttribPointer(JNIEnv *env, jclass, jint indx, jint size, jint type, jboolean normalized, jint stride, jint offset)
{
	glVertexAttribPointer((GLuint)indx, (GLint)size, (GLenum)type, (GLboolean)normalized, (GLsizei)stride, (GLvoid *)(intptr_t)offset);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniformMatrix4fv__IIZ_3FI(JNIEnv *env, jclass, jint location, jint count, jboolean transpose, jfloatArray value_ref, jint offset)
{
	jfloat *value = (*env)->GetFloatArrayElements(env, value_ref, NULL);
	glUniformMatrix4fv((GLint)location, (GLsizei)count, (GLboolean)transpose, (GLfloat *)value + offset);
	(*env)->ReleaseFloatArrayElements(env, value_ref, value, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform1i(JNIEnv *env, jclass, jint location, jint x)
{
	glUniform1i((GLint)location, (GLint)x);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform4f(JNIEnv *env, jclass, jint location, jfloat x, jfloat y, jfloat z, jfloat w)
{
	glUniform4f((GLint)location, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDrawArrays(JNIEnv *env, jclass, jint mode, jint first, jint count)
{
	glDrawArrays((GLenum)mode, (GLint)first, (GLsizei)count);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDrawElements__IIILjava_nio_Buffer_2(JNIEnv *env, jclass, jint mode, jint count, jint type, jobject indices)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *data = get_nio_buffer(env, indices, &array_ref, &array);

	glDrawElements((GLenum)mode, (GLsizei)count, (GLenum)type, data);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDrawElements__IIII(JNIEnv *env, jclass, jint mode, jint count, jint type, jint offset)
{
	glDrawElements((GLenum)mode, (GLsizei)count, (GLenum)type, (const GLvoid *)(intptr_t)offset);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glClearColor(JNIEnv *env, jclass, jfloat red, jfloat green, jfloat blue, jfloat alpha)
{
	glClearColor((GLclampf)red, (GLclampf)green, (GLclampf)blue, (GLclampf)alpha);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glClear(JNIEnv *env, jclass, jint mask)
{
	glClear((GLbitfield)mask);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBlendFunc(JNIEnv *env, jclass, jint sfactor, jint dfactor)
{
	glBlendFunc((GLenum)sfactor, (GLenum)dfactor);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetFloatv__I_3FI(JNIEnv *env, jclass this, jint pname, jfloatArray params_ref, jint offset)
{
	GLfloat *params_base = (GLfloat *)(*env)->GetPrimitiveArrayCritical(env, params_ref, 0);
	GLfloat *params = params_base + offset;

	glGetFloatv((GLenum)pname, params);

	(*env)->ReleasePrimitiveArrayCritical(env, params_ref, params_base, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glFlush(JNIEnv *env, jclass this)
{
	glFlush();
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glReadPixels(JNIEnv *env, jclass this, jint x, jint y, jint width, jint height, jint format, jint type, jobject pixels_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *pixels = get_nio_buffer(env, pixels_buf, &array_ref, &array);
	glReadPixels(x, y, width, height, format, type, pixels);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glPixelStorei(JNIEnv *env, jclass this, jint pname, jint param)
{
	glPixelStorei((GLenum)pname, (GLint)param);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexParameteri(JNIEnv *env, jclass this, jint target, jint pname, jint param)
{
	glTexParameteri((GLenum)target, (GLenum)pname, (GLint)param);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetShaderiv__IILjava_nio_IntBuffer_2(JNIEnv *env, jclass this, jint shader, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetShaderiv((GLuint)shader, (GLenum)pname, (GLint *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetProgramiv__IILjava_nio_IntBuffer_2(JNIEnv *env, jclass this, jint program, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetProgramiv((GLuint)program, (GLenum)pname, (GLint *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDepthMask(JNIEnv *env, jclass this, jboolean flag)
{
	glDepthMask((GLboolean)flag);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBlendFuncSeparate(JNIEnv *env, jclass this, jint srcRGB, jint dstRGB, jint srcAlpha, jint dstAlpha)
{
	glBlendFuncSeparate((GLenum)srcRGB, (GLenum)dstRGB, (GLenum)srcAlpha, (GLenum)dstAlpha);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenFramebuffers__I_3II(JNIEnv *env, jclass this, jint n, jintArray framebuffers_ref, jint offset)
{
	GLuint *framebuffers = (*env)->GetPrimitiveArrayCritical(env, framebuffers_ref, 0);
	glGenFramebuffers((GLsizei)n, framebuffers + offset);
	(*env)->ReleasePrimitiveArrayCritical(env, framebuffers_ref, framebuffers, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBindFramebuffer(JNIEnv *env, jclass this, jint target, jint framebuffer)
{
	bionic_glBindFramebuffer((GLenum)target, (GLuint)framebuffer);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glFramebufferTexture2D(JNIEnv *env, jclass this, jint target, jint attachment, jint textarget, jint texture, jint level)
{
	glFramebufferTexture2D((GLenum)target, (GLenum)attachment, (GLenum)textarget, (GLuint)texture, (GLint)level);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBindRenderbuffer(JNIEnv *env, jclass this, jint target, jint renderbuffer)
{
	glBindRenderbuffer((GLenum)target, (GLuint)renderbuffer);
}

JNIEXPORT jint JNICALL Java_android_opengl_GLES20_glCheckFramebufferStatus(JNIEnv *env, jclass this, jint target)
{
	return (jint)glCheckFramebufferStatus((GLenum)target);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteFramebuffers__I_3II(JNIEnv *env, jclass this, jint n, jintArray framebuffers_ref, jint offset)
{
	GLuint *framebuffers = (*env)->GetPrimitiveArrayCritical(env, framebuffers_ref, 0);
	glDeleteFramebuffers((GLsizei)n, framebuffers + offset);
	(*env)->ReleasePrimitiveArrayCritical(env, framebuffers_ref, framebuffers, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteProgram(JNIEnv *env, jclass this, jint program)
{
	glDeleteProgram((GLuint)program);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetFloatv__ILjava_nio_FloatBuffer_2(JNIEnv *env, jclass this, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetFloatv((GLenum)pname, (GLfloat *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenerateMipmap(JNIEnv *env, jclass this, jint pname)
{
	glGenerateMipmap((GLenum)pname);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glLineWidth(JNIEnv *env, jclass this, jfloat width)
{
	glLineWidth((GLfloat)width);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glColorMask(JNIEnv *env, jclass this, jboolean red, jboolean green, jboolean blue, jboolean alpha)
{
	glColorMask((GLboolean)red, (GLboolean)green, (GLboolean)blue, (GLboolean)alpha);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBufferSubData(JNIEnv *env, jclass this, jint target, jint offset, jint size, jobject data_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *data = get_nio_buffer(env, data_buf, &array_ref, &array);
	glBufferSubData((GLenum)target, (GLintptr)offset, (GLsizeiptr)size, data);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT jstring JNICALL Java_android_opengl_GLES20_glGetShaderInfoLog(JNIEnv *env, jclass this, jint shader)
{
	GLsizei bufSize;
	glGetShaderiv((GLuint)shader, GL_INFO_LOG_LENGTH, &bufSize);

	jstring output;
	if (bufSize == 0) {
		char cstring = 0;
		output = _JSTRING(&cstring);
	} else {
		GLchar *infoLog = malloc(sizeof(GLchar) * bufSize + 1);
		GLsizei length;
		glGetShaderInfoLog((GLuint)shader, bufSize, &length, infoLog);
		output = _JSTRING(infoLog);
		free(infoLog);
	}
	return output;
}

JNIEXPORT jstring JNICALL Java_android_opengl_GLES20_glGetActiveUniform__IILjava_nio_IntBuffer_2Ljava_nio_IntBuffer_2(JNIEnv *env, jclass this, jint program, jint index, jobject size_buf, jobject type_buf)
{
	jarray size_array_ref, type_array_ref;
	jbyte *size_array, *type_array;
	GLvoid *size = get_nio_buffer(env, size_buf, &size_array_ref, &size_array);
	GLvoid *type = get_nio_buffer(env, type_buf, &type_array_ref, &type_array);
	jstring output = get_active_name(env, program, index, GL_ACTIVE_UNIFORM_MAX_LENGTH, (GLint *)size, (GLenum *)type, 1);
	release_nio_buffer(env, type_array_ref, type_array);
	release_nio_buffer(env, size_array_ref, size_array);
	return output;
}

/*
 * The rest of GLES 2.0. Every entry point below follows the AOSP binding: the
 * array overloads take a Java array plus an element offset, the Buffer
 * overloads a direct or heap NIO buffer through get_nio_buffer(). Nothing here
 * is emulated -- each one forwards to the GL call of the same name.
 */

/* ---- per-fragment and per-primitive state ---- */

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glCullFace(JNIEnv *env, jclass, jint mode)
{
	glCullFace((GLenum)mode);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDepthFunc(JNIEnv *env, jclass, jint func)
{
	glDepthFunc((GLenum)func);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDepthRangef(JNIEnv *env, jclass, jfloat zNear, jfloat zFar)
{
	glDepthRangef((GLclampf)zNear, (GLclampf)zFar);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glClearDepthf(JNIEnv *env, jclass, jfloat depth)
{
	glClearDepthf((GLclampf)depth);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glClearStencil(JNIEnv *env, jclass, jint s)
{
	glClearStencil((GLint)s);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glPolygonOffset(JNIEnv *env, jclass, jfloat factor, jfloat units)
{
	glPolygonOffset((GLfloat)factor, (GLfloat)units);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glSampleCoverage(JNIEnv *env, jclass, jfloat value, jboolean invert)
{
	glSampleCoverage((GLclampf)value, (GLboolean)invert);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glScissor(JNIEnv *env, jclass, jint x, jint y, jint width, jint height)
{
	glScissor((GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glHint(JNIEnv *env, jclass, jint target, jint mode)
{
	glHint((GLenum)target, (GLenum)mode);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glFinish(JNIEnv *env, jclass)
{
	glFinish();
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBlendColor(JNIEnv *env, jclass, jfloat red, jfloat green, jfloat blue, jfloat alpha)
{
	glBlendColor((GLclampf)red, (GLclampf)green, (GLclampf)blue, (GLclampf)alpha);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBlendEquation(JNIEnv *env, jclass, jint mode)
{
	glBlendEquation((GLenum)mode);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glBlendEquationSeparate(JNIEnv *env, jclass, jint modeRGB, jint modeAlpha)
{
	glBlendEquationSeparate((GLenum)modeRGB, (GLenum)modeAlpha);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glStencilFunc(JNIEnv *env, jclass, jint func, jint ref, jint mask)
{
	glStencilFunc((GLenum)func, (GLint)ref, (GLuint)mask);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glStencilFuncSeparate(JNIEnv *env, jclass, jint face, jint func, jint ref, jint mask)
{
	glStencilFuncSeparate((GLenum)face, (GLenum)func, (GLint)ref, (GLuint)mask);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glStencilMask(JNIEnv *env, jclass, jint mask)
{
	glStencilMask((GLuint)mask);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glStencilMaskSeparate(JNIEnv *env, jclass, jint face, jint mask)
{
	glStencilMaskSeparate((GLenum)face, (GLuint)mask);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glStencilOp(JNIEnv *env, jclass, jint fail, jint zfail, jint zpass)
{
	glStencilOp((GLenum)fail, (GLenum)zfail, (GLenum)zpass);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glStencilOpSeparate(JNIEnv *env, jclass, jint face, jint fail, jint zfail, jint zpass)
{
	glStencilOpSeparate((GLenum)face, (GLenum)fail, (GLenum)zfail, (GLenum)zpass);
}

/* ---- programs and shaders ---- */

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDetachShader(JNIEnv *env, jclass, jint program, jint shader)
{
	glDetachShader((GLuint)program, (GLuint)shader);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glValidateProgram(JNIEnv *env, jclass, jint program)
{
	glValidateProgram((GLuint)program);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glReleaseShaderCompiler(JNIEnv *env, jclass)
{
	glReleaseShaderCompiler();
}

JNIEXPORT jstring JNICALL Java_android_opengl_GLES20_glGetProgramInfoLog(JNIEnv *env, jclass, jint program)
{
	GLsizei bufSize = 0;
	glGetProgramiv((GLuint)program, GL_INFO_LOG_LENGTH, &bufSize);

	if (bufSize <= 0)
		return _JSTRING("");

	GLchar *infoLog = malloc(bufSize + 1);
	infoLog[0] = '\0';
	glGetProgramInfoLog((GLuint)program, bufSize, NULL, infoLog);
	jstring output = _JSTRING(infoLog);
	free(infoLog);
	return output;
}

JNIEXPORT jstring JNICALL Java_android_opengl_GLES20_glGetShaderSource__I(JNIEnv *env, jclass, jint shader)
{
	GLint length = 0;
	glGetShaderiv((GLuint)shader, GL_SHADER_SOURCE_LENGTH, &length);

	if (length <= 0)
		return _JSTRING("");

	GLchar *source = malloc(length + 1);
	source[0] = '\0';
	glGetShaderSource((GLuint)shader, length, NULL, source);
	jstring output = _JSTRING(source);
	free(source);
	return output;
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetShaderSource__II_3II_3BI(JNIEnv *env, jclass, jint shader, jint bufsize, jintArray length_ref, jint lengthOffset, jbyteArray source_ref, jint sourceOffset)
{
	jint *length = length_ref ? (*env)->GetIntArrayElements(env, length_ref, NULL) : NULL;
	jbyte *source = (*env)->GetByteArrayElements(env, source_ref, NULL);
	glGetShaderSource((GLuint)shader, (GLsizei)bufsize, length ? (GLsizei *)length + lengthOffset : NULL, (GLchar *)source + sourceOffset);
	(*env)->ReleaseByteArrayElements(env, source_ref, source, 0);
	if (length)
		(*env)->ReleaseIntArrayElements(env, length_ref, length, 0);
}

/*
 * AOSP marks this overload broken (b/6006380): it takes a single byte where the
 * source needs a buffer, so there is nothing to write the shader into. Refusing
 * it is what Android does too.
 */
JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetShaderSource__IILjava_nio_IntBuffer_2B(JNIEnv *env, jclass, jint shader, jint bufsize, jobject length_buf, jbyte source)
{
	throw_unsupported(env, "glGetShaderSource");
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetShaderPrecisionFormat__II_3II_3II(JNIEnv *env, jclass, jint shadertype, jint precisiontype, jintArray range_ref, jint rangeOffset, jintArray precision_ref, jint precisionOffset)
{
	jint *range = (*env)->GetIntArrayElements(env, range_ref, NULL);
	jint *precision = (*env)->GetIntArrayElements(env, precision_ref, NULL);
	glGetShaderPrecisionFormat((GLenum)shadertype, (GLenum)precisiontype, (GLint *)range + rangeOffset, (GLint *)precision + precisionOffset);
	(*env)->ReleaseIntArrayElements(env, precision_ref, precision, 0);
	(*env)->ReleaseIntArrayElements(env, range_ref, range, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetShaderPrecisionFormat__IILjava_nio_IntBuffer_2Ljava_nio_IntBuffer_2(JNIEnv *env, jclass, jint shadertype, jint precisiontype, jobject range_buf, jobject precision_buf)
{
	jarray range_array_ref, precision_array_ref;
	jbyte *range_array, *precision_array;
	GLvoid *range = get_nio_buffer(env, range_buf, &range_array_ref, &range_array);
	GLvoid *precision = get_nio_buffer(env, precision_buf, &precision_array_ref, &precision_array);
	glGetShaderPrecisionFormat((GLenum)shadertype, (GLenum)precisiontype, (GLint *)range, (GLint *)precision);
	release_nio_buffer(env, precision_array_ref, precision_array);
	release_nio_buffer(env, range_array_ref, range_array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetAttachedShaders__II_3II_3II(JNIEnv *env, jclass, jint program, jint maxcount, jintArray count_ref, jint countOffset, jintArray shaders_ref, jint shadersOffset)
{
	jint *count = count_ref ? (*env)->GetIntArrayElements(env, count_ref, NULL) : NULL;
	jint *shaders = (*env)->GetIntArrayElements(env, shaders_ref, NULL);
	glGetAttachedShaders((GLuint)program, (GLsizei)maxcount, count ? (GLsizei *)count + countOffset : NULL, (GLuint *)shaders + shadersOffset);
	(*env)->ReleaseIntArrayElements(env, shaders_ref, shaders, 0);
	if (count)
		(*env)->ReleaseIntArrayElements(env, count_ref, count, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetAttachedShaders__IILjava_nio_IntBuffer_2Ljava_nio_IntBuffer_2(JNIEnv *env, jclass, jint program, jint maxcount, jobject count_buf, jobject shaders_buf)
{
	jarray count_array_ref, shaders_array_ref;
	jbyte *count_array, *shaders_array;
	GLvoid *count = get_nio_buffer(env, count_buf, &count_array_ref, &count_array);
	GLvoid *shaders = get_nio_buffer(env, shaders_buf, &shaders_array_ref, &shaders_array);
	glGetAttachedShaders((GLuint)program, (GLsizei)maxcount, (GLsizei *)count, (GLuint *)shaders);
	release_nio_buffer(env, shaders_array_ref, shaders_array);
	release_nio_buffer(env, count_array_ref, count_array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glShaderBinary__I_3IIILjava_nio_Buffer_2I(JNIEnv *env, jclass, jint n, jintArray shaders_ref, jint offset, jint binaryformat, jobject binary_buf, jint length)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *binary = get_nio_buffer(env, binary_buf, &array_ref, &array);
	jint *shaders = (*env)->GetIntArrayElements(env, shaders_ref, NULL);
	glShaderBinary((GLsizei)n, (GLuint *)shaders + offset, (GLenum)binaryformat, binary, (GLsizei)length);
	(*env)->ReleaseIntArrayElements(env, shaders_ref, shaders, JNI_ABORT);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glShaderBinary__ILjava_nio_IntBuffer_2ILjava_nio_Buffer_2I(JNIEnv *env, jclass, jint n, jobject shaders_buf, jint binaryformat, jobject binary_buf, jint length)
{
	jarray shaders_array_ref, binary_array_ref;
	jbyte *shaders_array, *binary_array;
	GLvoid *shaders = get_nio_buffer(env, shaders_buf, &shaders_array_ref, &shaders_array);
	GLvoid *binary = get_nio_buffer(env, binary_buf, &binary_array_ref, &binary_array);
	glShaderBinary((GLsizei)n, (GLuint *)shaders, (GLenum)binaryformat, binary, (GLsizei)length);
	release_nio_buffer(env, binary_array_ref, binary_array);
	release_nio_buffer(env, shaders_array_ref, shaders_array);
}

/* ---- active attribute and uniform introspection ---- */

JNIEXPORT jstring JNICALL Java_android_opengl_GLES20_glGetActiveAttrib__II_3II_3II(JNIEnv *env, jclass, jint program, jint index, jintArray size_ref, jint sizeOffset, jintArray type_ref, jint typeOffset)
{
	jint *size = (*env)->GetIntArrayElements(env, size_ref, NULL);
	jint *type = (*env)->GetIntArrayElements(env, type_ref, NULL);
	jstring output = get_active_name(env, program, index, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, (GLint *)size + sizeOffset, (GLenum *)type + typeOffset, 0);
	(*env)->ReleaseIntArrayElements(env, type_ref, type, 0);
	(*env)->ReleaseIntArrayElements(env, size_ref, size, 0);
	return output;
}

JNIEXPORT jstring JNICALL Java_android_opengl_GLES20_glGetActiveAttrib__IILjava_nio_IntBuffer_2Ljava_nio_IntBuffer_2(JNIEnv *env, jclass, jint program, jint index, jobject size_buf, jobject type_buf)
{
	jarray size_array_ref, type_array_ref;
	jbyte *size_array, *type_array;
	GLvoid *size = get_nio_buffer(env, size_buf, &size_array_ref, &size_array);
	GLvoid *type = get_nio_buffer(env, type_buf, &type_array_ref, &type_array);
	jstring output = get_active_name(env, program, index, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, (GLint *)size, (GLenum *)type, 0);
	release_nio_buffer(env, type_array_ref, type_array);
	release_nio_buffer(env, size_array_ref, size_array);
	return output;
}

JNIEXPORT jstring JNICALL Java_android_opengl_GLES20_glGetActiveUniform__II_3II_3II(JNIEnv *env, jclass, jint program, jint index, jintArray size_ref, jint sizeOffset, jintArray type_ref, jint typeOffset)
{
	jint *size = (*env)->GetIntArrayElements(env, size_ref, NULL);
	jint *type = (*env)->GetIntArrayElements(env, type_ref, NULL);
	jstring output = get_active_name(env, program, index, GL_ACTIVE_UNIFORM_MAX_LENGTH, (GLint *)size + sizeOffset, (GLenum *)type + typeOffset, 1);
	(*env)->ReleaseIntArrayElements(env, type_ref, type, 0);
	(*env)->ReleaseIntArrayElements(env, size_ref, size, 0);
	return output;
}

/*
 * AOSP marks these two broken (b/6006380): the name is a single byte, so the
 * call has nowhere to write it. Refusing them is what Android does too.
 */
JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetActiveAttrib__IIILjava_nio_IntBuffer_2Ljava_nio_IntBuffer_2Ljava_nio_IntBuffer_2B(JNIEnv *env, jclass, jint program, jint index, jint bufsize, jobject length_buf, jobject size_buf, jobject type_buf, jbyte name)
{
	throw_unsupported(env, "glGetActiveAttrib");
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetActiveUniform__IIILjava_nio_IntBuffer_2Ljava_nio_IntBuffer_2Ljava_nio_IntBuffer_2B(JNIEnv *env, jclass, jint program, jint index, jint bufsize, jobject length_buf, jobject size_buf, jobject type_buf, jbyte name)
{
	throw_unsupported(env, "glGetActiveUniform");
}

/* ---- object existence ---- */

JNIEXPORT jboolean JNICALL Java_android_opengl_GLES20_glIsBuffer(JNIEnv *env, jclass, jint buffer)
{
	return (jboolean)glIsBuffer((GLuint)buffer);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_GLES20_glIsEnabled(JNIEnv *env, jclass, jint cap)
{
	return (jboolean)glIsEnabled((GLenum)cap);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_GLES20_glIsFramebuffer(JNIEnv *env, jclass, jint framebuffer)
{
	return (jboolean)glIsFramebuffer((GLuint)framebuffer);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_GLES20_glIsProgram(JNIEnv *env, jclass, jint program)
{
	return (jboolean)glIsProgram((GLuint)program);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_GLES20_glIsRenderbuffer(JNIEnv *env, jclass, jint renderbuffer)
{
	return (jboolean)glIsRenderbuffer((GLuint)renderbuffer);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_GLES20_glIsShader(JNIEnv *env, jclass, jint shader)
{
	return (jboolean)glIsShader((GLuint)shader);
}

JNIEXPORT jboolean JNICALL Java_android_opengl_GLES20_glIsTexture(JNIEnv *env, jclass, jint texture)
{
	return (jboolean)glIsTexture((GLuint)texture);
}

/* ---- buffer objects ---- */

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenBuffers__ILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint n, jobject buffers_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *buffers = get_nio_buffer(env, buffers_buf, &array_ref, &array);
	glGenBuffers((GLsizei)n, (GLuint *)buffers);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteBuffers__I_3II(JNIEnv *env, jclass, jint n, jintArray buffers_ref, jint offset)
{
	jint *buffers = (*env)->GetIntArrayElements(env, buffers_ref, NULL);
	glDeleteBuffers((GLsizei)n, (GLuint *)buffers + offset);
	(*env)->ReleaseIntArrayElements(env, buffers_ref, buffers, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteBuffers__ILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint n, jobject buffers_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *buffers = get_nio_buffer(env, buffers_buf, &array_ref, &array);
	glDeleteBuffers((GLsizei)n, (GLuint *)buffers);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetBufferParameteriv__II_3II(JNIEnv *env, jclass, jint target, jint pname, jintArray params_ref, jint offset)
{
	jint *params = (*env)->GetIntArrayElements(env, params_ref, NULL);
	glGetBufferParameteriv((GLenum)target, (GLenum)pname, (GLint *)params + offset);
	(*env)->ReleaseIntArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetBufferParameteriv__IILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint target, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetBufferParameteriv((GLenum)target, (GLenum)pname, (GLint *)params);
	release_nio_buffer(env, array_ref, array);
}

/* ---- framebuffers and renderbuffers ---- */

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenFramebuffers__ILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint n, jobject framebuffers_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *framebuffers = get_nio_buffer(env, framebuffers_buf, &array_ref, &array);
	glGenFramebuffers((GLsizei)n, (GLuint *)framebuffers);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteFramebuffers__ILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint n, jobject framebuffers_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *framebuffers = get_nio_buffer(env, framebuffers_buf, &array_ref, &array);
	glDeleteFramebuffers((GLsizei)n, (GLuint *)framebuffers);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenRenderbuffers__I_3II(JNIEnv *env, jclass, jint n, jintArray renderbuffers_ref, jint offset)
{
	jint *renderbuffers = (*env)->GetIntArrayElements(env, renderbuffers_ref, NULL);
	glGenRenderbuffers((GLsizei)n, (GLuint *)renderbuffers + offset);
	(*env)->ReleaseIntArrayElements(env, renderbuffers_ref, renderbuffers, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenRenderbuffers__ILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint n, jobject renderbuffers_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *renderbuffers = get_nio_buffer(env, renderbuffers_buf, &array_ref, &array);
	glGenRenderbuffers((GLsizei)n, (GLuint *)renderbuffers);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteRenderbuffers__I_3II(JNIEnv *env, jclass, jint n, jintArray renderbuffers_ref, jint offset)
{
	jint *renderbuffers = (*env)->GetIntArrayElements(env, renderbuffers_ref, NULL);
	glDeleteRenderbuffers((GLsizei)n, (GLuint *)renderbuffers + offset);
	(*env)->ReleaseIntArrayElements(env, renderbuffers_ref, renderbuffers, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteRenderbuffers__ILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint n, jobject renderbuffers_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *renderbuffers = get_nio_buffer(env, renderbuffers_buf, &array_ref, &array);
	glDeleteRenderbuffers((GLsizei)n, (GLuint *)renderbuffers);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glRenderbufferStorage(JNIEnv *env, jclass, jint target, jint internalformat, jint width, jint height)
{
	glRenderbufferStorage((GLenum)target, (GLenum)internalformat, (GLsizei)width, (GLsizei)height);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glFramebufferRenderbuffer(JNIEnv *env, jclass, jint target, jint attachment, jint renderbuffertarget, jint renderbuffer)
{
	glFramebufferRenderbuffer((GLenum)target, (GLenum)attachment, (GLenum)renderbuffertarget, (GLuint)renderbuffer);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetFramebufferAttachmentParameteriv__III_3II(JNIEnv *env, jclass, jint target, jint attachment, jint pname, jintArray params_ref, jint offset)
{
	jint *params = (*env)->GetIntArrayElements(env, params_ref, NULL);
	glGetFramebufferAttachmentParameteriv((GLenum)target, (GLenum)attachment, (GLenum)pname, (GLint *)params + offset);
	(*env)->ReleaseIntArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetFramebufferAttachmentParameteriv__IIILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint target, jint attachment, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetFramebufferAttachmentParameteriv((GLenum)target, (GLenum)attachment, (GLenum)pname, (GLint *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetRenderbufferParameteriv__II_3II(JNIEnv *env, jclass, jint target, jint pname, jintArray params_ref, jint offset)
{
	jint *params = (*env)->GetIntArrayElements(env, params_ref, NULL);
	glGetRenderbufferParameteriv((GLenum)target, (GLenum)pname, (GLint *)params + offset);
	(*env)->ReleaseIntArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetRenderbufferParameteriv__IILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint target, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetRenderbufferParameteriv((GLenum)target, (GLenum)pname, (GLint *)params);
	release_nio_buffer(env, array_ref, array);
}

/* ---- textures ---- */

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGenTextures__ILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint n, jobject textures_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *textures = get_nio_buffer(env, textures_buf, &array_ref, &array);
	glGenTextures((GLsizei)n, (GLuint *)textures);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glDeleteTextures__ILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint n, jobject textures_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *textures = get_nio_buffer(env, textures_buf, &array_ref, &array);
	glDeleteTextures((GLsizei)n, (GLuint *)textures);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glCopyTexImage2D(JNIEnv *env, jclass, jint target, jint level, jint internalformat, jint x, jint y, jint width, jint height, jint border)
{
	glCopyTexImage2D((GLenum)target, (GLint)level, (GLenum)internalformat, (GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height, (GLint)border);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glCopyTexSubImage2D(JNIEnv *env, jclass, jint target, jint level, jint xoffset, jint yoffset, jint x, jint y, jint width, jint height)
{
	glCopyTexSubImage2D((GLenum)target, (GLint)level, (GLint)xoffset, (GLint)yoffset, (GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glCompressedTexImage2D(JNIEnv *env, jclass, jint target, jint level, jint internalformat, jint width, jint height, jint border, jint imageSize, jobject data_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *data = get_nio_buffer(env, data_buf, &array_ref, &array);
	glCompressedTexImage2D((GLenum)target, (GLint)level, (GLenum)internalformat, (GLsizei)width, (GLsizei)height, (GLint)border, (GLsizei)imageSize, data);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glCompressedTexSubImage2D(JNIEnv *env, jclass, jint target, jint level, jint xoffset, jint yoffset, jint width, jint height, jint format, jint imageSize, jobject data_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *data = get_nio_buffer(env, data_buf, &array_ref, &array);
	glCompressedTexSubImage2D((GLenum)target, (GLint)level, (GLint)xoffset, (GLint)yoffset, (GLsizei)width, (GLsizei)height, (GLenum)format, (GLsizei)imageSize, data);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexParameterfv__II_3FI(JNIEnv *env, jclass, jint target, jint pname, jfloatArray params_ref, jint offset)
{
	jfloat *params = (*env)->GetFloatArrayElements(env, params_ref, NULL);
	glTexParameterfv((GLenum)target, (GLenum)pname, (GLfloat *)params + offset);
	(*env)->ReleaseFloatArrayElements(env, params_ref, params, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexParameterfv__IILjava_nio_FloatBuffer_2(JNIEnv *env, jclass, jint target, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glTexParameterfv((GLenum)target, (GLenum)pname, (GLfloat *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexParameteriv__II_3II(JNIEnv *env, jclass, jint target, jint pname, jintArray params_ref, jint offset)
{
	jint *params = (*env)->GetIntArrayElements(env, params_ref, NULL);
	glTexParameteriv((GLenum)target, (GLenum)pname, (GLint *)params + offset);
	(*env)->ReleaseIntArrayElements(env, params_ref, params, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glTexParameteriv__IILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint target, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glTexParameteriv((GLenum)target, (GLenum)pname, (GLint *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetTexParameterfv__II_3FI(JNIEnv *env, jclass, jint target, jint pname, jfloatArray params_ref, jint offset)
{
	jfloat *params = (*env)->GetFloatArrayElements(env, params_ref, NULL);
	glGetTexParameterfv((GLenum)target, (GLenum)pname, (GLfloat *)params + offset);
	(*env)->ReleaseFloatArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetTexParameterfv__IILjava_nio_FloatBuffer_2(JNIEnv *env, jclass, jint target, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetTexParameterfv((GLenum)target, (GLenum)pname, (GLfloat *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetTexParameteriv__II_3II(JNIEnv *env, jclass, jint target, jint pname, jintArray params_ref, jint offset)
{
	jint *params = (*env)->GetIntArrayElements(env, params_ref, NULL);
	glGetTexParameteriv((GLenum)target, (GLenum)pname, (GLint *)params + offset);
	(*env)->ReleaseIntArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetTexParameteriv__IILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint target, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetTexParameteriv((GLenum)target, (GLenum)pname, (GLint *)params);
	release_nio_buffer(env, array_ref, array);
}

/* ---- global state queries ---- */

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetBooleanv__I_3ZI(JNIEnv *env, jclass, jint pname, jbooleanArray params_ref, jint offset)
{
	jboolean *params = (*env)->GetBooleanArrayElements(env, params_ref, NULL);
	glGetBooleanv((GLenum)pname, (GLboolean *)params + offset);
	(*env)->ReleaseBooleanArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetBooleanv__ILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	int remaining = get_nio_buffer_size(env, params_buf);

	/* Like AOSP: GL writes GLbooleans into the int buffer's own memory, then
	 * they are widened in place backwards so source and destination never
	 * overlap. */
	glGetBooleanv((GLenum)pname, (GLboolean *)params);
	for (int i = remaining - 1; i >= 0; i--)
		((jint *)params)[i] = ((GLboolean *)params)[i];

	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetIntegerv__ILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetIntegerv((GLenum)pname, (GLint *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetUniformfv__II_3FI(JNIEnv *env, jclass, jint program, jint location, jfloatArray params_ref, jint offset)
{
	jfloat *params = (*env)->GetFloatArrayElements(env, params_ref, NULL);
	glGetUniformfv((GLuint)program, (GLint)location, (GLfloat *)params + offset);
	(*env)->ReleaseFloatArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetUniformfv__IILjava_nio_FloatBuffer_2(JNIEnv *env, jclass, jint program, jint location, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetUniformfv((GLuint)program, (GLint)location, (GLfloat *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetUniformiv__II_3II(JNIEnv *env, jclass, jint program, jint location, jintArray params_ref, jint offset)
{
	jint *params = (*env)->GetIntArrayElements(env, params_ref, NULL);
	glGetUniformiv((GLuint)program, (GLint)location, (GLint *)params + offset);
	(*env)->ReleaseIntArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetUniformiv__IILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint program, jint location, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetUniformiv((GLuint)program, (GLint)location, (GLint *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetVertexAttribfv__II_3FI(JNIEnv *env, jclass, jint index, jint pname, jfloatArray params_ref, jint offset)
{
	jfloat *params = (*env)->GetFloatArrayElements(env, params_ref, NULL);
	glGetVertexAttribfv((GLuint)index, (GLenum)pname, (GLfloat *)params + offset);
	(*env)->ReleaseFloatArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetVertexAttribfv__IILjava_nio_FloatBuffer_2(JNIEnv *env, jclass, jint index, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetVertexAttribfv((GLuint)index, (GLenum)pname, (GLfloat *)params);
	release_nio_buffer(env, array_ref, array);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetVertexAttribiv__II_3II(JNIEnv *env, jclass, jint index, jint pname, jintArray params_ref, jint offset)
{
	jint *params = (*env)->GetIntArrayElements(env, params_ref, NULL);
	glGetVertexAttribiv((GLuint)index, (GLenum)pname, (GLint *)params + offset);
	(*env)->ReleaseIntArrayElements(env, params_ref, params, 0);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glGetVertexAttribiv__IILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint index, jint pname, jobject params_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *params = get_nio_buffer(env, params_buf, &array_ref, &array);
	glGetVertexAttribiv((GLuint)index, (GLenum)pname, (GLint *)params);
	release_nio_buffer(env, array_ref, array);
}

/* ---- uniforms ---- */

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform1f(JNIEnv *env, jclass, jint location, jfloat x)
{
	glUniform1f((GLint)location, (GLfloat)x);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform2f(JNIEnv *env, jclass, jint location, jfloat x, jfloat y)
{
	glUniform2f((GLint)location, (GLfloat)x, (GLfloat)y);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform3f(JNIEnv *env, jclass, jint location, jfloat x, jfloat y, jfloat z)
{
	glUniform3f((GLint)location, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform2i(JNIEnv *env, jclass, jint location, jint x, jint y)
{
	glUniform2i((GLint)location, (GLint)x, (GLint)y);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform3i(JNIEnv *env, jclass, jint location, jint x, jint y, jint z)
{
	glUniform3i((GLint)location, (GLint)x, (GLint)y, (GLint)z);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform4i(JNIEnv *env, jclass, jint location, jint x, jint y, jint z, jint w)
{
	glUniform4i((GLint)location, (GLint)x, (GLint)y, (GLint)z, (GLint)w);
}

#define UNIFORM_FV(n)                                                                                                                                                    \
	JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform##n##fv__II_3FI(JNIEnv *env, jclass, jint location, jint count, jfloatArray v_ref, jint offset)         \
	{                                                                                                                                                                \
		jfloat *v = (*env)->GetFloatArrayElements(env, v_ref, NULL);                                                                                              \
		glUniform##n##fv((GLint)location, (GLsizei)count, (GLfloat *)v + offset);                                                                                 \
		(*env)->ReleaseFloatArrayElements(env, v_ref, v, JNI_ABORT);                                                                                              \
	}                                                                                                                                                                \
                                                                                                                                                                         \
	JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform##n##fv__IILjava_nio_FloatBuffer_2(JNIEnv *env, jclass, jint location, jint count, jobject v_buf)       \
	{                                                                                                                                                                \
		jarray array_ref;                                                                                                                                        \
		jbyte *array;                                                                                                                                            \
		GLvoid *v = get_nio_buffer(env, v_buf, &array_ref, &array);                                                                                               \
		glUniform##n##fv((GLint)location, (GLsizei)count, (GLfloat *)v);                                                                                          \
		release_nio_buffer(env, array_ref, array);                                                                                                                \
	}

#define UNIFORM_IV(n)                                                                                                                                                    \
	JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform##n##iv__II_3II(JNIEnv *env, jclass, jint location, jint count, jintArray v_ref, jint offset)           \
	{                                                                                                                                                                \
		jint *v = (*env)->GetIntArrayElements(env, v_ref, NULL);                                                                                                  \
		glUniform##n##iv((GLint)location, (GLsizei)count, (GLint *)v + offset);                                                                                   \
		(*env)->ReleaseIntArrayElements(env, v_ref, v, JNI_ABORT);                                                                                                \
	}                                                                                                                                                                \
                                                                                                                                                                         \
	JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniform##n##iv__IILjava_nio_IntBuffer_2(JNIEnv *env, jclass, jint location, jint count, jobject v_buf)         \
	{                                                                                                                                                                \
		jarray array_ref;                                                                                                                                        \
		jbyte *array;                                                                                                                                            \
		GLvoid *v = get_nio_buffer(env, v_buf, &array_ref, &array);                                                                                               \
		glUniform##n##iv((GLint)location, (GLsizei)count, (GLint *)v);                                                                                            \
		release_nio_buffer(env, array_ref, array);                                                                                                                \
	}

UNIFORM_FV(1)
UNIFORM_FV(2)
UNIFORM_FV(3)
UNIFORM_FV(4)
UNIFORM_IV(1)
UNIFORM_IV(2)
UNIFORM_IV(3)
UNIFORM_IV(4)

#define UNIFORM_MATRIX_FV(n)                                                                                                                                                              \
	JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniformMatrix##n##fv__IIZ_3FI(JNIEnv *env, jclass, jint location, jint count, jboolean transpose, jfloatArray value_ref, jint offset) \
	{                                                                                                                                                                                 \
		jfloat *value = (*env)->GetFloatArrayElements(env, value_ref, NULL);                                                                                                       \
		glUniformMatrix##n##fv((GLint)location, (GLsizei)count, (GLboolean)transpose, (GLfloat *)value + offset);                                                                  \
		(*env)->ReleaseFloatArrayElements(env, value_ref, value, JNI_ABORT);                                                                                                       \
	}                                                                                                                                                                                 \
                                                                                                                                                                                          \
	JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniformMatrix##n##fv__IIZLjava_nio_FloatBuffer_2(JNIEnv *env, jclass, jint location, jint count, jboolean transpose, jobject value_buf) \
	{                                                                                                                                                                                 \
		jarray array_ref;                                                                                                                                                         \
		jbyte *array;                                                                                                                                                             \
		GLvoid *value = get_nio_buffer(env, value_buf, &array_ref, &array);                                                                                                        \
		glUniformMatrix##n##fv((GLint)location, (GLsizei)count, (GLboolean)transpose, (GLfloat *)value);                                                                           \
		release_nio_buffer(env, array_ref, array);                                                                                                                                 \
	}

UNIFORM_MATRIX_FV(2)
UNIFORM_MATRIX_FV(3)

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glUniformMatrix4fv__IIZLjava_nio_FloatBuffer_2(JNIEnv *env, jclass, jint location, jint count, jboolean transpose, jobject value_buf)
{
	jarray array_ref;
	jbyte *array;
	GLvoid *value = get_nio_buffer(env, value_buf, &array_ref, &array);
	glUniformMatrix4fv((GLint)location, (GLsizei)count, (GLboolean)transpose, (GLfloat *)value);
	release_nio_buffer(env, array_ref, array);
}

/* ---- vertex attributes ---- */

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glVertexAttrib1f(JNIEnv *env, jclass, jint indx, jfloat x)
{
	glVertexAttrib1f((GLuint)indx, (GLfloat)x);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glVertexAttrib2f(JNIEnv *env, jclass, jint indx, jfloat x, jfloat y)
{
	glVertexAttrib2f((GLuint)indx, (GLfloat)x, (GLfloat)y);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glVertexAttrib3f(JNIEnv *env, jclass, jint indx, jfloat x, jfloat y, jfloat z)
{
	glVertexAttrib3f((GLuint)indx, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

JNIEXPORT void JNICALL Java_android_opengl_GLES20_glVertexAttrib4f(JNIEnv *env, jclass, jint indx, jfloat x, jfloat y, jfloat z, jfloat w)
{
	glVertexAttrib4f((GLuint)indx, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

#define VERTEX_ATTRIB_FV(n)                                                                                                                              \
	JNIEXPORT void JNICALL Java_android_opengl_GLES20_glVertexAttrib##n##fv__I_3FI(JNIEnv *env, jclass, jint indx, jfloatArray values_ref, jint offset) \
	{                                                                                                                                                \
		jfloat *values = (*env)->GetFloatArrayElements(env, values_ref, NULL);                                                                    \
		glVertexAttrib##n##fv((GLuint)indx, (GLfloat *)values + offset);                                                                          \
		(*env)->ReleaseFloatArrayElements(env, values_ref, values, JNI_ABORT);                                                                    \
	}                                                                                                                                                \
                                                                                                                                                         \
	JNIEXPORT void JNICALL Java_android_opengl_GLES20_glVertexAttrib##n##fv__ILjava_nio_FloatBuffer_2(JNIEnv *env, jclass, jint indx, jobject values_buf) \
	{                                                                                                                                                \
		jarray array_ref;                                                                                                                        \
		jbyte *array;                                                                                                                            \
		GLvoid *values = get_nio_buffer(env, values_buf, &array_ref, &array);                                                                     \
		glVertexAttrib##n##fv((GLuint)indx, (GLfloat *)values);                                                                                   \
		release_nio_buffer(env, array_ref, array);                                                                                                \
	}

VERTEX_ATTRIB_FV(1)
VERTEX_ATTRIB_FV(2)
VERTEX_ATTRIB_FV(3)
VERTEX_ATTRIB_FV(4)
