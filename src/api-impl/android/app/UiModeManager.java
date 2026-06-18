package android.app;

import android.content.Context;
import android.content.res.Configuration;

public class UiModeManager {

	public interface ContrastChangeListener {
	
	public default void onContrastChanged(float a0) { }
}

	public int getCurrentModeType() {
		return Context.sys_config.uiMode & Configuration.UI_MODE_TYPE_MASK;
	}

	public int getNightMode() {
		return Context.sys_config.uiMode & Configuration.UI_MODE_NIGHT_MASK;
	}

	public float getContrast() { return 0.0f; }

	public void removeContrastChangeListener(android.app.UiModeManager.ContrastChangeListener a0) { }

	public void addContrastChangeListener(java.util.concurrent.Executor a0, android.app.UiModeManager.ContrastChangeListener a1) { }
}
