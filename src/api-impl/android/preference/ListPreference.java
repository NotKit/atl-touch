package android.preference;

import android.content.Context;
import android.util.AttributeSet;

public class ListPreference extends DialogPreference {
	public ListPreference(Context context) {
		super(context);
	}

	public ListPreference(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public ListPreference(Context context, AttributeSet attrs, int defStyleAttr) {
		super(context, attrs, defStyleAttr);
	}

	public String getValue() { return null; }

	public CharSequence[] getEntryValues() {
		CharSequence[] array = new CharSequence[0];
		return array;
	}

	public void setSummary(CharSequence sequence) {}
}
