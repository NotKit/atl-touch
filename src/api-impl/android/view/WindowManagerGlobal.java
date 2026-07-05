package android.view;

import java.util.ArrayList;

public class WindowManagerGlobal {

	private static final WindowManagerGlobal instance = new WindowManagerGlobal();

	/** the ViewRootImpl currently attached to the (shared) native window; sub-window
	 *  content (dialogs, popups, WindowManager.addView) attaches to it as panels */
	private static ViewRootImpl activeViewRoot;

	/** root views of all attached panels. Same name/shape as AOSP: apps (e.g.
	 *  AnkiDroid's WindowManagerSpy) reflect into this field to enumerate windows. */
	private final ArrayList<View> mViews = new ArrayList<>();

	static void onPanelAdded(View view) {
		instance.mViews.add(view);
	}

	static void onPanelRemoved(View view) {
		instance.mViews.remove(view);
	}

	public static WindowManagerGlobal getInstance() {
		return instance;
	}

	public static void setActiveViewRoot(ViewRootImpl root) {
		activeViewRoot = root;
	}

	public static ViewRootImpl getActiveViewRoot() {
		return activeViewRoot;
	}
}
