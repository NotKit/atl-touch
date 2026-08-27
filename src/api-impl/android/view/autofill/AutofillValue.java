package android.view.autofill;

public class AutofillValue {

	public boolean isText() { return false; }

	public boolean isToggle() { return false; }

	public boolean isList() { return false; }

	public boolean isDate() { return false; }

	public java.lang.CharSequence getTextValue() { return null; }

	public boolean getToggleValue() { return false; }

	public int getListValue() { return 0; }

	public long getDateValue() { return 0; }

	public static android.view.autofill.AutofillValue forText(java.lang.CharSequence a0) { return null; }

	public static android.view.autofill.AutofillValue forToggle(boolean a0) { return null; }

	public static android.view.autofill.AutofillValue forList(int a0) { return null; }

	public static android.view.autofill.AutofillValue forDate(long a0) { return null; }
}
