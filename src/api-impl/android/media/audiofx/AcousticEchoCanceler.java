package android.media.audiofx;

public class AcousticEchoCanceler extends AudioEffect {

	/* no effect backend, so create() would hand back null anyway */
	public static boolean isAvailable() { return false; }

	public static android.media.audiofx.AcousticEchoCanceler create(int a0) { return null; }
}
