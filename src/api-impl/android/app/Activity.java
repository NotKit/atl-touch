package android.app;

import android.app.ActionBar;
import android.atl.ATLFilePicker;
import android.atl.ATLLoadedApp;
import android.content.BroadcastReceiver;
import android.content.ComponentCallbacks2;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageParser;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.util.Slog;
import android.view.ContextMenu;
import android.view.ContextThemeWrapper;
import android.view.Display;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.WindowManagerImpl;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class Activity extends ContextThemeWrapper implements Window.Callback, LayoutInflater.Factory2,
		ComponentCallbacks2, KeyEvent.Callback, View.OnCreateContextMenuListener {
	private final static String TAG = "Activity";

	public static final int RESULT_CANCELED = 0;
	public static final int RESULT_OK = -1;

	Window window;
	int requested_orientation = -1 /*ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED*/; // dummy
	public Intent intent;
	private Activity resultActivity;
	private int resultRequestCode;
	private boolean paused = false;
	private CharSequence title = null;
	List<Fragment> fragments = new ArrayList<>();
	boolean destroyed = false;
	private boolean finishing = false;

	public static Activity internalCreateActivity(String className, long native_window, Intent intent) throws ReflectiveOperationException {
		Activity activity = ATLLoadedApp.getPrimaryApplication().createActivity(className, intent);
		activity.window.set_native_window(native_window);
		return activity;
	}

	/**
	 * Helper function to be called from native code to construct main activity
	 *
	 * @param className  class name of activity or null
	 * @return  instance of main activity class
	 * @throws Exception
	 */
	private static Activity createMainActivity(String className, long native_window, String uriString) throws ReflectiveOperationException {
		Uri uri = uriString != null ? Uri.parse(uriString) : null;
		if (className == null) {
			for (PackageParser.Activity activity : ATLLoadedApp.getPrimaryApplication().pkg.activities) {
				if (!activity.info.enabled)
					continue;
				boolean done = false;
				for (PackageParser.IntentInfo intent : activity.intents) {
					Slog.i(TAG, intent.toString());
					if ((uri == null && intent.hasCategory("android.intent.category.LAUNCHER") && intent.hasAction("android.intent.action.MAIN")) ||        // NOLINT
					    (uri != null && intent.hasDataScheme(uri.getScheme())                  && intent.hasCategory("android.intent.category.DEFAULT"))) { // NOLINT
						className = activity.info.targetActivity != null ? activity.info.targetActivity : activity.className;
						done = true;
						break;
					}
				}
				if (done)
					break;
			}
		} else {
			className = className.replace('/', '.');
		}
		if (className == null) {
			if (uri != null)
				System.err.println("Failed to find Activity to launch URI: " + uri);
			else
				System.err.println("Failed to find main Activity");
			System.exit(1);
		}
		return internalCreateActivity(className, native_window, uri != null ? new Intent("android.intent.action.VIEW", uri) : new Intent());
	}

	public Activity() {
		super(null);
	}

	public View root_view;

	public final Application getApplication() {
		return (Application)getApplicationContext();
	}

	public WindowManager getWindowManager() {
		return new WindowManagerImpl();
	}

	public String getCallingPackage() {
		return null; // [from api reference] Note: if the calling activity is not expecting a result (that is it did not use the startActivityForResult(Intent, int) form that includes a request code), then the calling package will be null.
	}

	public ComponentName getComponentName() {
		return intent.getComponent();
	}

	public Intent getIntent() {
		return intent;
	}

	public int getRequestedOrientation() {
		return requested_orientation;
	}

	public void setRequestedOrientation(int orientation) {
		requested_orientation = orientation;
	}

	public boolean isFinishing() {
		return finishing;
	}

	public final boolean requestWindowFeature(int featureId) {
		return false; // whatever feature it is, it's probably not supported
	}

	public final void setVolumeControlStream(int streamType) {}

	protected void onCreate(Bundle savedInstanceState) {
		Slog.i(TAG, "- onCreate - yay!");

		for (Fragment fragment : fragments) {
			fragment.created = true;
			fragment.onCreate(savedInstanceState);
		}

		return;
	}

	protected void onPostCreate(Bundle savedInstanceState) {
		Slog.i(TAG, "- onPostCreate - yay!");
		return;
	}

	protected void onStart() {
		Slog.i(TAG, "- onStart - yay!");
		window.attachViewRoot();
		window.setTitle(title);

		for (Fragment fragment : fragments) {
			fragment.onStart();
		}

		return;
	}

	protected void onRestart() {
		Slog.i(TAG, "- onRestart - yay!");

		return;
	}

	protected void onResume() {
		Slog.i(TAG, "- onResume - yay!");

		for (Fragment fragment : fragments) {
			fragment.onResume();
		}

		paused = false;
		return;
	}

	protected void onPostResume() {
		Slog.i(TAG, "- onPostResume - yay!");
		return;
	}

	protected void onPause() {
		Slog.i(TAG, "- onPause - yay!");

		for (Fragment fragment : fragments) {
			fragment.onPause();
		}

		paused = true;
		return;
	}

	protected void onStop() {
		Slog.i(TAG, "- onStop - yay!");

		for (Fragment fragment : fragments) {
			fragment.onStop();
		}

		return;
	}

	protected void onDestroy() {
		Slog.i(TAG, "- onDestroy - yay!");

		for (Fragment fragment : fragments) {
			fragment.onDestroy();
		}

		destroyed = true;
		return;
	}

	public void onWindowFocusChanged(boolean hasFocus) {
		Slog.i(TAG, "- onWindowFocusChanged - yay! (hasFocus: " + hasFocus + ")");

		return;
	}

	protected void onSaveInstanceState(Bundle outState) {
	}

	public void onConfigurationChanged(Configuration newConfig) {
	}

	public void onLowMemory() {
	}

	public void onTrimMemory(int level) {
	}

	/* KeyEvent.Callback: nothing in ATL dispatches key events here yet, but an
	   app subclass that calls super.onKeyDown() must find one. */
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		return false;
	}

	public boolean onKeyUp(int keyCode, KeyEvent event) {
		return false;
	}

	public boolean onKeyLongPress(int keyCode, KeyEvent event) {
		return false;
	}

	public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
		return false;
	}

	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
	}

	/* --- */

	public void setContentView(int layoutResID) throws Exception {
		Slog.i(TAG, "- setContentView - yay!");

		root_view = getLayoutInflater().inflate(layoutResID, null, false);

		window.setContentView(root_view);
		onContentChanged();
	}

	public void setContentView(View view, ViewGroup.LayoutParams layoutParams) {
		setContentView(view);
	}

	public void setContentView(View view) {
		window.setContentView(view);
		onContentChanged();
	}

	public <T extends android.view.View> T findViewById(int id) {
		View view = window.findViewById(id);

		return (T)view;
	}

	public void invalidateOptionsMenu() {
		Slog.i(TAG, "invalidateOptionsMenu() called, should we do something?");
	}

	/* AOSP tears the view hierarchy down after onDestroy, and a SurfaceView's
	 * layer depends on the synchronous surfaceDestroyed that comes with it:
	 * without it an app's render thread keeps presenting into a wl_egl_window
	 * whose EGLDisplay is about to go away. Called from activity_close. */
	void detachWindowViews() {
		if (window != null && window.getViewRootImpl() != null)
			window.getViewRootImpl().setView(null);
	}

	public Window getWindow() {
		return this.window;
	}

	public Display getDisplay() {
		return new Display();
	}

	public final void runOnUiThread(Runnable action) {
		if (Looper.myLooper() == Looper.getMainLooper()) {
			action.run();
		} else {
			new Handler(Looper.getMainLooper()).post(action);
		}
	}

	protected void onActivityResult(int requestCode, int resultCode, Intent data) {}

	// the order must match the ATLFilePicker.ACTION_* values
	private static final List<String> FILE_CHOOSER_ACTIONS = Arrays.asList(
	    "android.intent.action.OPEN_DOCUMENT",     // (0) ATLFilePicker.ACTION_OPEN
	    "android.intent.action.CREATE_DOCUMENT",   // (1) ATLFilePicker.ACTION_SAVE
	    "android.intent.action.OPEN_DOCUMENT_TREE" // (2) ATLFilePicker.ACTION_SELECT_FOLDER
	);

	protected void fileChooserResultCallback(int requestCode, int resultCode, int action, String uri) {
		onActivityResult(requestCode, resultCode, new Intent(FILE_CHOOSER_ACTIONS.get(action), uri != null ? Uri.parse(uri) : null));
	}

	private void showFilePicker(final int action, Intent intent, final int requestCode) {
		new ATLFilePicker(this, action, null, intent.getStringExtra(Intent.EXTRA_TITLE),
		                  new ATLFilePicker.ResultListener() {
			@Override
			public void onResult(java.io.File file) {
				fileChooserResultCallback(requestCode, file != null ? -1 /*RESULT_OK*/ : 0 /*RESULT_CANCELED*/,
				                          action, file != null ? Uri.fromFile(file).toString() : null);
			}
		}).show();
	}

	public void startActivityForResult(Intent intent, int requestCode, Bundle options) {
		Slog.i(TAG, "startActivityForResult(" + intent + ", " + requestCode + "," + options + ") called");
		if (intent.getComponent() != null) {
			try {
				final Activity activity = internalCreateActivity(intent.getComponent().getClassName(), getWindow().native_window, intent);
				activity.resultRequestCode = requestCode;
				activity.resultActivity = this;
				runOnUiThread(new Runnable() {
					@Override
					public void run() {
						nativeStartActivity(activity);
					}
				});
			} catch (ReflectiveOperationException e) {
				onActivityResult(requestCode, 0 /*RESULT_CANCELED*/, new Intent());
			}
		} else if (FILE_CHOOSER_ACTIONS.contains(intent.getAction())) {
			showFilePicker(FILE_CHOOSER_ACTIONS.indexOf(intent.getAction()), intent, requestCode);
		} else if (Intent.ACTION_GET_CONTENT.equals(intent.getAction())) {
			showFilePicker(ATLFilePicker.ACTION_OPEN, intent, requestCode);
		} else if ("android.intent.action.INSTALL_PACKAGE".equals(intent.getAction())) {
			try {
				Process p = new ProcessBuilder("/usr/bin/env", "android-translation-layer", "--install", intent.getData().getPath()).start();
				int exitValue = p.waitFor();
				if (exitValue == 0) {
					onActivityResult(requestCode, -1 /*RESULT_OK*/, new Intent());
				} else {
					onActivityResult(requestCode, 0 /*RESULT_CANCELED*/, new Intent());
				}
			} catch (IOException | InterruptedException e) {
				e.printStackTrace();
				onActivityResult(requestCode, 0 /*RESULT_CANCELED*/, new Intent());
			}
		} else {
			Slog.i(TAG, "startActivityForResult: intent was not handled. Calling onActivityResult(RESULT_CANCELED).");
			onActivityResult(requestCode, 0 /*RESULT_CANCELED*/, new Intent());
		}
	}
	public void startActivityForResult(Intent intent, int requestCode) {
		startActivityForResult(intent, requestCode, null);
	}

	private final android.window.OnBackInvokedDispatcher onBackInvokedDispatcher = new android.window.OnBackInvokedDispatcher() {
		public void registerOnBackInvokedCallback(int priority, android.window.OnBackInvokedCallback callback) {}
		public void unregisterOnBackInvokedCallback(android.window.OnBackInvokedCallback callback) {}
	};

	public android.window.OnBackInvokedDispatcher getOnBackInvokedDispatcher() {
		return onBackInvokedDispatcher;
	}

	public boolean startActivityIfNeeded(Intent intent, int requestCode) {
		return startActivityIfNeeded(intent, requestCode, null);
	}

	public boolean startActivityIfNeeded(Intent intent, int requestCode, Bundle options) {
		startActivityForResult(intent, requestCode, options);
		return true;
	}

	public void setResult(int resultCode, Intent data) {
		if (resultActivity != null) {
			resultActivity.onActivityResult(resultRequestCode, resultCode, data);
		}
	}

	public void setResult(int resultCode) {
		setResult(resultCode, null);
	}

	protected Dialog onCreateDialog(int id) {
		Slog.i(TAG, "Activity.onCreateDialog(" + id + ") called");
		return null;
	}

	protected void onPrepareDialog(int id, Dialog dialog) {
		Slog.i(TAG, "Activity.onPrepareDialog(" + id + ") called");
	}

	private Map<Integer, Dialog> dialogs = new HashMap<Integer, Dialog>();

	public final void showDialog(int id) {
		Slog.i(TAG, "Activity.showDialog(" + id + ") called");
		Dialog dialog = dialogs.get(id);
		if (dialog == null)
			dialogs.put(id, dialog = onCreateDialog(id));
		if (dialog == null) {
			Slog.w(TAG, "Dialog " + id + " was not created");
			return;
		}
		onPrepareDialog(id, dialog);
		dialog.show();
	}

	public boolean showDialog(int id, Bundle args) {
		return false;
	}

	public void removeDialog(int id) {
		Dialog dialog = dialogs.remove(id);
		if (dialog != null)
			dialog.dismiss();
	}

	public void finish() {
		finishing = true;
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				if (window != null && window.native_window != 0) {
					nativeFinish(getWindow().native_window);
					window.native_window = 0;
				}
			}
		});
	}

	public Object getLastNonConfigurationInstance() {
		return null;
	}

	private FragmentManager fragmentManager;

	public FragmentManager getFragmentManager() {
		if (fragmentManager == null)
			fragmentManager = new FragmentManager(this);
		return fragmentManager;
	}

	public LayoutInflater getLayoutInflater() {
		return (LayoutInflater)getSystemService("layout_inflater");
	}

	public boolean isChangingConfigurations() { return false; }

	public boolean isInPictureInPictureMode() { return false; }

	/* No picture-in-picture mode here, so the params are dropped. Present
	 * because LaunchActivity's override calls super. */
	public void setPictureInPictureParams(PictureInPictureParams params) {}

	/* Nothing ever changes the picture-in-picture UI state, so this is never
	 * called. Present because androidx.activity's ComponentActivity declares it
	 * as an override. */
	public void onPictureInPictureUiStateChanged(PictureInPictureUiState state) {}

	/* No assistant here, so the content an activity fills in is dropped.
	 * Present because Fenix's HomeActivity and ExternalAppBrowserActivity
	 * override it and call super, and the nearest superclass that declares it is
	 * this one. */
	public void onProvideAssistContent(android.app.assist.AssistContent content) {}

	@Override
	public void onContentChanged() {
		Slog.i(TAG, "- onContentChanged - yay!");
	}

	public boolean onCreateOptionsMenu(Menu menu) {
		return true;
	}

	@Override
	public boolean onCreatePanelMenu(int featureId, Menu menu) {
		if (featureId == Window.FEATURE_OPTIONS_PANEL) {
			// HACK: catch non critical error occuring in Open Sudoku app
			try {
				return onCreateOptionsMenu(menu);
			} catch (Exception e) {
				e.printStackTrace();
				return false;
			}
		}
		return false;
	}

	@Override
	public View onCreatePanelView(int featureId) {
		return null;
	}

	public MenuInflater getMenuInflater() {
		return new MenuInflater(this);
	}

	public boolean onPrepareOptionsMenu(Menu menu) {
		return true;
	}

	@Override
	public boolean onPreparePanel(int featureId, View view, Menu menu) {
		if (featureId == Window.FEATURE_OPTIONS_PANEL && menu != null) {
			return onPrepareOptionsMenu(menu);
		}
		return true;
	}

	@Override
	public boolean onMenuItemSelected(int featureId, MenuItem item) {
		if (featureId == Window.FEATURE_OPTIONS_PANEL) {
			return onOptionsItemSelected(item);
		}
		return false;
	}

	public boolean onOptionsItemSelected(MenuItem item) {
		return false;
	}

	public void onOptionsMenuClosed(Menu menu) {}

	@Override
	public void onPanelClosed(int featureId, Menu menu) {
		if (featureId == Window.FEATURE_OPTIONS_PANEL) {
			onOptionsMenuClosed(menu);
		}
	}

	public void setTitle(CharSequence title) {
		this.title = title;
	}

	public void setTitle(int titleId) {
		this.title = getText(titleId);
	}

	public CharSequence getTitle() {
		return title;
	}

	public void onBackPressed() {
		System.out.println("onBackPressed() called");
		finish();
	}

	public void setIntent(Intent newIntent) {
		this.intent = newIntent;
	}

	public Intent getParentActivityIntent() {
		return null;
	}

	@Override
	public boolean onMenuOpened(int featureId, Menu menu) {
		Slog.i(TAG, "onMenuOpened(" + featureId + ", " + menu + ") called");
		return false;
	}

	public void recreate() {
		finishing = true;
		try {
			/* TODO: check if this is a toplevel activity */
			Activity activity = internalCreateActivity(this.getClass().getName(), getWindow().native_window, intent);
			new Handler().post(new Runnable() {
				@Override
				public void run() {
					nativeFinish(0);
					nativeStartActivity(activity);
				}
			});
		} catch (ReflectiveOperationException e) {
			Slog.i(TAG, "exception in Activity.recreate, this is kinda sus");
			e.printStackTrace();
		}
	}

	public String getLocalClassName() {
		final String pkg = getPackageName();
		final String cls = this.getClass().getName();
		int packageLen = pkg.length();
		if (!cls.startsWith(pkg) || cls.length() <= packageLen || cls.charAt(packageLen) != '.') {
			return cls;
		}
		return cls.substring(packageLen + 1);
	}

	public SharedPreferences getPreferences(int mode) {
		return getSharedPreferences(getLocalClassName(), mode);
	}

	protected void onNewIntent(Intent intent) {}

	public final Activity getParent() {
		return null;
	}

	public boolean hasWindowFocus() {
		return true; // FIXME?
	}

	public boolean isDestroyed() {
		return destroyed;
	}

	public void finishAffinity() {
		finish();
	}

	/* No task stack here, so there is no task to remove: finishing is all of it. */
	public void finishAndRemoveTask() {
		finish();
	}

	public void overridePendingTransition(int enterAnim, int exitAnim) {}

	public native boolean isTaskRoot();

	public void postponeEnterTransition() {}

	public void startPostponedEnterTransition() {}

	public boolean isChild() {
		return false;
	}

	public void setTaskDescription(ActivityManager.TaskDescription description) {}

	private native void nativeFinish(long native_window);
	public static native void nativeStartActivity(Activity activity);
	public static native boolean nativeResumeActivity(Class<? extends Activity> activityClass, Intent intent);
	public static native void nativeOpenURI(String uri);
	public void reportFullyDrawn() {}
	public void setVisible(boolean visible) {}
	public Uri getReferrer() { return null; }
	public void setDefaultKeyMode(int flag) {}
	public void registerForContextMenu(View view) {}
	public native boolean isInMultiWindowMode();

	private final List<Application.ActivityLifecycleCallbacks> activityLifecycleCallbacks =
	    new java.util.concurrent.CopyOnWriteArrayList<>();

	public void registerActivityLifecycleCallbacks(Application.ActivityLifecycleCallbacks callback) {
		activityLifecycleCallbacks.add(callback);
	}

	public void unregisterActivityLifecycleCallbacks(Application.ActivityLifecycleCallbacks callback) {
		activityLifecycleCallbacks.remove(callback);
	}

	private static final int LIFECYCLE_PRE = 0, LIFECYCLE_ON = 1, LIFECYCLE_POST = 2;
	private static final int LIFECYCLE_CREATE = 0, LIFECYCLE_START = 1, LIFECYCLE_RESUME = 2,
	    LIFECYCLE_PAUSE = 3, LIFECYCLE_STOP = 4, LIFECYCLE_DESTROY = 5,
	    LIFECYCLE_SAVE_INSTANCE_STATE = 6;

	/* AOSP order: the application's callbacks run before the activity's own. */
	private void dispatchLifecycle(int state, int phase, Bundle bundle) {
		Application application = getApplication();
		if (application != null) {
			for (Application.ActivityLifecycleCallbacks callback : application.getActivityLifecycleCallbacks())
				dispatchLifecycle(callback, state, phase, bundle);
		}
		for (Application.ActivityLifecycleCallbacks callback : activityLifecycleCallbacks)
			dispatchLifecycle(callback, state, phase, bundle);
	}

	private void dispatchLifecycle(Application.ActivityLifecycleCallbacks callback, int state,
	                               int phase, Bundle bundle) {
		switch (state) {
		case LIFECYCLE_CREATE:
			if (phase == LIFECYCLE_PRE) callback.onActivityPreCreated(this, bundle);
			else if (phase == LIFECYCLE_ON) callback.onActivityCreated(this, bundle);
			else callback.onActivityPostCreated(this, bundle);
			break;
		case LIFECYCLE_START:
			if (phase == LIFECYCLE_PRE) callback.onActivityPreStarted(this);
			else if (phase == LIFECYCLE_ON) callback.onActivityStarted(this);
			else callback.onActivityPostStarted(this);
			break;
		case LIFECYCLE_RESUME:
			if (phase == LIFECYCLE_PRE) callback.onActivityPreResumed(this);
			else if (phase == LIFECYCLE_ON) callback.onActivityResumed(this);
			else callback.onActivityPostResumed(this);
			break;
		case LIFECYCLE_PAUSE:
			if (phase == LIFECYCLE_PRE) callback.onActivityPrePaused(this);
			else if (phase == LIFECYCLE_ON) callback.onActivityPaused(this);
			else callback.onActivityPostPaused(this);
			break;
		case LIFECYCLE_STOP:
			if (phase == LIFECYCLE_PRE) callback.onActivityPreStopped(this);
			else if (phase == LIFECYCLE_ON) callback.onActivityStopped(this);
			else callback.onActivityPostStopped(this);
			break;
		case LIFECYCLE_DESTROY:
			if (phase == LIFECYCLE_PRE) callback.onActivityPreDestroyed(this);
			else if (phase == LIFECYCLE_ON) callback.onActivityDestroyed(this);
			else callback.onActivityPostDestroyed(this);
			break;
		case LIFECYCLE_SAVE_INSTANCE_STATE:
			if (phase == LIFECYCLE_PRE) callback.onActivityPreSaveInstanceState(this, bundle);
			else if (phase == LIFECYCLE_ON) callback.onActivitySaveInstanceState(this, bundle);
			else callback.onActivityPostSaveInstanceState(this, bundle);
			break;
		}
	}

	/* The lifecycle entry points the native side calls; they wrap the protected
	 * onX() an app may override, so that ActivityLifecycleCallbacks see the same
	 * pre/on/post sequence AOSP produces. */
	void performCreate(Bundle savedInstanceState) {
		dispatchLifecycle(LIFECYCLE_CREATE, LIFECYCLE_PRE, savedInstanceState);
		onCreate(savedInstanceState);
		/* A fragment added from the app's own onCreate() - androidx's
		 * ReportFragment is - was not in the list when onCreate() looped over
		 * it. Catch it up, then hand every fragment onActivityCreated(), which
		 * is ReportFragment's only source of ON_CREATE below SDK 29. */
		for (Fragment fragment : new ArrayList<>(fragments)) {
			if (!fragment.created) {
				fragment.created = true;
				fragment.onCreate(savedInstanceState);
			}
			fragment.onActivityCreated(savedInstanceState);
		}
		dispatchLifecycle(LIFECYCLE_CREATE, LIFECYCLE_ON, savedInstanceState);
		dispatchLifecycle(LIFECYCLE_CREATE, LIFECYCLE_POST, savedInstanceState);
	}

	void performStart() {
		dispatchLifecycle(LIFECYCLE_START, LIFECYCLE_PRE, null);
		onStart();
		dispatchLifecycle(LIFECYCLE_START, LIFECYCLE_ON, null);
		dispatchLifecycle(LIFECYCLE_START, LIFECYCLE_POST, null);
	}

	void performResume() {
		dispatchLifecycle(LIFECYCLE_RESUME, LIFECYCLE_PRE, null);
		onResume();
		dispatchLifecycle(LIFECYCLE_RESUME, LIFECYCLE_ON, null);
		dispatchLifecycle(LIFECYCLE_RESUME, LIFECYCLE_POST, null);
	}

	void performPause() {
		dispatchLifecycle(LIFECYCLE_PAUSE, LIFECYCLE_PRE, null);
		onPause();
		dispatchLifecycle(LIFECYCLE_PAUSE, LIFECYCLE_ON, null);
		dispatchLifecycle(LIFECYCLE_PAUSE, LIFECYCLE_POST, null);
	}

	void performStop() {
		dispatchLifecycle(LIFECYCLE_STOP, LIFECYCLE_PRE, null);
		onStop();
		dispatchLifecycle(LIFECYCLE_STOP, LIFECYCLE_ON, null);
		dispatchLifecycle(LIFECYCLE_STOP, LIFECYCLE_POST, null);
	}

	void performDestroy() {
		dispatchLifecycle(LIFECYCLE_DESTROY, LIFECYCLE_PRE, null);
		onDestroy();
		dispatchLifecycle(LIFECYCLE_DESTROY, LIFECYCLE_ON, null);
		dispatchLifecycle(LIFECYCLE_DESTROY, LIFECYCLE_POST, null);
	}

	public void setDisablePreviewScreenshots(boolean disable) {}
	public final View requireViewById(int id) {
		View view = findViewById(id);
		if (view == null)
			throw new IllegalArgumentException("ID does not reference a View inside this View");
		return view;
	}

	public View onCreateView(View parent, String name, Context context, AttributeSet attrs) {
		return null;
	}

	public boolean onSearchRequested() {
		return false;
	}

	public View getCurrentFocus() {
		return null;
	}

	public void setProgressBarIndeterminateVisibility(boolean indeterminate) {}

	public int getChangingConfigurations() {
		return 0;
	}

	public int getTaskId() {
		/* we don't support multiple activity stacks, so this is probably fine? */
		return System.identityHashCode(this.getApplicationContext());
	}

	boolean moveTaskToBack(boolean nonroot) {
		return true;
	}

	void setFinishOnTouchOutside(boolean finish) {
	}

	public void closeOptionsMenu() {
	}

	public void finishActivity(int requestCode) {
		/* TODO: track started activities so we can finish the right one here */
		Slog.w(TAG, "finishActivity: stub");
	}

	public void finishAfterTransition() {
		finish();
	}

	public boolean dispatchKeyEvent(KeyEvent event) {
		return false;
	}

	public void requestPermissions(final String[] permissions, final int requestCode) {
		/* ATL has no permission dialog; resolve against checkPermission (which grants
		 * everything except env-gated location/mic) and deliver the result asynchronously
		 * so the app's onRequestPermissionsResult / ActivityResult callback completes. */
		final int[] grantResults = new int[permissions.length];
		for (int i = 0; i < permissions.length; i++)
			grantResults[i] = getPackageManager().checkPermission(permissions[i], getPackageName());
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				onRequestPermissionsResult(requestCode, permissions, grantResults);
			}
		});
	}

	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {}

	public boolean shouldShowRequestPermissionRationale(String permission) {
		return false;
	}

	public ActionBar getActionBar() {
		return null;
	}

	@Override
	public void atl_attach_base_context(Context baseContext) {
		super.atl_attach_base_context(baseContext);
		// Setting up a window requires a context.
		this.window = new Window(this, this);
	}

	/**
	 * Manifest-driven setup, run by ATLLoadedApp.createActivity once the window exists.
	 *
	 * The title goes onto the field rather than through the (overridable) setTitle():
	 * for an AppCompatActivity setTitle() routes through AppCompatDelegate and
	 * prematurely installs the sub-decor - resolving windowBackground from the
	 * manifest (launcher) theme before the app's own onCreate gets to switch themes.
	 * onStart() applies the field to the native window title.
	 */
	public void atl_apply_manifest(int softInputMode, CharSequence title) {
		this.window.setSoftInputMode(softInputMode);
		if (title != null)
			this.title = title;
	}

	public android.view.ActionMode startActionMode(android.view.ActionMode.Callback a0) { return null; }

	public android.view.ActionMode startActionMode(android.view.ActionMode.Callback a0, int a1) { return null; }
}
