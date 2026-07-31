package android.view;

import android.graphics.Rect;
import android.os.Bundle;

/**
 * Assist/autofill view structure. ATL never builds one -- nothing calls
 * View.onProvideStructure() -- so this only needs to exist: a view that
 * declares onProvideStructure(ViewStructure) makes the runtime load the
 * parameter type as soon as anything reflects over its declared methods.
 */
public abstract class ViewStructure {
	public abstract void setId(int id, String packageName, String typeName, String entryName);

	public abstract void setDimens(int left, int top, int scrollX, int scrollY, int width, int height);

	public abstract void setVisibility(int visibility);

	public abstract void setEnabled(boolean state);

	public abstract void setClickable(boolean state);

	public abstract void setLongClickable(boolean state);

	public abstract void setFocusable(boolean state);

	public abstract void setFocused(boolean state);

	public abstract void setSelected(boolean state);

	public abstract void setActivated(boolean state);

	public abstract void setChecked(boolean state);

	public abstract void setContextClickable(boolean state);

	public abstract void setOpaque(boolean opaque);

	public abstract void setClassName(String className);

	public abstract void setContentDescription(CharSequence contentDescription);

	public abstract void setText(CharSequence text);

	public abstract void setText(CharSequence text, int selectionStart, int selectionEnd);

	public abstract void setTextStyle(float size, int fgColor, int bgColor, int style);

	public abstract void setTextLines(int[] charOffsets, int[] baselines);

	public abstract void setHint(CharSequence hint);

	public abstract CharSequence getText();

	public abstract int getTextSelectionStart();

	public abstract int getTextSelectionEnd();

	public abstract CharSequence getHint();

	public abstract Bundle getExtras();

	public abstract boolean hasExtras();

	public abstract void setChildCount(int num);

	public abstract int addChildCount(int num);

	public abstract int getChildCount();

	public abstract ViewStructure newChild(int index);

	public abstract ViewStructure asyncNewChild(int index);

	public abstract void asyncCommit();

	public abstract Rect getTempRect();
}
