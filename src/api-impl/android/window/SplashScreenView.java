package android.window;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

/**
 * ATL never puts a system splash screen up, so no instance of this is ever
 * created. It exists because androidx.core.splashscreen does an instanceof
 * against it on API 31+, and an unresolvable class there aborts the process.
 */
public final class SplashScreenView extends FrameLayout {

	public SplashScreenView(Context context) {
		super(context);
	}

	public View getIconView() {
		return null;
	}

	public View getBrandingView() {
		return null;
	}

	public void remove() {}
}
