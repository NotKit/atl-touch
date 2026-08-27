package android.view;

import java.util.ArrayList;
import java.util.List;

/**
 * API 29 way to enumerate the process's window views; before that apps reflected
 * into {@link WindowManagerGlobal}. Same list either way, so an app that draws
 * every window (Telegram blurs them behind a menu) works at any SDK level.
 */
public final class WindowInspector {

	private WindowInspector() {}

	public static List<View> getGlobalWindowViews() {
		return new ArrayList<View>(WindowManagerGlobal.getWindowViews());
	}
}
