package android.graphics;

public class ComposeShader extends Shader {

	/* held so the children cannot be collected before this shader: the native
	 * side keeps their pointers and rebuilds the blend from them on each draw */
	private final Shader shaderA;
	private final Shader shaderB;

	public ComposeShader(Shader shaderA, Shader shaderB, PorterDuff.Mode mode) {
		this(shaderA, shaderB, mode.nativeInt);
	}

	public ComposeShader(Shader shaderA, Shader shaderB, BlendMode mode) {
		this(shaderA, shaderB, porterDuffMode(mode));
	}

	private ComposeShader(Shader shaderA, Shader shaderB, int porterDuffMode) {
		this.shaderA = shaderA;
		this.shaderB = shaderB;
		init(native_create(shaderA.getNativeInstance(), shaderB.getNativeInstance(), porterDuffMode));
	}

	/* the modes BlendMode adds over PorterDuff have no legacy int; SRC_OVER is
	 * what the native side falls back to for anything it does not know */
	private static int porterDuffMode(BlendMode mode) {
		PorterDuff.Mode legacy = BlendMode.blendModeToPorterDuffMode(mode);
		return legacy != null ? legacy.nativeInt : PorterDuff.Mode.SRC_OVER.nativeInt;
	}

	private static native long native_create(long shaderA, long shaderB, int porterDuffMode);
}
