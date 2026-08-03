package android.media;

/** Stub: ATL ships no camera sound assets, so play() is silent. */
public class MediaActionSound {
	public static final int SHUTTER_CLICK = 0;
	public static final int FOCUS_COMPLETE = 1;
	public static final int START_VIDEO_RECORDING = 2;
	public static final int STOP_VIDEO_RECORDING = 3;

	public void load(int soundName) {}

	public void play(int soundName) {}

	public void release() {}
}
