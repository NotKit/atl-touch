#pragma once

#include "include/core/SkBlendMode.h"

/* PorterDuff.Mode nativeInt values (see android.graphics.PorterDuff). The
 * legacy ints do not match SkBlendMode's ordering past kXor, so this is a
 * table rather than a cast. */
static inline SkBlendMode porter_duff_to_blend_mode(int mode)
{
	switch (mode) {
		case 0: return SkBlendMode::kClear;
		case 1: return SkBlendMode::kSrc;
		case 2: return SkBlendMode::kDst;
		case 3: return SkBlendMode::kSrcOver;
		case 4: return SkBlendMode::kDstOver;
		case 5: return SkBlendMode::kSrcIn;
		case 6: return SkBlendMode::kDstIn;
		case 7: return SkBlendMode::kSrcOut;
		case 8: return SkBlendMode::kDstOut;
		case 9: return SkBlendMode::kSrcATop;
		case 10: return SkBlendMode::kDstATop;
		case 11: return SkBlendMode::kXor;
		case 12: return SkBlendMode::kDarken;
		case 13: return SkBlendMode::kLighten;
		case 14: return SkBlendMode::kModulate; // PorterDuff MULTIPLY
		case 15: return SkBlendMode::kScreen;
		case 16: return SkBlendMode::kPlus; // PorterDuff ADD
		case 17: return SkBlendMode::kOverlay;
		default: return SkBlendMode::kSrcOver;
	}
}
