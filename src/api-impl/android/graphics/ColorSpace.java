package android.graphics;

import java.util.function.DoubleUnaryOperator;

public class ColorSpace {
	private final String name;

	public ColorSpace() {
		this("sRGB IEC61966-2.1");
	}

	ColorSpace(String name) {
		this.name = name;
	}

	public String getName() {
		return name;
	}

	/** native SkColorSpace handle; 0 = treat as sRGB */
	public long getNativeInstance() {
		return 0;
	}


	public static enum Named {
		SRGB,
	}

	public static ColorSpace get(Named named) {
		return new ColorSpace();
	}

	/**
	 * An RGB color space defined by primaries, a white point and a transfer
	 * function.
	 *
	 * Honest stub: it stores what it is handed and converts nothing. None of
	 * the AOSP colorimetry is here -- no toXyz/fromXyz, no chromatic
	 * adaptation, and no matrix computed from primaries and white point, so
	 * getTransform() answers only when the caller supplied the matrix itself.
	 * getNativeInstance() still reports 0, i.e. everything is drawn as sRGB.
	 *
	 * It exists because Compose's ImageBitmap factory names it: the whole of
	 * androidx.compose.ui.graphics.ImageBitmapKt fails verification without
	 * the type, which takes out every vector icon.
	 */
	public static class Rgb extends ColorSpace {
		private final float[] primaries;
		private final float[] whitePoint;
		private final float[] transform;
		private final TransferParameters transferParameters;
		private final DoubleUnaryOperator oetf;
		private final DoubleUnaryOperator eotf;

		public Rgb(String name, float[] toXYZ, TransferParameters function) {
			this(name, null, null, toXYZ, function, null, null);
		}

		public Rgb(String name, float[] primaries, float[] whitePoint, TransferParameters function) {
			this(name, primaries, whitePoint, null, function, null, null);
		}

		public Rgb(String name, float[] primaries, float[] whitePoint,
		           DoubleUnaryOperator oetf, DoubleUnaryOperator eotf, float min, float max) {
			this(name, primaries, whitePoint, null, null, oetf, eotf);
		}

		private Rgb(String name, float[] primaries, float[] whitePoint, float[] transform,
		            TransferParameters function, DoubleUnaryOperator oetf, DoubleUnaryOperator eotf) {
			super(name);
			this.primaries = primaries;
			this.whitePoint = whitePoint;
			this.transform = transform;
			this.transferParameters = function;
			this.oetf = oetf;
			this.eotf = eotf;
		}

		public float[] getPrimaries() {
			return primaries;
		}

		public float[] getWhitePoint() {
			return whitePoint;
		}

		/** the RGB-to-XYZ matrix, or null if we were given primaries instead */
		public float[] getTransform() {
			return transform;
		}

		public TransferParameters getTransferParameters() {
			return transferParameters;
		}

		public DoubleUnaryOperator getOetf() {
			return oetf;
		}

		public DoubleUnaryOperator getEotf() {
			return eotf;
		}

		/** The seven parameters of a parametric ICC transfer function. */
		public static class TransferParameters {
			public final double a;
			public final double b;
			public final double c;
			public final double d;
			public final double e;
			public final double f;
			public final double g;

			public TransferParameters(double a, double b, double c, double d, double g) {
				this(a, b, c, d, 0.0, 0.0, g);
			}

			public TransferParameters(double a, double b, double c, double d, double e, double f, double g) {
				this.a = a;
				this.b = b;
				this.c = c;
				this.d = d;
				this.e = e;
				this.f = f;
				this.g = g;
			}
		}
	}
}
