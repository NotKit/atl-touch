package android.preference;

import android.os.Parcelable;
import android.view.AbsSavedState;

public class Preference {
	public interface OnPreferenceChangeListener {
		public boolean onPreferenceChange(Preference preference, Object newValue);
	}
	public interface OnPreferenceClickListener {
		public boolean onPreferenceClick(Preference preference);
	}
	public static class BaseSavedState extends AbsSavedState {
		public BaseSavedState(Parcelable superState) {}
	}
	public void setOnPreferenceChangeListener(OnPreferenceChangeListener onPreferenceChangeListener) {}
	public void setOnPreferenceClickListener(OnPreferenceClickListener onPreferenceClickListener) {}
	private String key;
	public String getKey() { return key; }
	public void setKey(String key) { this.key = key; }
	public void setIcon(android.graphics.drawable.Drawable icon) {}
	public void setIcon(int iconResId) {}
	protected void onBindView(android.view.View view) {}
	public void setEnabled(boolean enabled) {}
	public void setTitle(CharSequence title) {}
	public void setSummary(CharSequence summary) {}
}
