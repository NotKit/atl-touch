#include "../defines.h"
#include "ATLCanvas.h"

extern "C" {
#include "../generated_headers/android_view_RenderNode.h"
}

/*
 * RenderNode display lists are recorded into an ATLCanvas (recording mode)
 * and finished into an ATLNode (a patchable SkDrawable tree, see ATLCanvas.h).
 * Child render nodes are recorded as stub drawables which can be retargeted
 * later without re-recording the parent, mirroring what nativePatchNode did
 * with the immutable GSK render node tree.
 */

JNIEXPORT jlong JNICALL Java_android_view_RenderNode_nativeCreateSnapshot(JNIEnv *env, jobject thiz)
{
	return _INTPTR(ATLCanvas::new_recording());
}

JNIEXPORT jlong JNICALL Java_android_view_RenderNode_nativeCreateNode(JNIEnv *env, jobject thiz, jlong snapshot_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	ATLNode *node = new ATLNode();
	node->recording = atl_canvas->recorder->finishRecordingAsDrawable();
	node->stubs = std::move(atl_canvas->stubs);
	delete atl_canvas;
	return _INTPTR(node);
}

JNIEXPORT jlong JNICALL Java_android_view_RenderNode_nativePatchNode(JNIEnv *env, jobject thiz, jlong node_ptr, jlong old_child_ptr, jlong new_child_ptr)
{
	ATLNode *node = (ATLNode *)_PTR(node_ptr);
	node->patch((ATLNode *)_PTR(old_child_ptr), (ATLNode *)_PTR(new_child_ptr));
	/* stubs are retargeted in place; the node itself is returned unchanged
	 * (the caller passes back the ref it already owns) */
	return _INTPTR(node);
}

JNIEXPORT jlong JNICALL Java_android_view_RenderNode_nativeTransform(JNIEnv *env, jobject thiz, jlong node_ptr, jfloat scale_x, jfloat scale_y, jfloat translate_x, jfloat translate_y, jfloat rotate, jfloat pivot_x, jfloat pivot_y)
{
	ATLNode *node = (ATLNode *)_PTR(node_ptr);
	ATLNode *transformed = new ATLNode();
	transformed->child = sk_ref_sp(node);
	/* same composition order as the GSK transform this replaces:
	 * translate(-pivot), scale, rotate, translate(translate + pivot),
	 * with the last operation applied to points first */
	transformed->matrix.setTranslate(-pivot_x, -pivot_y);
	transformed->matrix.preScale(scale_x, scale_y);
	transformed->matrix.preRotate(rotate);
	transformed->matrix.preTranslate(translate_x + pivot_x, translate_y + pivot_y);
	return _INTPTR(transformed);
}

JNIEXPORT jlong JNICALL Java_android_view_RenderNode_nativeClip(JNIEnv *env, jobject thiz, jlong node_ptr, jfloat left, jfloat top, jfloat right, jfloat bottom)
{
	ATLNode *node = (ATLNode *)_PTR(node_ptr);
	ATLNode *clipped = new ATLNode();
	clipped->child = sk_sp<ATLNode>(node); // takes over the caller's ref
	clipped->has_clip = true;
	clipped->clip = SkRect::MakeLTRB(left, top, right, bottom);
	return _INTPTR(clipped);
}

JNIEXPORT void JNICALL Java_android_view_RenderNode_nativeUnref(JNIEnv *env, jobject thiz, jlong node_ptr)
{
	if (node_ptr)
		((ATLNode *)_PTR(node_ptr))->unref();
}

JNIEXPORT jlong JNICALL Java_android_view_RenderNode_nativeAddStubNode(JNIEnv *env, jobject thiz, jlong snapshot_ptr)
{
	ATLCanvas *atl_canvas = (ATLCanvas *)_PTR(snapshot_ptr);
	sk_sp<ATLNode> stub(new ATLNode());
	stub->is_stub = true;
	atl_canvas->canvas->drawDrawable(stub.get());
	ATLNode *result = stub.get();
	atl_canvas->stubs.push_back(std::move(stub));
	return _INTPTR(result);
}
