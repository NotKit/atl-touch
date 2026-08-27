package android.app;

import android.os.Bundle;

public class Fragment {

	Activity activity;
	String tag;
	boolean created; // onCreate() has already been dispatched to this fragment

	public final String getTag() {
		return tag;
	}

	public void onCreate(Bundle savedInstanceState) {
	}

	public void onActivityCreated(Bundle savedInstanceState) {
	}

	public void onStart() {
	}

	public void onResume() {
	}

	public void onPause() {
	}

	public void onStop() {
	}

	public void onDestroy() {
	}

	public Activity getActivity() {
		return activity;
	}
}
