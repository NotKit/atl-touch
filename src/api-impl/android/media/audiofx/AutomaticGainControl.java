package android.media.audiofx;

/** No effect backend here, so the AGC is never available and never created. */
public class AutomaticGainControl extends AudioEffect {

	public static boolean isAvailable() { return false; }

	public static android.media.audiofx.AutomaticGainControl create(int a0) { return null; }
}
