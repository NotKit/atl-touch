package android.view.accessibility;

/* Shape matters: ART rejects any class whose bytecode treats an
 * AccessibilityEvent as an AccessibilityRecord if this supertype is absent. */
public class AccessibilityRecord {

	public int getAddedCount() { return 0; }

	public java.lang.CharSequence getBeforeText() { return null; }

	public java.lang.CharSequence getClassName() { return null; }

	public java.lang.CharSequence getContentDescription() { return null; }

	public int getCurrentItemIndex() { return 0; }

	public int getDisplayId() { return 0; }

	public int getFromIndex() { return 0; }

	public int getItemCount() { return 0; }

	public int getMaxScrollX() { return 0; }

	public int getMaxScrollY() { return 0; }

	public android.os.Parcelable getParcelableData() { return null; }

	public int getRemovedCount() { return 0; }

	public int getScrollDeltaX() { return 0; }

	public int getScrollDeltaY() { return 0; }

	public int getScrollX() { return 0; }

	public int getScrollY() { return 0; }

	public android.view.accessibility.AccessibilityNodeInfo getSource() { return null; }

	public android.view.accessibility.AccessibilityNodeInfo getSource(int a0) { return null; }

	public java.util.List getText() { return null; }

	public int getToIndex() { return 0; }

	public int getWindowId() { return 0; }

	public boolean isChecked() { return false; }

	public boolean isEnabled() { return false; }

	public boolean isFullScreen() { return false; }

	public boolean isPassword() { return false; }

	public boolean isScrollable() { return false; }

	public static android.view.accessibility.AccessibilityRecord obtain() { return null; }

	public static android.view.accessibility.AccessibilityRecord obtain(android.view.accessibility.AccessibilityRecord a0) { return null; }

	public void recycle() { }

	public void setAddedCount(int a0) { }

	public void setBeforeText(java.lang.CharSequence a0) { }

	public void setChecked(boolean a0) { }

	public void setClassName(java.lang.CharSequence a0) { }

	public void setContentDescription(java.lang.CharSequence a0) { }

	public void setCurrentItemIndex(int a0) { }

	public void setEnabled(boolean a0) { }

	public void setFromIndex(int a0) { }

	public void setFullScreen(boolean a0) { }

	public void setItemCount(int a0) { }

	public void setMaxScrollX(int a0) { }

	public void setMaxScrollY(int a0) { }

	public void setParcelableData(android.os.Parcelable a0) { }

	public void setPassword(boolean a0) { }

	public void setRemovedCount(int a0) { }

	public void setScrollDeltaX(int a0) { }

	public void setScrollDeltaY(int a0) { }

	public void setScrollX(int a0) { }

	public void setScrollY(int a0) { }

	public void setScrollable(boolean a0) { }

	public void setSource(android.view.View a0) { }

	public void setSource(android.view.View a0, int a1) { }

	public void setToIndex(int a0) { }

	public java.lang.String toString() { return null; }

}
