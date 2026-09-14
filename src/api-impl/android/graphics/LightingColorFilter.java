package android.graphics;

public class LightingColorFilter extends ColorFilter {
	private final int mMul;
	private final int mAdd;

	public LightingColorFilter(int mul, int add) {
		mMul = mul;
		mAdd = add;
	}

	public int getColorMultiply() { return mMul; }
	public int getColorAdd() { return mAdd; }

	@Override
	public long getNativeInstance() {
		try {
			return native_CreateLightingFilter(mMul, mAdd);
		} catch (UnsatisfiedLinkError e) {
			return 0; // an older native half: draw untinted rather than die
		}
	}

	private static native long native_CreateLightingFilter(int mul, int add);
}
