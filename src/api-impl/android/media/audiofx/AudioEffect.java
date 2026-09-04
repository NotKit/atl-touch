package android.media.audiofx;

/**
 * There is no effect engine behind ALSA here, so the effect list is always empty
 * and no effect can be created or enabled. The type UUIDs are the real AOSP ones
 * so that callers matching on them compare against something meaningful.
 */
public class AudioEffect {
	public static final int SUCCESS = 0;
	public static final int ERROR = -1;
	public static final int ERROR_INVALID_OPERATION = -3;

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

	/* nothing here is a real effect, so there is no descriptor to describe it */
	public Descriptor getDescriptor() { return null; }

	public boolean getEnabled() { return false; }

	public int setEnabled(boolean enable) { return ERROR_INVALID_OPERATION; }

	public static final java.util.UUID EFFECT_TYPE_AEC =
	    java.util.UUID.fromString("7b491460-8d4d-11e0-bd61-0002a5d5c51b");

	public static final java.util.UUID EFFECT_TYPE_NS =
	    java.util.UUID.fromString("58b4b260-8e06-11e0-aa8e-0002a5d5c51b");

	/** Nothing can hold a descriptor: no effect is ever created. */
	public Descriptor getDescriptor() throws IllegalStateException {
		throw new IllegalStateException("no audio effect engine");
	}

	public void release() { }
}
