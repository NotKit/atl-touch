package android.view.accessibility;

import android.os.Parcelable;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * The state an accessibility event carries. Nothing here observes it — we have no
 * accessibility service — but androidx wraps it, so apps reach it through
 * AccessibilityRecordCompat and it has to hold what it is given.
 */
public class AccessibilityRecord {
	private CharSequence className;
	private CharSequence contentDescription;
	private CharSequence beforeText;
	private Parcelable parcelableData;
	private final List<CharSequence> text = new ArrayList<>();
	private AccessibilityNodeInfo source;
	private int windowId = -1;
	private int addedCount = -1;
	private int removedCount = -1;
	private int currentItemIndex = -1;
	private int itemCount;
	private int fromIndex = -1;
	private int toIndex = -1;
	private int scrollX;
	private int scrollY;
	private int maxScrollX;
	private int maxScrollY;
	private boolean checked;
	private boolean enabled;
	private boolean password;
	private boolean fullScreen;
	private boolean scrollable;

	public static AccessibilityRecord obtain() {
		return new AccessibilityRecord();
	}

	public static AccessibilityRecord obtain(AccessibilityRecord record) {
		AccessibilityRecord copy = new AccessibilityRecord();
		copy.className = record.className;
		copy.contentDescription = record.contentDescription;
		copy.beforeText = record.beforeText;
		copy.parcelableData = record.parcelableData;
		copy.text.addAll(record.text);
		copy.source = record.source;
		copy.windowId = record.windowId;
		copy.addedCount = record.addedCount;
		copy.removedCount = record.removedCount;
		copy.currentItemIndex = record.currentItemIndex;
		copy.itemCount = record.itemCount;
		copy.fromIndex = record.fromIndex;
		copy.toIndex = record.toIndex;
		copy.scrollX = record.scrollX;
		copy.scrollY = record.scrollY;
		copy.maxScrollX = record.maxScrollX;
		copy.maxScrollY = record.maxScrollY;
		copy.checked = record.checked;
		copy.enabled = record.enabled;
		copy.password = record.password;
		copy.fullScreen = record.fullScreen;
		copy.scrollable = record.scrollable;
		return copy;
	}

	public void recycle() {}

	public void setSource(View source) {
		setSource(source, View.NO_ID);
	}

	public void setSource(View root, int virtualDescendantId) {
		this.source = null;
	}

	public AccessibilityNodeInfo getSource() {
		return source;
	}

	public int getWindowId() {
		return windowId;
	}

	public CharSequence getClassName() {
		return className;
	}

	public void setClassName(CharSequence className) {
		this.className = className;
	}

	public CharSequence getContentDescription() {
		return contentDescription;
	}

	public void setContentDescription(CharSequence contentDescription) {
		this.contentDescription = contentDescription;
	}

	public CharSequence getBeforeText() {
		return beforeText;
	}

	public void setBeforeText(CharSequence beforeText) {
		this.beforeText = beforeText;
	}

	public Parcelable getParcelableData() {
		return parcelableData;
	}

	public void setParcelableData(Parcelable parcelableData) {
		this.parcelableData = parcelableData;
	}

	public List<CharSequence> getText() {
		return text;
	}

	public int getAddedCount() {
		return addedCount;
	}

	public void setAddedCount(int addedCount) {
		this.addedCount = addedCount;
	}

	public int getRemovedCount() {
		return removedCount;
	}

	public void setRemovedCount(int removedCount) {
		this.removedCount = removedCount;
	}

	public int getCurrentItemIndex() {
		return currentItemIndex;
	}

	public void setCurrentItemIndex(int currentItemIndex) {
		this.currentItemIndex = currentItemIndex;
	}

	public int getItemCount() {
		return itemCount;
	}

	public void setItemCount(int itemCount) {
		this.itemCount = itemCount;
	}

	public int getFromIndex() {
		return fromIndex;
	}

	public void setFromIndex(int fromIndex) {
		this.fromIndex = fromIndex;
	}

	public int getToIndex() {
		return toIndex;
	}

	public void setToIndex(int toIndex) {
		this.toIndex = toIndex;
	}

	public int getScrollX() {
		return scrollX;
	}

	public void setScrollX(int scrollX) {
		this.scrollX = scrollX;
	}

	public int getScrollY() {
		return scrollY;
	}

	public void setScrollY(int scrollY) {
		this.scrollY = scrollY;
	}

	public int getMaxScrollX() {
		return maxScrollX;
	}

	public void setMaxScrollX(int maxScrollX) {
		this.maxScrollX = maxScrollX;
	}

	public int getMaxScrollY() {
		return maxScrollY;
	}

	public void setMaxScrollY(int maxScrollY) {
		this.maxScrollY = maxScrollY;
	}

	public boolean isChecked() {
		return checked;
	}

	public void setChecked(boolean checked) {
		this.checked = checked;
	}

	public boolean isEnabled() {
		return enabled;
	}

	public void setEnabled(boolean enabled) {
		this.enabled = enabled;
	}

	public boolean isPassword() {
		return password;
	}

	public void setPassword(boolean password) {
		this.password = password;
	}

	public boolean isFullScreen() {
		return fullScreen;
	}

	public void setFullScreen(boolean fullScreen) {
		this.fullScreen = fullScreen;
	}

	public boolean isScrollable() {
		return scrollable;
	}

	public void setScrollable(boolean scrollable) {
		this.scrollable = scrollable;
	}
}
