package android.app;

import android.content.Context;
import android.content.res.Configuration;
import android.util.Log;

public class UiModeManager {
	private static final String TAG = "UiModeManager";

	public interface ContrastChangeListener {
	
	public default void onContrastChanged(float a0) { }
}

	public int getCurrentModeType() {
		return Context.sys_config.uiMode & Configuration.UI_MODE_TYPE_MASK;
	}

	public int getNightMode() {
		return Context.sys_config.uiMode & Configuration.UI_MODE_NIGHT_MASK;
	}

	/* the host session owns the theme; an app asking for its own night mode is noted and ignored */
	public void setApplicationNightMode(int mode) {
		Log.i(TAG, "setApplicationNightMode(" + mode + "): STUB");
	}

	public void setNightMode(int mode) {
		Log.i(TAG, "setNightMode(" + mode + "): STUB");
	}
	public float getContrast() { return 0.0f; }

	public void removeContrastChangeListener(android.app.UiModeManager.ContrastChangeListener a0) { }

	public void addContrastChangeListener(java.util.concurrent.Executor a0, android.app.UiModeManager.ContrastChangeListener a1) { }
}
