package android.view;

import android.view.WindowInsets.Type.InsetsType;

/* We don't draw system bars, so showing and hiding them is a no-op — but the
 * appearance/behaviour flags have to be remembered, because androidx reads them
 * back (WindowInsetsControllerCompat.isAppearanceLightStatusBars() and friends). */
public class InsetsController implements WindowInsetsController {

	private int systemBarsAppearance;
	private int systemBarsBehavior = BEHAVIOR_DEFAULT;

	public void setSystemBarsAppearance(int appearance, int mask) {
		systemBarsAppearance = (systemBarsAppearance & ~mask) | (appearance & mask);
	}

	public int getSystemBarsAppearance() {
		return systemBarsAppearance;
	}

	public void setSystemBarsBehavior(int behavior) {
		systemBarsBehavior = behavior;
	}

	public int getSystemBarsBehavior() {
		return systemBarsBehavior;
	}

	public void show(@InsetsType int types) {}
	public void hide(@InsetsType int types) {}

	/* Nothing here ever becomes controllable, so a registered listener would
	 * never have anything to report. */
	public void addOnControllableInsetsChangedListener(OnControllableInsetsChangedListener listener) {}
	public void removeOnControllableInsetsChangedListener(OnControllableInsetsChangedListener listener) {}
}
