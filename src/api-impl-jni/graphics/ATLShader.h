#pragma once

#include "include/core/SkMatrix.h"
#include "include/core/SkShader.h"

/* Native backing for android.graphics.Shader and its gradient subclasses.
 * Keeps the base (untransformed) shader plus a local matrix that callers
 * (e.g. VectorDrawable) set per-draw; effective() composes the two. */
struct ATLShader {
	sk_sp<SkShader> base;
	SkMatrix localMatrix = SkMatrix::I();

	sk_sp<SkShader> effective() {
		if (!base || localMatrix.isIdentity())
			return base;
		return base->makeWithLocalMatrix(localMatrix);
	}
};
