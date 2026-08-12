package android.preference;

import android.view.View;
import android.widget.AdapterView;

public class PreferenceScreen extends Preference implements AdapterView.OnItemClickListener {
	public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
	}

	public int getPreferenceCount() {
		return 0;
	}

	public Preference getPreference(int index) {
		return null;
	}

	public boolean removePreference(Preference preference) {
		return false;
	}
}
