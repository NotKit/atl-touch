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

	/** every window view, bottom-most first: the active root's main view, then
	 *  the panels above it. AOSP has one ViewRootImpl per window and enumerates
	 *  those; here dialogs and popups are panels of the single native window, so
	 *  they take the place of AOSP's extra roots. */
	static ArrayList<View> getWindowViews() {
		ArrayList<View> views = new ArrayList<>();
		View main = activeViewRoot != null ? activeViewRoot.getView() : null;
		if (main != null)
			views.add(main);
		views.addAll(instance.mViews);
		return views;
	}

	/** a name that is unique per window and stable while it is attached, which is
	 *  all {@link #getRootView} needs to map one back. Not AOSP's format (title +
	 *  ViewRootImpl identity), because a panel here has no ViewRootImpl of its own. */
	private static String windowName(View view) {
		return view.getClass().getName() + "@" + Integer.toHexString(System.identityHashCode(view));
	}

	/** AOSP internal, reached by reflection: apps that want to draw or inspect
	 *  every window of the process (Telegram blurs them all behind a menu) list
	 *  the names here and ask {@link #getRootView} for each one. */
	public String[] getViewRootNames() {
		ArrayList<View> views = getWindowViews();
		String[] names = new String[views.size()];
		for (int i = 0; i < views.size(); i++)
			names[i] = windowName(views.get(i));
		return names;
	}

	public View getRootView(String name) {
		for (View view : getWindowViews()) {
			if (windowName(view).equals(name))
				return view;
		}
		return null;
	}
}
