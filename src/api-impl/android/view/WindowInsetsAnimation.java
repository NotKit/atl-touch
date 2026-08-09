package android.view;

import android.graphics.Insets;
import android.view.animation.Interpolator;

import java.util.List;

/* Nothing here ever animates — there are no system bars, and the IME inset is
 * applied in one step. The class exists in full because androidx.core subclasses
 * Callback (WindowInsetsAnimationCompat$Impl30$ProxyCallback), which fails to
 * verify unless every type in the overridden signatures resolves. */
public class WindowInsetsAnimation {

	private final int typeMask;
	private final Interpolator interpolator;
	private final long durationMillis;
	private float fraction;
	private float alpha = 1f;

	public WindowInsetsAnimation(int typeMask, Interpolator interpolator, long durationMillis) {
		this.typeMask = typeMask;
		this.interpolator = interpolator;
		this.durationMillis = durationMillis;
	}

	public int getTypeMask() {
		return typeMask;
	}

	public float getFraction() {
		return fraction;
	}

	public void setFraction(float fraction) {
		this.fraction = fraction;
	}

	public float getInterpolatedFraction() {
		return interpolator != null ? interpolator.getInterpolation(fraction) : fraction;
	}

	public Interpolator getInterpolator() {
		return interpolator;
	}

	public long getDurationMillis() {
		return durationMillis;
	}

	public float getAlpha() {
		return alpha;
	}

	public void setAlpha(float alpha) {
		this.alpha = alpha;
	}

	public static final class Bounds {

		private final Insets lowerBound;
		private final Insets upperBound;

		public Bounds(Insets lowerBound, Insets upperBound) {
			this.lowerBound = lowerBound;
			this.upperBound = upperBound;
		}

		public Insets getLowerBound() {
			return lowerBound;
		}

		public Insets getUpperBound() {
			return upperBound;
		}

		public Bounds inset(Insets insets) {
			return new Bounds(inset(lowerBound, insets), inset(upperBound, insets));
		}

		private static Insets inset(Insets bound, Insets insets) {
			return Insets.of(Math.max(0, bound.left - insets.left), Math.max(0, bound.top - insets.top),
			                 Math.max(0, bound.right - insets.right),
			                 Math.max(0, bound.bottom - insets.bottom));
		}
	}

	public abstract static class Callback {

		public static final int DISPATCH_MODE_STOP = 0;
		public static final int DISPATCH_MODE_CONTINUE_ON_SUBTREE = 1;

		private final int dispatchMode;

		public Callback(int dispatchMode) {
			this.dispatchMode = dispatchMode;
		}

		public final int getDispatchMode() {
			return dispatchMode;
		}

		public void onPrepare(WindowInsetsAnimation animation) {}

		public Bounds onStart(WindowInsetsAnimation animation, Bounds bounds) {
			return bounds;
		}

		public abstract WindowInsets onProgress(WindowInsets insets,
		                                        List<WindowInsetsAnimation> runningAnimations);

		public void onEnd(WindowInsetsAnimation animation) {}
	}
}
