package android.os;

/**
 * A haptic effect. ATL's motor only knows on/off durations, so an effect is
 * reduced to the off/on pattern Vibrator.vibrate(long[], int) takes; amplitude
 * and composition primitives are lost. The classes and the constants have to
 * exist regardless, because apps build their effects in a static initializer
 * (Google Camera's shutter button does).
 */
public class VibrationEffect {

	/* short pulses standing in for the predefined effects and primitives */
	private static final long TICK_MS = 10;
	private static final long CLICK_MS = 20;
	private static final long HEAVY_CLICK_MS = 30;

	/* off, on, off, on... in milliseconds */
	final long[] pattern;

	VibrationEffect(long[] pattern) {
		this.pattern = pattern;
	}

	public static final int DEFAULT_AMPLITUDE = -1;
	public static final int EFFECT_CLICK = 0;
	public static final int EFFECT_DOUBLE_CLICK = 1;
	public static final int EFFECT_TICK = 2;
	public static final int EFFECT_THUD = 3;
	public static final int EFFECT_POP = 4;
	public static final int EFFECT_HEAVY_CLICK = 5;

	public static VibrationEffect createOneShot(long milliseconds, int amplitude) {
		if (milliseconds <= 0)
			throw new IllegalArgumentException("duration must be positive");
		return new VibrationEffect(new long[] {0, milliseconds});
	}

	public static VibrationEffect createWaveform(long[] pattern, int repeat) {
		return new VibrationEffect(pattern.clone());
	}

	public static VibrationEffect createWaveform(long[] timings, int[] amplitudes, int repeat) {
		if (timings.length != amplitudes.length)
			throw new IllegalArgumentException("timing and amplitude arrays must be of equal length");
		// merge the segments into alternating off/on runs; amplitude 0 is off
		long[] pattern = new long[timings.length + 1];
		int n = 0;
		boolean on = false;
		for (int i = 0; i < timings.length; i++) {
			boolean segmentOn = amplitudes[i] != 0;
			if (segmentOn != on) {
				n++;
				on = segmentOn;
			}
			pattern[n] += timings[i];
		}
		return new VibrationEffect(java.util.Arrays.copyOf(pattern, n + 1));
	}

	public static VibrationEffect createPredefined(int effectId) {
		switch (effectId) {
			case EFFECT_TICK:
				return new VibrationEffect(new long[] {0, TICK_MS});
			case EFFECT_DOUBLE_CLICK:
				return new VibrationEffect(new long[] {0, CLICK_MS, 100, CLICK_MS});
			case EFFECT_HEAVY_CLICK:
			case EFFECT_THUD:
				return new VibrationEffect(new long[] {0, HEAVY_CLICK_MS});
			default:
				return new VibrationEffect(new long[] {0, CLICK_MS});
		}
	}

	public static Composition startComposition() {
		return new Composition();
	}

	public static final class Composition {
		public static final int PRIMITIVE_NOOP = 0;
		public static final int PRIMITIVE_CLICK = 1;
		public static final int PRIMITIVE_THUD = 2;
		public static final int PRIMITIVE_SPIN = 3;
		public static final int PRIMITIVE_QUICK_RISE = 4;
		public static final int PRIMITIVE_SLOW_RISE = 5;
		public static final int PRIMITIVE_QUICK_FALL = 6;
		public static final int PRIMITIVE_TICK = 7;
		public static final int PRIMITIVE_LOW_TICK = 8;

		Composition() {
		}

		public Composition addPrimitive(int primitiveId) {
			return this;
		}

		public Composition addPrimitive(int primitiveId, float scale) {
			return this;
		}

		public Composition addPrimitive(int primitiveId, float scale, int delay) {
			return this;
		}

		public VibrationEffect compose() {
			return new VibrationEffect(new long[] {0, TICK_MS});
		}
	}
}
