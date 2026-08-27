package android.media;

/**
 * Told about audio endpoints appearing and disappearing. We have no device
 * hotplug notifications, so a registered callback is simply never called.
 */
public abstract class AudioDeviceCallback {
	public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {}

	public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {}
}
