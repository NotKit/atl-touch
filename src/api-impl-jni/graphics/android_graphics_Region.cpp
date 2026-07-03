#include "../defines.h"

#include "include/core/SkPath.h"
#include "include/core/SkRegion.h"
#include "include/core/SkString.h"

extern "C" {
#include "../generated_headers/android_graphics_Region.h"
#include "../generated_headers/android_graphics_RegionIterator.h"
}

/* android.graphics.Region natives over SkRegion, adapted from AOSP
 * libs/hwui/jni/android_graphics_Region.cpp. */

static SkRegion *region_from_object(JNIEnv *env, jobject region_obj)
{
	jclass region_class = env->GetObjectClass(region_obj);
	static jfieldID field = env->GetFieldID(region_class, "mNativeRegion", "J");
	return reinterpret_cast<SkRegion *>(env->GetLongField(region_obj, field));
}

static inline SkRegion *to_region(jlong ptr)
{
	return reinterpret_cast<SkRegion *>(ptr);
}

JNIEXPORT jlong JNICALL Java_android_graphics_Region_nativeConstructor(JNIEnv *, jclass)
{
	return reinterpret_cast<jlong>(new SkRegion);
}

JNIEXPORT void JNICALL Java_android_graphics_Region_nativeDestructor(JNIEnv *, jclass, jlong region_ptr)
{
	delete to_region(region_ptr);
}

JNIEXPORT void JNICALL Java_android_graphics_Region_nativeSetRegion(JNIEnv *, jclass, jlong dst_ptr, jlong src_ptr)
{
	*to_region(dst_ptr) = *to_region(src_ptr);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_nativeSetRect(JNIEnv *, jclass, jlong dst_ptr,
	jint left, jint top, jint right, jint bottom)
{
	return to_region(dst_ptr)->setRect(SkIRect::MakeLTRB(left, top, right, bottom));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_nativeSetPath(JNIEnv *, jclass, jlong dst_ptr,
	jlong path_ptr, jlong clip_ptr)
{
	return to_region(dst_ptr)->setPath(*reinterpret_cast<SkPath *>(path_ptr), *to_region(clip_ptr));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_nativeGetBounds(JNIEnv *env, jclass, jlong region_ptr,
	jobject rect)
{
	const SkIRect &bounds = to_region(region_ptr)->getBounds();
	jclass rect_class = env->GetObjectClass(rect);
	env->SetIntField(rect, env->GetFieldID(rect_class, "left", "I"), bounds.fLeft);
	env->SetIntField(rect, env->GetFieldID(rect_class, "top", "I"), bounds.fTop);
	env->SetIntField(rect, env->GetFieldID(rect_class, "right", "I"), bounds.fRight);
	env->SetIntField(rect, env->GetFieldID(rect_class, "bottom", "I"), bounds.fBottom);
	return !to_region(region_ptr)->isEmpty();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_nativeGetBoundaryPath(JNIEnv *, jclass, jlong region_ptr,
	jlong path_ptr)
{
	return to_region(region_ptr)->getBoundaryPath(reinterpret_cast<SkPath *>(path_ptr));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_nativeOp__JIIIII(JNIEnv *, jclass, jlong dst_ptr,
	jint left, jint top, jint right, jint bottom, jint op)
{
	SkIRect rect = SkIRect::MakeLTRB(left, top, right, bottom);
	return to_region(dst_ptr)->op(rect, static_cast<SkRegion::Op>(op));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_nativeOp__JLandroid_graphics_Rect_2JI(JNIEnv *env, jclass,
	jlong dst_ptr, jobject rect, jlong region_ptr, jint op)
{
	jclass rect_class = env->GetObjectClass(rect);
	SkIRect ir = SkIRect::MakeLTRB(
	    env->GetIntField(rect, env->GetFieldID(rect_class, "left", "I")),
	    env->GetIntField(rect, env->GetFieldID(rect_class, "top", "I")),
	    env->GetIntField(rect, env->GetFieldID(rect_class, "right", "I")),
	    env->GetIntField(rect, env->GetFieldID(rect_class, "bottom", "I")));
	SkRegion rect_region;
	rect_region.setRect(ir);
	return to_region(dst_ptr)->op(rect_region, *to_region(region_ptr), static_cast<SkRegion::Op>(op));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_nativeOp__JJJI(JNIEnv *, jclass, jlong dst_ptr,
	jlong region1_ptr, jlong region2_ptr, jint op)
{
	return to_region(dst_ptr)->op(*to_region(region1_ptr), *to_region(region2_ptr),
	                              static_cast<SkRegion::Op>(op));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_nativeEquals(JNIEnv *, jclass, jlong r1_ptr, jlong r2_ptr)
{
	return *to_region(r1_ptr) == *to_region(r2_ptr);
}

JNIEXPORT jstring JNICALL Java_android_graphics_Region_nativeToString(JNIEnv *env, jclass, jlong region_ptr)
{
	SkString str;
	str.append("SkRegion(");
	SkRegion::Iterator iter(*to_region(region_ptr));
	while (!iter.done()) {
		const SkIRect &r = iter.rect();
		str.appendf("(%d,%d,%d,%d)", r.fLeft, r.fTop, r.fRight, r.fBottom);
		iter.next();
	}
	str.append(")");
	return env->NewStringUTF(str.c_str());
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_isEmpty(JNIEnv *env, jobject region)
{
	return region_from_object(env, region)->isEmpty();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_isRect(JNIEnv *env, jobject region)
{
	return region_from_object(env, region)->isRect();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_isComplex(JNIEnv *env, jobject region)
{
	return region_from_object(env, region)->isComplex();
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_contains(JNIEnv *env, jobject region, jint x, jint y)
{
	return region_from_object(env, region)->contains(x, y);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_quickContains(JNIEnv *env, jobject region,
	jint left, jint top, jint right, jint bottom)
{
	return region_from_object(env, region)->quickContains(SkIRect::MakeLTRB(left, top, right, bottom));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_quickReject__IIII(JNIEnv *env, jobject region,
	jint left, jint top, jint right, jint bottom)
{
	return region_from_object(env, region)->quickReject(SkIRect::MakeLTRB(left, top, right, bottom));
}

JNIEXPORT jboolean JNICALL Java_android_graphics_Region_quickReject__Landroid_graphics_Region_2(JNIEnv *env,
	jobject region, jobject other)
{
	return region_from_object(env, region)->quickReject(*region_from_object(env, other));
}

JNIEXPORT void JNICALL Java_android_graphics_Region_translate(JNIEnv *env, jobject region,
	jint dx, jint dy, jobject dst)
{
	SkRegion *self = region_from_object(env, region);
	if (dst)
		self->translate(dx, dy, region_from_object(env, dst));
	else
		self->translate(dx, dy);
}

// Scale the rectangle by given scale and set the reuslt to the dst.
static void scale_rect(SkIRect *dst, const SkIRect &src, float scale)
{
	dst->fLeft = (int)::roundf(src.fLeft * scale);
	dst->fTop = (int)::roundf(src.fTop * scale);
	dst->fRight = (int)::roundf(src.fRight * scale);
	dst->fBottom = (int)::roundf(src.fBottom * scale);
}

// Scale the region by given scale and set the result to the dst.
// dest and src can be the same region instance.
static void scale_region(SkRegion *dst, const SkRegion &src, float scale)
{
	SkRegion tmp;
	SkRegion::Iterator iter(src);

	while (!iter.done()) {
		SkIRect r;
		scale_rect(&r, iter.rect(), scale);
		tmp.op(r, SkRegion::kUnion_Op);
		iter.next();
	}

	dst->swap(tmp);
}

JNIEXPORT void JNICALL Java_android_graphics_Region_scale(JNIEnv *env, jobject region,
	jfloat scale, jobject dst)
{
	SkRegion *self = region_from_object(env, region);
	if (dst)
		scale_region(region_from_object(env, dst), *self, scale);
	else
		scale_region(self, *self, scale);
}

/* android.graphics.RegionIterator */

JNIEXPORT jlong JNICALL Java_android_graphics_RegionIterator_nativeConstructor(JNIEnv *, jclass, jlong region_ptr)
{
	return reinterpret_cast<jlong>(new SkRegion::Iterator(*to_region(region_ptr)));
}

JNIEXPORT void JNICALL Java_android_graphics_RegionIterator_nativeDestructor(JNIEnv *, jclass, jlong iter_ptr)
{
	delete reinterpret_cast<SkRegion::Iterator *>(iter_ptr);
}

JNIEXPORT jboolean JNICALL Java_android_graphics_RegionIterator_nativeNext(JNIEnv *env, jclass, jlong iter_ptr,
	jobject rect)
{
	SkRegion::Iterator *iter = reinterpret_cast<SkRegion::Iterator *>(iter_ptr);
	if (iter->done())
		return false;
	const SkIRect &r = iter->rect();
	jclass rect_class = env->GetObjectClass(rect);
	env->SetIntField(rect, env->GetFieldID(rect_class, "left", "I"), r.fLeft);
	env->SetIntField(rect, env->GetFieldID(rect_class, "top", "I"), r.fTop);
	env->SetIntField(rect, env->GetFieldID(rect_class, "right", "I"), r.fRight);
	env->SetIntField(rect, env->GetFieldID(rect_class, "bottom", "I"), r.fBottom);
	iter->next();
	return true;
}
