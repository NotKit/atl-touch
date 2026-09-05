package android.media.audiofx;

public class NoiseSuppressor extends AudioEffect {

	/* no effect backend, so create() would hand back null anyway */
	public static boolean isAvailable() { return false; }

	public static android.media.audiofx.NoiseSuppressor create(int a0) { return null; }
}
