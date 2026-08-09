package android.graphics;

/**
 * API 31 render effects. ATL records what was asked for but its renderer does
 * not apply any of it, so an effect is inert rather than approximated: a layer
 * that should be blurred draws sharp instead of drawing wrong.
 */
public final class RenderEffect {

	private RenderEffect() {}

	public static RenderEffect createOffsetEffect(float offsetX, float offsetY) {
		return new RenderEffect();
	}

	public static RenderEffect createOffsetEffect(float offsetX, float offsetY, RenderEffect input) {
		return new RenderEffect();
	}

	public static RenderEffect createBlurEffect(float radiusX, float radiusY, Shader.TileMode edgeTreatment) {
		return new RenderEffect();
	}

	public static RenderEffect createBlurEffect(float radiusX, float radiusY, RenderEffect input, Shader.TileMode edgeTreatment) {
		return new RenderEffect();
	}

	public static RenderEffect createColorFilterEffect(ColorFilter colorFilter) {
		return new RenderEffect();
	}

	public static RenderEffect createColorFilterEffect(ColorFilter colorFilter, RenderEffect input) {
		return new RenderEffect();
	}

	public static RenderEffect createBitmapEffect(Bitmap bitmap) {
		return new RenderEffect();
	}

	public static RenderEffect createBitmapEffect(Bitmap bitmap, Rect src, Rect dst) {
		return new RenderEffect();
	}

	public static RenderEffect createShaderEffect(Shader shader) {
		return new RenderEffect();
	}

	public static RenderEffect createChainEffect(RenderEffect outer, RenderEffect inner) {
		return new RenderEffect();
	}
}
