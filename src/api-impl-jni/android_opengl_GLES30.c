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
