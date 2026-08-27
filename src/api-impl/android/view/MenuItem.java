package android.view;

import android.content.Intent;
import android.graphics.drawable.Drawable;

public interface MenuItem {

	public interface OnMenuItemClickListener {
		public boolean onMenuItemClick(MenuItem item);
	}

	public interface OnActionExpandListener {
		public boolean onMenuItemActionExpand(MenuItem item);
		public boolean onMenuItemActionCollapse(MenuItem item);
	}

	public MenuItem setIcon(int iconRes);

	public MenuItem setVisible(boolean visible);

	public MenuItem setChecked(boolean checked);

	public MenuItem setEnabled(boolean enabled);

	public MenuItem setCheckable(boolean checkable);

	public boolean isCheckable();

	public MenuItem setTitleCondensed(CharSequence titleCondensed);

	public MenuItem setTitle(CharSequence title);

	public MenuItem setActionView(View actionView);

	public void setShowAsAction(int action);

	public int getItemId();

	public int getGroupId();

	public MenuItem setOnMenuItemClickListener(OnMenuItemClickListener listener);

	public MenuItem setTitle(int resId);

	public boolean isVisible();

	public Drawable getIcon();

	public SubMenu getSubMenu();

	public MenuItem setActionView(int resId);

	public View getActionView();

	public boolean hasSubMenu();

	public MenuItem setOnActionExpandListener(OnActionExpandListener listener);

	public MenuItem setIcon(Drawable icon);

	public boolean isChecked();

	public MenuItem setShowAsActionFlags(int action);

	public MenuItem setAlphabeticShortcut(char alphaChar);

	public MenuItem setShortcut(char numeric, char alpha);

	public int getOrder();

	public boolean isEnabled();

	public CharSequence getTitleCondensed();

	public CharSequence getTitle();

	public MenuItem setNumericShortcut(char numericChar);

	public boolean expandActionView();

	public boolean collapseActionView();

	public boolean isActionViewExpanded();

	public MenuItem setIntent(Intent intent);

	public default android.content.Intent getIntent() { return null; }

	/**
	 * AOSP keeps the tint on the item and applies it when the icon is read;
	 * without state an interface can only forward to the icon itself. Fenix's
	 * showToolbarWithIconButton calls this from onCreateMenu, and the missing
	 * method aborted the whole toolbar menu.
	 */
	public default MenuItem setIconTintList(android.content.res.ColorStateList tint) {
		Drawable icon = getIcon();
		if (icon != null)
			icon.setTintList(tint);
		return this;
	}

	public default android.content.res.ColorStateList getIconTintList() { return null; }

	public default MenuItem setIconTintMode(android.graphics.PorterDuff.Mode mode) { return this; }

	public default android.graphics.PorterDuff.Mode getIconTintMode() { return null; }

	/**
	 * API 26's accessibility/shortcut tail. Fenix's showToolbarWithIconButton
	 * builds a title-less icon item and calls setContentDescription between
	 * setIconTintList and setShowAsAction, so the missing method left the item
	 * with neither its "show as action" flag nor its click listener.
	 * Defaults, not state: appcompat's MenuItemImpl overrides all of them.
	 */
	public default MenuItem setContentDescription(CharSequence contentDescription) { return this; }

	public default CharSequence getContentDescription() { return null; }

	public default MenuItem setTooltipText(CharSequence tooltipText) { return this; }

	public default CharSequence getTooltipText() { return null; }

	public default MenuItem setAlphabeticShortcut(char alphaChar, int alphaModifiers) {
		return setAlphabeticShortcut(alphaChar);
	}

	public default char getAlphabeticShortcut() { return 0; }

	public default int getAlphabeticModifiers() { return KeyEvent.META_CTRL_ON; }

	public default MenuItem setNumericShortcut(char numericChar, int numericModifiers) {
		return setNumericShortcut(numericChar);
	}

	public default char getNumericShortcut() { return 0; }

	public default int getNumericModifiers() { return KeyEvent.META_CTRL_ON; }

	public default MenuItem setShortcut(char numericChar, char alphaChar, int numericModifiers, int alphaModifiers) {
		return setShortcut(numericChar, alphaChar);
	}

	public default ContextMenu.ContextMenuInfo getMenuInfo() { return null; }
}
