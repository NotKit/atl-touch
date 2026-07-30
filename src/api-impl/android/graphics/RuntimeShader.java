package android.graphics;

/**
 * API 33 AGSL shader. Skia here is not driven through a runtime-effect pipeline,
 * so the program is kept but never compiled: apps reach this only above their
 * SDK check and draw nothing with it.
 */
public class RuntimeShader extends Shader {

	private final String shader;

	public RuntimeShader(String shader) {
		this.shader = shader;
	}

	public String getShader() {
		return shader;
	}

	public void setFloatUniform(String uniformName, float value) {}

	public void setFloatUniform(String uniformName, float value1, float value2) {}

	public void setFloatUniform(String uniformName, float value1, float value2, float value3) {}

	public void setFloatUniform(String uniformName, float value1, float value2, float value3, float value4) {}

	public void setFloatUniform(String uniformName, float[] values) {}

	public void setIntUniform(String uniformName, int value) {}

	public void setIntUniform(String uniformName, int value1, int value2) {}

	public void setIntUniform(String uniformName, int value1, int value2, int value3) {}

	public void setIntUniform(String uniformName, int value1, int value2, int value3, int value4) {}

	public void setIntUniform(String uniformName, int[] values) {}

	public void setColorUniform(String uniformName, int color) {}

	public void setColorUniform(String uniformName, Color color) {}

	public void setColorUniform(String uniformName, long color) {}

	public void setInputShader(String shaderName, Shader shader) {}

	public void setInputBuffer(String shaderName, BitmapShader shader) {}
}
