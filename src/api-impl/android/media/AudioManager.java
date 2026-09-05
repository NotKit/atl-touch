package android.media;

public class AudioManager {
	public static final String PROPERTY_OUTPUT_FRAMES_PER_BUFFER = "android.media.property.OUTPUT_FRAMES_PER_BUFFER";
	public static final String PROPERTY_OUTPUT_SAMPLE_RATE = "android.media.property.OUTPUT_SAMPLE_RATE";

	public static final int STREAM_MUSIC = 0x3;

	/* getDevices() filters, values match AOSP */
	public static final int GET_DEVICES_INPUTS = 0x1;
	public static final int GET_DEVICES_OUTPUTS = 0x2;
	public static final int GET_DEVICES_ALL = GET_DEVICES_INPUTS | GET_DEVICES_OUTPUTS;

	private native void nativeSetStreamVolume(int volume);

	public boolean isBluetoothA2dpOn() {
		return false;
	}

	public String getProperty(String name) {
		switch (name) {
			case PROPERTY_OUTPUT_FRAMES_PER_BUFFER:
				return "256"; // FIXME arbitrary
			case PROPERTY_OUTPUT_SAMPLE_RATE:
				return "44100"; // FIXME arbitrary
			default:
				System.out.println("AudioManager.getProperty: >" + name + "< not handled");
				return "";
		}
	}

	public interface OnAudioFocusChangeListener {
	}

	public int getRingerMode() {
		return 0;
	}

	public int getStreamVolume(int streamType) {
		return 0; // arbitrary, shouldn't matter too much?
	}

	public int getStreamMaxVolume(int streamType) {
		return 100;
	}

	public int requestAudioFocus(OnAudioFocusChangeListener listener, int streamType, int durationHint) {
		return /*AUDIOFOCUS_REQUEST_GRANTED*/ 1;
	}

	public int abandonAudioFocus(OnAudioFocusChangeListener listener) {
		return /*AUDIOFOCUS_REQUEST_GRANTED*/ 1;
	}

	public int requestAudioFocus(AudioFocusRequest request) {
		return /*AUDIOFOCUS_REQUEST_GRANTED*/ 1;
	}

	public int abandonAudioFocusRequest(AudioFocusRequest request) {
		return /*AUDIOFOCUS_REQUEST_GRANTED*/ 1;
	}

	public boolean isWiredHeadsetOn() {
		return false;
	}

	public void setStreamVolume(int streamType, int index, int flags) {
		nativeSetStreamVolume(index);
	}

	/**
	 * Ignored: the argument is a direction, not a level, and there is no system
	 * volume UI to raise for FLAG_SHOW_UI either. Apps call this to nudge the
	 * volume they are about to play at.
	 */
	public void adjustStreamVolume(int streamType, int direction, int flags) {}

	public boolean isStreamMute(int streamType) {
		return false;
	}

	public boolean isMusicActive() {
		return false;
	}

	public void setSpeakerphoneOn(boolean on) {}

	public boolean isSpeakerphoneOn() {
		return false;
	}

	public void setBluetoothScoOn(boolean on) {}

	public boolean isBluetoothScoOn() {
		return false;
	}

	/**
	 * There is no SCO route here — capture goes straight to ALSA — so an app that
	 * offers "record over Bluetooth" is told the route does not exist rather than
	 * being left to start a link that never carries audio.
	 */
	public boolean isBluetoothScoAvailableOffCall() {
		return false;
	}

	public void startBluetoothSco() {}

	public void stopBluetoothSco() {}

	public void setMode(int mode) {}

	public int getMode() {
		return /*MODE_NORMAL*/ 0;
	}

	public boolean isMicrophoneMute() {
		return false;
	}

	public void setMicrophoneMute(boolean on) {
		System.out.println("AudioManager.setMicrophoneMute(" + on + ")");
	}
	public void unloadSoundEffects() {}

	/* no device hotplug notifications here, so a callback is only ever remembered */
	public void registerAudioDeviceCallback(AudioDeviceCallback callback, android.os.Handler handler) {}

	public void unregisterAudioDeviceCallback(AudioDeviceCallback callback) {}

	public AudioDeviceInfo[] getDevices(int flags) {
		return new AudioDeviceInfo[0];
	}

	public int generateAudioSessionId() {
		return 0;
	}

	public boolean isVolumeFixed() { return false; }

	public java.util.List getActiveRecordingConfigurations() { return null; }

	public static final int AUDIO_SESSION_ID_GENERATE = 0;

	public static final int MODE_IN_COMMUNICATION = 3;

	public static final int MODE_RINGTONE = 1;

	public static final int STREAM_ALARM = 4;

	public static final int STREAM_NOTIFICATION = 5;

	public static final int STREAM_RING = 2;

	public static final int STREAM_SYSTEM = 1;

	public static final int STREAM_VOICE_CALL = 0;

	public void dispatchMediaKeyEvent(android.view.KeyEvent a0) { }

	/**
	 * There is no media button router here, so nothing ever dispatches a
	 * media key to the receiver -- registering one only has to not throw.
	 */
	public void registerMediaButtonEventReceiver(android.content.ComponentName eventReceiver) {}

	public void unregisterMediaButtonEventReceiver(android.content.ComponentName eventReceiver) {}

	public static final int MODE_IN_CALL = 2;

	public static final int MODE_NORMAL = 0;
}
