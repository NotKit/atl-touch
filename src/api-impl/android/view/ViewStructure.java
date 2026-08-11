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
}
