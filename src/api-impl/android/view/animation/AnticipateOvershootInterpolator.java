package android.view.animation;

import android.animation.TimeInterpolator;

public class AnticipateOvershootInterpolator implements TimeInterpolator {

	private final float tension;

	public AnticipateOvershootInterpolator() {
		tension = 2.0f * 1.5f;
	}

	public AnticipateOvershootInterpolator(float tension) {
		this.tension = tension * 1.5f;
	}

	public AnticipateOvershootInterpolator(float tension, float extraTension) {
		this.tension = tension * extraTension;
	}

	private static float a(float t, float s) {
		return t * t * ((s + 1) * t - s);
	}

	private static float o(float t, float s) {
		return t * t * ((s + 1) * t + s);
	}

	@Override
	public float getInterpolation(float t) {
		if (t < 0.5f)
			return 0.5f * a(t * 2.0f, tension);
		else
			return 0.5f * (o(t * 2.0f - 2.0f, tension) + 2.0f);
	}
}
