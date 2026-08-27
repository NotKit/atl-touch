package android.view.accessibility;

import java.util.ArrayList;
import java.util.List;

/**
 * Something an accessibility service would want to hear about. We have no such
 * service, so events are built and dropped; apps and androidx still construct
 * them and read them back.
 */
public class AccessibilityEvent extends AccessibilityRecord implements android.os.Parcelable {
	/* event types, values match AOSP */
	public static final int TYPE_VIEW_CLICKED = 0x00000001;
	public static final int TYPE_VIEW_LONG_CLICKED = 0x00000002;
	public static final int TYPE_VIEW_SELECTED = 0x00000004;
	public static final int TYPE_VIEW_FOCUSED = 0x00000008;
	public static final int TYPE_VIEW_TEXT_CHANGED = 0x00000010;
	public static final int TYPE_WINDOW_STATE_CHANGED = 0x00000020;
	public static final int TYPE_NOTIFICATION_STATE_CHANGED = 0x00000040;
	public static final int TYPE_VIEW_HOVER_ENTER = 0x00000080;
	public static final int TYPE_VIEW_HOVER_EXIT = 0x00000100;
	public static final int TYPE_TOUCH_EXPLORATION_GESTURE_START = 0x00000200;
	public static final int TYPE_TOUCH_EXPLORATION_GESTURE_END = 0x00000400;
	public static final int TYPE_WINDOW_CONTENT_CHANGED = 0x00000800;
	public static final int TYPE_VIEW_SCROLLED = 0x00001000;
	public static final int TYPE_VIEW_TEXT_SELECTION_CHANGED = 0x00002000;
	public static final int TYPE_ANNOUNCEMENT = 0x00004000;
	public static final int TYPE_VIEW_ACCESSIBILITY_FOCUSED = 0x00008000;
	public static final int TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED = 0x00010000;
	public static final int TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY = 0x00020000;
	public static final int TYPE_GESTURE_DETECTION_START = 0x00040000;
	public static final int TYPE_GESTURE_DETECTION_END = 0x00080000;
	public static final int TYPE_TOUCH_INTERACTION_START = 0x00100000;
	public static final int TYPE_TOUCH_INTERACTION_END = 0x00200000;
	public static final int TYPE_WINDOWS_CHANGED = 0x00400000;
	public static final int TYPE_VIEW_CONTEXT_CLICKED = 0x00800000;
	public static final int TYPE_ASSIST_READING_CONTEXT = 0x01000000;
	public static final int TYPES_ALL_MASK = 0xFFFFFFFF;

	/* change types for TYPE_WINDOW_CONTENT_CHANGED */
	public static final int CONTENT_CHANGE_TYPE_UNDEFINED = 0x0000;
	public static final int CONTENT_CHANGE_TYPE_SUBTREE = 0x0001;
	public static final int CONTENT_CHANGE_TYPE_TEXT = 0x0002;
	public static final int CONTENT_CHANGE_TYPE_CONTENT_DESCRIPTION = 0x0004;
	public static final int CONTENT_CHANGE_TYPE_PANE_TITLE = 0x0008;
	public static final int CONTENT_CHANGE_TYPE_PANE_APPEARED = 0x0010;
	public static final int CONTENT_CHANGE_TYPE_PANE_DISAPPEARED = 0x0020;
	public static final int CONTENT_CHANGE_TYPE_STATE_DESCRIPTION = 0x0040;

	public static final int INVALID_POSITION = -1;

	/* constants the generated stub carried that AOSP also defines */
	public static final int CONTENT_CHANGE_TYPE_CHECKED = 8192;
	public static final int CONTENT_CHANGE_TYPE_CONTENT_INVALID = 1024;
	public static final int CONTENT_CHANGE_TYPE_DRAG_CANCELLED = 512;
	public static final int CONTENT_CHANGE_TYPE_DRAG_DROPPED = 256;
	public static final int CONTENT_CHANGE_TYPE_DRAG_STARTED = 128;
	public static final int CONTENT_CHANGE_TYPE_ENABLED = 4096;
	public static final int CONTENT_CHANGE_TYPE_ERROR = 2048;
	public static final int CONTENT_CHANGE_TYPE_EXPANDED = 16384;
	public static final int CONTENT_CHANGE_TYPE_SORT_DIRECTION = 65536;
	public static final int CONTENT_CHANGE_TYPE_SUPPLEMENTAL_DESCRIPTION = 32768;
	public static final int MAX_TEXT_LENGTH = 500;
	public static final int SPEECH_STATE_LISTENING_END = 8;
	public static final int SPEECH_STATE_LISTENING_START = 4;
	public static final int SPEECH_STATE_SPEAKING_END = 2;
	public static final int SPEECH_STATE_SPEAKING_START = 1;
	public static final int TEXT_CHANGE_TYPE_COMMITTED_BY_IME = 2;
	public static final int TEXT_CHANGE_TYPE_CONVERSION_SUGGESTION_SELECTED_BY_IME = 4;
	public static final int TEXT_CHANGE_TYPE_IN_COMPOSITION = 1;
	public static final int TEXT_CHANGE_TYPE_UNDEFINED = 0;
	public static final int TYPE_SPEECH_STATE_CHANGE = 33554432;
	public static final int TYPE_VIEW_TARGETED_BY_SCROLL = 67108864;
	public static final int WINDOWS_CHANGE_ACCESSIBILITY_FOCUSED = 128;
	public static final int WINDOWS_CHANGE_ACTIVE = 32;
	public static final int WINDOWS_CHANGE_ADDED = 1;
	public static final int WINDOWS_CHANGE_BOUNDS = 8;
	public static final int WINDOWS_CHANGE_CHILDREN = 512;
	public static final int WINDOWS_CHANGE_FOCUSED = 64;
	public static final int WINDOWS_CHANGE_LAYER = 16;
	public static final int WINDOWS_CHANGE_PARENT = 256;
	public static final int WINDOWS_CHANGE_PIP = 1024;
	public static final int WINDOWS_CHANGE_REMOVED = 2;
	public static final int WINDOWS_CHANGE_TITLE = 4;

	private final List<AccessibilityRecord> records = new ArrayList<>();
	private CharSequence packageName;
	private int eventType;
	private long eventTime;
	private int action;
	private int contentChangeTypes;
	private int movementGranularity;
	private boolean accessibilityDataSensitive;

	public AccessibilityEvent() {
	}

	public AccessibilityEvent(int eventType) {
		this.eventType = eventType;
	}

	public static AccessibilityEvent obtain() {
		return new AccessibilityEvent();
	}

	public static AccessibilityEvent obtain(int eventType) {
		return new AccessibilityEvent(eventType);
	}

	public static AccessibilityEvent obtain(AccessibilityEvent event) {
		AccessibilityEvent copy = new AccessibilityEvent(event.eventType);
		copy.packageName = event.packageName;
		copy.eventTime = event.eventTime;
		copy.action = event.action;
		copy.contentChangeTypes = event.contentChangeTypes;
		copy.movementGranularity = event.movementGranularity;
		copy.accessibilityDataSensitive = event.accessibilityDataSensitive;
		copy.records.addAll(event.records);
		copy.setClassName(event.getClassName());
		copy.setContentDescription(event.getContentDescription());
		copy.getText().addAll(event.getText());
		return copy;
	}

	@Override
	public void recycle() {}

	public int getEventType() {
		return eventType;
	}

	public void setEventType(int eventType) {
		this.eventType = eventType;
	}

	public long getEventTime() {
		return eventTime;
	}

	public void setEventTime(long eventTime) {
		this.eventTime = eventTime;
	}

	public CharSequence getPackageName() {
		return packageName;
	}

	public void setPackageName(CharSequence packageName) {
		this.packageName = packageName;
	}

	public int getAction() {
		return action;
	}

	public void setAction(int action) {
		this.action = action;
	}

	public int getContentChangeTypes() {
		return contentChangeTypes;
	}

	public void setContentChangeTypes(int contentChangeTypes) {
		this.contentChangeTypes = contentChangeTypes;
	}

	public int getMovementGranularity() {
		return movementGranularity;
	}

	public void setMovementGranularity(int movementGranularity) {
		this.movementGranularity = movementGranularity;
	}

	public boolean isAccessibilityDataSensitive() {
		return accessibilityDataSensitive;
	}

	public void setAccessibilityDataSensitive(boolean accessibilityDataSensitive) {
		this.accessibilityDataSensitive = accessibilityDataSensitive;
	}

	public void appendRecord(AccessibilityRecord record) {
		records.add(record);
	}

	public AccessibilityRecord getRecord(int index) {
		return records.get(index);
	}

	public int getRecordCount() {
		return records.size();
	}
}
