package android.view.animation;

import android.content.Context;

public class AnimationUtils {
	public static Animation loadAnimation(Context context, int dummy) { return new Animation(); }

	public static long currentAnimationTimeMillis() {
		return System.currentTimeMillis();
	}

	public static Interpolator loadInterpolator(Context context, int id) {
		// Returning null here crashed callers that immediately invoke
		// getInterpolation() on the result (e.g. Material visibility animations:
		// "invoke ... getInterpolation(float) on a null object reference"). ATL's
		// interpolators are currently linear pass-throughs, so the specific curve
		// in the resource doesn't matter yet — just return a valid, non-null one.
		return new AccelerateDecelerateInterpolator();
	}
}
