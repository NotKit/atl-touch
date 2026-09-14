package android.media;

import android.os.Handler;

public interface AudioRouting {

	AudioDeviceInfo getPreferredDevice();

	boolean setPreferredDevice(AudioDeviceInfo deviceInfo);

	AudioDeviceInfo getRoutedDevice();

	void addOnRoutingChangedListener(OnRoutingChangedListener listener, Handler handler);

	void removeOnRoutingChangedListener(OnRoutingChangedListener listener);

	interface OnRoutingChangedListener {
		void onRoutingChanged(AudioRouting router);
	}
}
