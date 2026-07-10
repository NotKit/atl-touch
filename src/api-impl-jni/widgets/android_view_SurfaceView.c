#include "../defines.h"
#include "../util.h"

#include "../graphics/ATLCanvas.h"

#include "../generated_headers/android_view_SurfaceView.h"

/* SurfaceHolder.lockCanvas: an offscreen raster canvas; the finished frame is
 * detached as a Bitmap (native_canvas_to_bitmap in android_graphics_Bitmap.cpp)
 * and drawn into the scene by SurfaceView.onDraw. MediaCodec video frames
 * arrive through Surface.postFrame the same way. */
JNIEXPORT jlong JNICALL Java_android_view_SurfaceView_native_1createSnapshot(JNIEnv *env, jobject this, jint width, jint height)
{
	return _INTPTR(atl_canvas_new_raster(width, height));
}
