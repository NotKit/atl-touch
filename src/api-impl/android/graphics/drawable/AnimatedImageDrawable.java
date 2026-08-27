package android.graphics.drawable;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.PixelFormat;

/**
 * A stub. ImageDecoder never hands one of these out here, so nothing can hold
 * an instance; the class exists because callers test for it - androidx's
 * DrawablePainter does an instanceof on every draw, and a missing class turns
 * that into a NoClassDefFoundError that kills the whole draw pass.
 */
public class AnimatedImageDrawable extends Drawable implements Animatable2 {

	public static final int REPEAT_INFINITE = -1;
	public static final int LOOP_INFINITE = REPEAT_INFINITE;

	@Override
	public void draw(Canvas canvas) {
	}

	@Override
	public void setAlpha(int alpha) {
	}

	@Override
	public void setColorFilter(ColorFilter colorFilter) {
	}

	@Override
	public int getOpacity() {
		return PixelFormat.TRANSLUCENT;
	}

	@Override
	public void start() {
	}

	@Override
	public void stop() {
	}

	@Override
	public boolean isRunning() {
		return false;
	}

	@Override
	public void registerAnimationCallback(AnimationCallback callback) {
	}

	@Override
	public boolean unregisterAnimationCallback(AnimationCallback callback) {
		return false;
	}

	@Override
	public void clearAnimationCallbacks() {
	}

	public int getRepeatCount() {
		return 0;
	}

	public void setRepeatCount(int repeatCount) {
	}
}
