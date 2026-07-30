package android.graphics;

/**
 * API 31 effect applied to a {@link RenderNode}. Nothing renders a display list
 * here, so an effect is a token: apps build one, hand it to a RenderNode and get
 * an unfiltered result.
 */
public final class RenderEffect {

	private RenderEffect() {}

	public static RenderEffect createBlurEffect(float radiusX, float radiusY, Shader.TileMode edgeTreatment) {
		return new RenderEffect();
	}

	public static RenderEffect createBlurEffect(float radiusX, float radiusY, RenderEffect inputEffect,
	                                            Shader.TileMode edgeTreatment) {
		return new RenderEffect();
	}

	public static RenderEffect createOffsetEffect(float offsetX, float offsetY) {
		return new RenderEffect();
	}

	public static RenderEffect createOffsetEffect(float offsetX, float offsetY, RenderEffect input) {
		return new RenderEffect();
	}

	public static RenderEffect createBitmapEffect(Bitmap bitmap) {
		return new RenderEffect();
	}

	public static RenderEffect createBitmapEffect(Bitmap bitmap, Rect src, Rect dst) {
		return new RenderEffect();
	}

	public static RenderEffect createColorFilterEffect(ColorFilter colorFilter) {
		return new RenderEffect();
	}

	public static RenderEffect createColorFilterEffect(ColorFilter colorFilter, RenderEffect renderEffect) {
		return new RenderEffect();
	}

	public static RenderEffect createBlendModeEffect(RenderEffect dst, RenderEffect src, BlendMode blendMode) {
		return new RenderEffect();
	}

	public static RenderEffect createChainEffect(RenderEffect outer, RenderEffect inner) {
		return new RenderEffect();
	}

	public static RenderEffect createShaderEffect(Shader shader) {
		return new RenderEffect();
	}

	public static RenderEffect createRuntimeShaderEffect(RuntimeShader shader, String uniformShaderName) {
		return new RenderEffect();
	}
}
