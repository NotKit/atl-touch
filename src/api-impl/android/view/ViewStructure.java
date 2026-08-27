package android.view;

public abstract class ViewStructure {
	public static final String EXTRA_VIRTUAL_STRUCTURE_TYPE = "android.view.extra.VIRTUAL_STRUCTURE_TYPE";
	public static final String EXTRA_VIRTUAL_STRUCTURE_VERSION_NUMBER = "android.view.extra.VIRTUAL_STRUCTURE_VERSION_NUMBER";

	public android.view.ViewStructure newChild(int a0) { return null; }

	public android.view.ViewStructure.HtmlInfo.Builder newHtmlInfoBuilder(java.lang.String a0) { return null; }

	public static abstract class HtmlInfo {
		public static abstract class Builder {
			public abstract Builder addAttribute(String name, String value);

			public abstract HtmlInfo build();
		}
	}

	public void setAutofillHints(java.lang.String[] a0) { }

	public void setAutofillId(android.view.autofill.AutofillId a0, int a1) { }

	public void setAutofillOptions(java.lang.CharSequence[] a0) { }

	public void setChildCount(int a0) { }

	public void setClassName(java.lang.String a0) { }

	public void setDimens(int a0, int a1, int a2, int a3, int a4, int a5) { }

	public void setEnabled(boolean a0) { }

	public void setFocusable(boolean a0) { }

	public void setFocused(boolean a0) { }

	public void setId(int a0, java.lang.String a1, java.lang.String a2, java.lang.String a3) { }

	public void setInputType(int a0) { }

	public void setVisibility(int a0) { }

	public void setWebDomain(java.lang.String a0) { }

	public void setAutofillType(int a0) { }

	public void setAutofillValue(android.view.autofill.AutofillValue a0) { }

	public void setHtmlInfo(android.view.ViewStructure.HtmlInfo a0) { }

	/* The rest of AOSP's surface, as no-ops: a view that declares
	 * onProvideStructure(ViewStructure) makes the runtime load this type as soon
	 * as anything reflects over its declared methods, and androidx calls into it. */
	public void setClickable(boolean state) { }

	public void setLongClickable(boolean state) { }

	public void setSelected(boolean state) { }

	public void setActivated(boolean state) { }

	public void setChecked(boolean state) { }

	public void setContextClickable(boolean state) { }

	public void setOpaque(boolean opaque) { }

	public void setContentDescription(java.lang.CharSequence contentDescription) { }

	public void setText(java.lang.CharSequence text) { }

	public void setText(java.lang.CharSequence text, int selectionStart, int selectionEnd) { }

	public void setTextStyle(float size, int fgColor, int bgColor, int style) { }

	public void setTextLines(int[] charOffsets, int[] baselines) { }

	public void setHint(java.lang.CharSequence hint) { }

	public java.lang.CharSequence getText() { return null; }

	public int getTextSelectionStart() { return -1; }

	public int getTextSelectionEnd() { return -1; }

	public java.lang.CharSequence getHint() { return null; }

	public android.os.Bundle getExtras() { return null; }

	public boolean hasExtras() { return false; }

	public int addChildCount(int num) { return 0; }

	public int getChildCount() { return 0; }

	public android.view.ViewStructure asyncNewChild(int index) { return null; }

	public void asyncCommit() { }

	public android.graphics.Rect getTempRect() { return null; }
}
