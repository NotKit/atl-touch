package android.hardware.display;

import android.os.Handler;
import android.view.Display;

public final class DisplayManager {
	public static interface DisplayListener {}

	public Display getDisplay(int dummy) {
		return new Display();
	}

	public void registerDisplayListener(DisplayListener listener, Handler handler) {
	}

	public void unregisterDisplayListener(DisplayListener listener) {}

	public Display[] getDisplays() {
		return new Display[0];
	}

	public static final int VIRTUAL_DISPLAY_FLAG_PRESENTATION = 2;

	public static final int VIRTUAL_DISPLAY_FLAG_PUBLIC = 1;
}
