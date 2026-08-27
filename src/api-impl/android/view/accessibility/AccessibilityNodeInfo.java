package android.view.accessibility;

import java.lang.CharSequence;

public class AccessibilityNodeInfo {
	public static final class AccessibilityAction {
		public static final AccessibilityAction ACTION_FOCUS = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_CLEAR_FOCUS = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SELECT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_CLEAR_SELECTION = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_CLICK = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_LONG_CLICK = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_ACCESSIBILITY_FOCUS = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_CLEAR_ACCESSIBILITY_FOCUS = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_NEXT_AT_MOVEMENT_GRANULARITY = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_NEXT_HTML_ELEMENT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PREVIOUS_HTML_ELEMENT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_FORWARD = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_BACKWARD = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_COPY = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PASTE = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_CUT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SET_SELECTION = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_EXPAND = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_COLLAPSE = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_DISMISS = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SET_TEXT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_CONTEXT_CLICK = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SET_PROGRESS = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SHOW_ON_SCREEN = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_TO_POSITION = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_UP = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_LEFT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_RIGHT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_DOWN = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PAGE_UP = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PAGE_DOWN = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PAGE_LEFT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PAGE_RIGHT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_MOVE_WINDOW = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SHOW_TOOLTIP = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_HIDE_TOOLTIP = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PRESS_AND_HOLD = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_IME_ENTER = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_DRAG_START = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_DRAG_DROP = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_DRAG_CANCEL = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SHOW_TEXT_SUGGESTIONS = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_IN_DIRECTION = new AccessibilityAction(0, null);

		private final int id;
		private final CharSequence label;

		public AccessibilityAction(int actionId, CharSequence label) {
			this.id = actionId;
			this.label = label;
		}

		public int getId() { return id; }

		/** androidx's ViewCompat.addAccessibilityAction() de-duplicates the
		 *  actions already on a view by label, so this has to be the label
		 *  the action was built with. */
		public CharSequence getLabel() { return label; }
	}

	public static class RangeInfo {
		public static final int RANGE_TYPE_INT = 0;
		public static final int RANGE_TYPE_FLOAT = 1;
		public static final int RANGE_TYPE_PERCENT = 2;
	
	public static android.view.accessibility.AccessibilityNodeInfo.RangeInfo obtain(int a0, float a1, float a2, float a3) { return null; }
}

	public static AccessibilityNodeInfo obtain(android.view.View a0, int a1) { return null; }

	public static AccessibilityNodeInfo obtain() { return null; }

	public void setClassName(CharSequence className) {}

	public void addAction(int action) {}

	public void setCheckable(boolean checkable) {}

	public void setChecked(boolean checked) {}

	public void setClickable(boolean clickable) {}

	public void setContentDescription(CharSequence description) {}

	public void setEditable(boolean editable) {}

	public void setEnabled(boolean enabled) {}

	public void setFocusable(boolean focusable) {}

	public void setFocused(boolean focused) {}

	public void setScrollable(boolean scrollable) {}

	public void setVisibleToUser(boolean visible) {}

	public void setTextSelection(int start, int end) {}

	public void setRangeInfo(RangeInfo info) {}

	public android.os.Bundle getExtras() { return null; }

	public boolean removeAction(android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction a0) { return false; }

	public static class CollectionInfo { 
	public static android.view.accessibility.AccessibilityNodeInfo.CollectionInfo obtain(int a0, int a1, boolean a2, int a3) { return null; }
}

	public static class CollectionItemInfo { 
	public static android.view.accessibility.AccessibilityNodeInfo.CollectionItemInfo obtain(int a0, int a1, int a2, int a3, boolean a4) { return null; }
}

	public static final int ACTION_ACCESSIBILITY_FOCUS = 64;

	public static final int ACTION_CLEAR_ACCESSIBILITY_FOCUS = 128;

	public static final int ACTION_CLICK = 16;

	public static final int ACTION_COLLAPSE = 524288;

	public static final int ACTION_COPY = 16384;

	public static final int ACTION_CUT = 65536;

	public static final int ACTION_EXPAND = 262144;

	public static final int ACTION_LONG_CLICK = 32;

	public static final int ACTION_NEXT_AT_MOVEMENT_GRANULARITY = 256;

	public static final int ACTION_NEXT_HTML_ELEMENT = 1024;

	public static final int ACTION_PASTE = 32768;

	public static final int ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY = 512;

	public static final int ACTION_PREVIOUS_HTML_ELEMENT = 2048;

	public static final int ACTION_SCROLL_BACKWARD = 8192;

	public static final int ACTION_SCROLL_FORWARD = 4096;

	public static final int ACTION_SELECT = 4;

	public static final int ACTION_SET_SELECTION = 131072;

	public static final int ACTION_SET_TEXT = 2097152;

	public static final int FOCUS_ACCESSIBILITY = 2;

	public static final int FOCUS_INPUT = 1;

	public static final int MOVEMENT_GRANULARITY_CHARACTER = 1;

	public static final int MOVEMENT_GRANULARITY_LINE = 4;

	public static final int MOVEMENT_GRANULARITY_PARAGRAPH = 8;

	public static final int MOVEMENT_GRANULARITY_WORD = 2;

	public static final java.lang.String ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN = "ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN";

	public static final java.lang.String ACTION_ARGUMENT_HTML_ELEMENT_STRING = "ACTION_ARGUMENT_HTML_ELEMENT_STRING";

	public static final java.lang.String ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT = "ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT";

	public static final java.lang.String ACTION_ARGUMENT_SELECTION_END_INT = "ACTION_ARGUMENT_SELECTION_END_INT";

	public static final java.lang.String ACTION_ARGUMENT_SELECTION_START_INT = "ACTION_ARGUMENT_SELECTION_START_INT";

	public static final java.lang.String ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE = "ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE";

	public void addChild(android.view.View a0, int a1) { }

	public void getBoundsInParent(android.graphics.Rect a0) { }

	public void setAccessibilityFocused(boolean a0) { }

	public void setBoundsInParent(android.graphics.Rect a0) { }

	public void setBoundsInScreen(android.graphics.Rect a0) { }

	public void setContentInvalid(boolean a0) { }

	public void setContextClickable(boolean a0) { }

	public void setHintText(java.lang.CharSequence a0) { }

	public void setInputType(int a0) { }

	public void setLongClickable(boolean a0) { }

	public void setMultiLine(boolean a0) { }

	public void setPackageName(java.lang.CharSequence a0) { }

	public void setParent(android.view.View a0, int a1) { }

	public void setPassword(boolean a0) { }

	public void setSelected(boolean a0) { }

	public void setText(java.lang.CharSequence a0) { }

	public void setViewIdResourceName(java.lang.String a0) { }

	public void setCollectionInfo(android.view.accessibility.AccessibilityNodeInfo.CollectionInfo a0) { }

	public void setCollectionItemInfo(android.view.accessibility.AccessibilityNodeInfo.CollectionItemInfo a0) { }

	public void setMovementGranularities(int a0) { }

	public void addAction(android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction a0) { }
}
