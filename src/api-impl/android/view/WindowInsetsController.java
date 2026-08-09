package android.view;

import android.view.WindowInsets.Type.InsetsType;

public interface WindowInsetsController {

	int APPEARANCE_OPAQUE_STATUS_BARS = 1 << 0;
	int APPEARANCE_OPAQUE_NAVIGATION_BARS = 1 << 1;
	int APPEARANCE_LOW_PROFILE_BARS = 1 << 2;
	int APPEARANCE_LIGHT_STATUS_BARS = 1 << 3;
	int APPEARANCE_LIGHT_NAVIGATION_BARS = 1 << 4;

	int BEHAVIOR_SHOW_BARS_BY_TOUCH = 0;
	int BEHAVIOR_SHOW_BARS_BY_SWIPE = 1;
	int BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE = 2;
	int BEHAVIOR_DEFAULT = BEHAVIOR_SHOW_BARS_BY_SWIPE;

	void setSystemBarsAppearance(int appearance, int mask);
	int getSystemBarsAppearance();
	void setSystemBarsBehavior(int behavior);
	int getSystemBarsBehavior();
	void show(@InsetsType int types);
	void hide(@InsetsType int types);

	void addOnControllableInsetsChangedListener(OnControllableInsetsChangedListener listener);
	void removeOnControllableInsetsChangedListener(OnControllableInsetsChangedListener listener);

	interface OnControllableInsetsChangedListener {
		void onControllableInsetsChanged(WindowInsetsController controller, @InsetsType int typeMask);
	}
}
