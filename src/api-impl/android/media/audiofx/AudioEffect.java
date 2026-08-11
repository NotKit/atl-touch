package android.media.audiofx;

public class AudioEffect {
	public static final int SUCCESS = 0;

	public static class Descriptor {
	public java.lang.String connectMode;

	public java.lang.String implementor;

	public java.lang.String name;

	public java.util.UUID type;

	public java.util.UUID uuid;
}

	public static Descriptor[] queryEffects() {
		return new Descriptor[0];
	}

	public boolean getEnabled() { return false; }

	public int setEnabled(boolean enable) { return SUCCESS; }

	public static final java.util.UUID EFFECT_TYPE_AEC = null;

	public static final java.util.UUID EFFECT_TYPE_NS = null;

	public void release() { }
}
