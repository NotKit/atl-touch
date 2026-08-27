package android.app;

import android.content.Context;
import android.os.Bundle;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

public class FragmentManager {

	private Activity activity;

	private final List<FragmentLifecycleCallbacks> lifecycleCallbacks = new ArrayList<>();

	public FragmentManager(Activity activity) {
		this.activity = activity;
	}

	/* androidx's ReportFragment adds itself by tag and then casts the result of
	 * this back without a null check, so returning null costs every
	 * ProcessLifecycleOwner user below API 29 its whole lifecycle. */
	public Fragment findFragmentByTag(String tag) {
		for (Fragment fragment : activity.fragments) {
			if (tag == null ? fragment.tag == null : tag.equals(fragment.tag))
				return fragment;
		}
		return null;
	}

	public FragmentTransaction beginTransaction() {
		return new FragmentTransaction(activity);
	}

	public boolean executePendingTransactions() {
		return false;
	}

	/**
	 * Nothing here dispatches these: ATL has no android.app fragment lifecycle
	 * to hang them off, so a registered callback is remembered and never called.
	 * The class has to exist because leakcanary's AndroidOFragmentDestroyWatcher
	 * subclasses it and registers one from Activity.getFragmentManager(), which
	 * a debug Fenix build reaches through FenixApplication.setupLeakCanary().
	 */
	public abstract static class FragmentLifecycleCallbacks {
		public void onFragmentPreAttached(FragmentManager fm, Fragment f, Context context) {}
		public void onFragmentAttached(FragmentManager fm, Fragment f, Context context) {}
		public void onFragmentPreCreated(FragmentManager fm, Fragment f, Bundle savedInstanceState) {}
		public void onFragmentCreated(FragmentManager fm, Fragment f, Bundle savedInstanceState) {}
		public void onFragmentActivityCreated(FragmentManager fm, Fragment f, Bundle savedInstanceState) {}
		public void onFragmentViewCreated(FragmentManager fm, Fragment f, View v, Bundle savedInstanceState) {}
		public void onFragmentStarted(FragmentManager fm, Fragment f) {}
		public void onFragmentResumed(FragmentManager fm, Fragment f) {}
		public void onFragmentPaused(FragmentManager fm, Fragment f) {}
		public void onFragmentStopped(FragmentManager fm, Fragment f) {}
		public void onFragmentSaveInstanceState(FragmentManager fm, Fragment f, Bundle outState) {}
		public void onFragmentViewDestroyed(FragmentManager fm, Fragment f) {}
		public void onFragmentDestroyed(FragmentManager fm, Fragment f) {}
		public void onFragmentDetached(FragmentManager fm, Fragment f) {}
	}

	public void registerFragmentLifecycleCallbacks(FragmentLifecycleCallbacks cb, boolean recursive) {
		lifecycleCallbacks.add(cb);
	}

	public void unregisterFragmentLifecycleCallbacks(FragmentLifecycleCallbacks cb) {
		lifecycleCallbacks.remove(cb);
	}
}
