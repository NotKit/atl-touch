package android.graphics;

public class Shader {

	public enum TileMode {
		CLAMP,
		MIRROR,
		REPEAT
	}

	/* native ATLShader* (0 for shader types not yet backed natively) */
	public long native_instance = 0;

	protected void init(long ni) {
		native_instance = ni;
	}

	public void setLocalMatrix(Matrix matrix) {
		if (native_instance != 0)
			native_setLocalMatrix(native_instance, matrix != null ? matrix.native_instance : 0);
	}

	protected void copyLocalMatrix(Shader dest) {}

	private static native void native_setLocalMatrix(long shader, long matrix);
}
