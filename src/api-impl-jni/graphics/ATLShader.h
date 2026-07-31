#pragma once

#include "include/core/SkBlendMode.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkShader.h"

/* Native backing for android.graphics.Shader and its gradient subclasses.
 * Keeps the base (untransformed) shader plus a local matrix that callers
 * (e.g. VectorDrawable) set per-draw; effective() composes the two. */
struct ATLShader {
	sk_sp<SkShader> base;
	SkMatrix localMatrix = SkMatrix::I();

	/* A ComposeShader keeps its two children rather than a finished base:
	 * callers set the children's local matrices after composing them, so the
	 * blend has to be rebuilt whenever it is used. Paint hands effective() to
	 * skia on every draw, so that is where it happens. */
	ATLShader *compose_dst = nullptr;
	ATLShader *compose_src = nullptr;
	SkBlendMode compose_mode = SkBlendMode::kSrcOver;

	sk_sp<SkShader> effective() {
		sk_sp<SkShader> shader = base;
		if (compose_dst != nullptr && compose_src != nullptr) {
			sk_sp<SkShader> dst = compose_dst->effective();
			sk_sp<SkShader> src = compose_src->effective();
			/* one side unbacked (a shader type with no native instance): blend
			 * with nothing is nothing, so fall back to the side that exists */
			shader = (dst && src) ? SkShaders::Blend(compose_mode, dst, src)
			                      : (dst ? dst : src);
		}
		if (!shader || localMatrix.isIdentity())
			return shader;
		return shader->makeWithLocalMatrix(localMatrix);
	}
};
