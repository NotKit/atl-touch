package android.preference;

import android.app.Fragment;

/**
 * Stub: apps subclass this for their settings screens. Nothing is rendered
 * yet, but the class has to exist so that app classes implementing
 * OnPreferenceStartFragmentCallback (Open Camera's MainActivity does) can link.
 */
public class PreferenceFragment extends Fragment {
	public interface OnPreferenceStartFragmentCallback {
		boolean onPreferenceStartFragment(PreferenceFragment caller, Preference pref);
	}

	public void addPreferencesFromResource(int preferencesResId) {}

	public Preference findPreference(CharSequence key) {
		return null;
	}

	public PreferenceManager getPreferenceManager() {
		return null;
	}

	public PreferenceScreen getPreferenceScreen() {
		return null;
	}
}
