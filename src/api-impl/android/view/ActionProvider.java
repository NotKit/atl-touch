package android.view;

import android.content.Context;

/**
 * Supplies the view and the behaviour of an action bar menu item.
 *
 * Honest stub. ATL has no action bar, so nothing in the framework ever calls
 * into a provider: the action view is never inflated, the sub-menu is never
 * prepared and the default action is never performed. Only the listener
 * plumbing is real, so a provider that overrides item visibility can still
 * tell its host it changed.
 *
 * It exists mainly because AppCompat's MenuItemWrapperICS names this class and
 * the nested VisibilityListener in its own class signature -- an absent type
 * there stops that class loading at all, which is a NoClassDefFoundError in
 * the middle of menu inflation rather than a missing feature.
 */
public abstract class ActionProvider {
	private VisibilityListener visibilityListener;

	public ActionProvider(Context context) {
	}

	public abstract View onCreateActionView();

	public View onCreateActionView(MenuItem forItem) {
		return onCreateActionView();
	}

	public boolean overridesItemVisibility() {
		return false;
	}

	public boolean isVisible() {
		return true;
	}

	public void refreshVisibility() {
		if (visibilityListener != null && overridesItemVisibility())
			visibilityListener.onActionProviderVisibilityChanged(isVisible());
	}

	public boolean onPerformDefaultAction() {
		return false;
	}

	public boolean hasSubMenu() {
		return false;
	}

	public void onPrepareSubMenu(SubMenu subMenu) {
	}

	public void setVisibilityListener(VisibilityListener listener) {
		visibilityListener = listener;
	}

	/** Told when a provider that overrides item visibility changes its mind. */
	public interface VisibilityListener {
		void onActionProviderVisibilityChanged(boolean isVisible);
	}
}
