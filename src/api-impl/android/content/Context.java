package android.content;

import android.R;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlarmManager;
import android.app.AppOpsManager;
import android.app.Application;
import android.app.ContextImpl;
import android.app.KeyguardManager;
import android.app.NotificationManager;
import android.app.Service;
import android.app.SharedPreferencesImpl;
import android.app.StatusBarManager;
import android.app.UiModeManager;
import android.app.job.JobScheduler;
import android.atl.ATLLoadedApp;
import android.atl.ATLTimeZone;
import android.bluetooth.BluetoothManager;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageParser;
import android.content.pm.ShortcutManager;
import android.content.res.AssetManager;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.database.DatabaseErrorHandler;
import android.database.sqlite.SQLiteDatabase;
import android.graphics.drawable.Drawable;
import android.hardware.SensorManager;
import android.hardware.display.ColorDisplayManager;
import android.hardware.display.DisplayManager;
import android.hardware.input.InputManager;
import android.hardware.usb.UsbManager;
import android.location.LocationManager;
import android.media.AudioManager;
import android.media.MediaRouter;
import android.net.ConnectivityManager;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.ParcelFileDescriptor;
import android.os.PowerManager;
import android.os.RemoteException;
import android.os.UserManager;
import android.os.Vibrator;
import android.security.keystore.AndroidKeyStoreProvider;
import android.telephony.TelephonyManager;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Slog;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.WindowManagerImpl;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.CaptioningManager;
import android.view.inputmethod.InputMethodManager;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.security.Security;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public abstract class Context {
	private final static String TAG = "Context";

	public static final int MODE_PRIVATE = 0;
	public static final String ACTIVITY_SERVICE = "activity";
	public static final String AUDIO_SERVICE = "audio";
	public static final String CLIPBOARD_SERVICE = "clipboard";
	public static final String DISPLAY_SERVICE = "display";
	public static final String INPUT_METHOD_SERVICE = "input_method";
	public static final String LOCATION_SERVICE = "location";
	public static final String MEDIA_ROUTER_SERVICE = "media_router";
	public static final String POWER_SERVICE = "power";
	public static final String VIBRATOR_SERVICE = "vibrator";
	public static final String WINDOW_SERVICE = "window";

	public static Vibrator vibrator;

	public static PackageManager package_manager;
	public static Configuration sys_config;

	// TODO: Migrate to ATLLoadedApp, cannot remove yet due to being called by native
	// The current replacement is ATLLoadedApp.getPrimaryApplication().getApplication()
	@Deprecated
	private static Application this_application;

	File data_dir = null;
	File prefs_dir = null;
	File files_dir = null;
	File obb_dir = null;
	File cache_dir = null;
	File nobackup_dir = null;

	private static Map<IntentFilter, BroadcastReceiver> receiverMap = new ConcurrentHashMap<>();

	static {
		sys_config = new Configuration();
		native_updateConfig(sys_config);
		applyWindowSizeDp(sys_config, new DisplayMetrics());
		ATLLoadedApp primary_application = ATLLoadedApp.getPrimaryApplication();

		ApplicationInfo application_info = primary_application.pkg.applicationInfo;
		application_info.dataDir = Environment.getExternalStorageDirectory().getAbsolutePath();
		application_info.nativeLibraryDir = (new File(Environment.getExternalStorageDirectory(), "lib")).getAbsolutePath();
		application_info.sourceDir = native_get_apk_path();
		package_manager = new PackageManager();

		Security.addProvider(new AndroidKeyStoreProvider());

		for (PackageParser.Activity receiver : primary_application.pkg.receivers) {
			if (receiver.intents == null)
				continue;
			for (PackageParser.ActivityIntentInfo intent : receiver.intents) {
				if (intent.matchAction("org.unifiedpush.android.connector.MESSAGE")) {
					nativeExportUnifiedPush(application_info.packageName);
					break;
				}
			}
		}
	}

	/* screenWidthDp and friends describe the app's window in dp. The window size
	 * is already in DisplayMetrics, in pixels, so convert here rather than asking
	 * the native side, which used to leave all three at 0 -- Compose measures
	 * popups against AT_MOST(screenWidthDp * density) and got 0x0. One method,
	 * because the boot snapshot and every later window resize have to agree. */
	private static void applyWindowSizeDp(Configuration config, DisplayMetrics metrics) {
		config.screenWidthDp = (int)(metrics.widthPixels / metrics.density);
		config.screenHeightDp = (int)(metrics.heightPixels / metrics.density);
		config.smallestScreenWidthDp = Math.min(config.screenWidthDp, config.screenHeightDp);
	}

	/**
	 * The window's size changed after the static initialiser above snapshotted
	 * it. AOSP does this from ActivityThread.handleConfigurationChanged;
	 * Display.setWindowSize() calls it here. The toolkit dispatches
	 * onConfigurationChanged into each view tree separately, from
	 * ViewRootImpl.dispatchConfigurationChanged().
	 */
	public static void onWindowSizeChanged() {
		if (sys_config == null)
			return; /* the static initialiser is still running: it reads the new size itself */
		applySystemConfig();
		if (this_application != null)
			this_application.onConfigurationChanged(sys_config);
	}

	/**
	 * Native code edits sys_config in place (the colour scheme, the screen size
	 * bucket); Resources keeps a copy of it, so push ours back over that copy.
	 */
	public static void onSystemConfigChanged() {
		if (sys_config == null)
			return; /* the static initialiser is still running: it reads sys_config itself */
		applySystemConfig();
	}

	private static void applySystemConfig() {
		Resources r = ATLLoadedApp.getPrimaryApplication().default_resources;
		DisplayMetrics dm = r.getDisplayMetrics();
		dm.setToDefaults();
		applyWindowSizeDp(sys_config, dm);
		r.updateConfiguration(sys_config, dm);
	}

	private static native String native_get_apk_path();
	protected static native void native_updateConfig(Configuration config);
	protected static native void nativeOpenFile(int fd);
	protected static native void nativeComposeEmail(String text, int fd);
	private static native void nativeExportUnifiedPush(String packageName);
	private static native void nativeRegisterUnifiedPush(String token, String application);
	protected static native void nativeStartExternalService(Intent service);

	static Application createApplication(long native_window) throws Exception {
		// before any app code runs: apps cache date formatters built from the default timezone
		ATLTimeZone.init();

		Application application = ATLLoadedApp.getPrimaryApplication().getApplication();
		application.native_window = native_window;
		this_application = application;
		return application;
	}

	public Context() {
		Slog.v(TAG, "new Context! this one is: " + this);
	}

	public int checkPermission(String permission, int pid, int uid) {
		return getPackageManager().checkPermission(permission, getPackageName());
	}

	public abstract Resources.Theme getTheme();

	public abstract ApplicationInfo getApplicationInfo();

	public Context getApplicationContext() {
		return ATLLoadedApp.getPrimaryApplication().getApplication();
	}

	public ContentResolver getContentResolver() {
		return new ContentResolver();
	}

	public abstract Object getSystemService(String name);

	public abstract <T> T getSystemService(Class<T> serviceClass);

	public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
		if (receiver != null)
			receiverMap.put(filter, receiver);
		/* AOSP hands back the matching sticky broadcast; apps that pass a null
		 * receiver only want that (Open Camera reads the battery level this way)
		 * and do not expect null, so return an empty Intent and let them fall
		 * back to their own defaults. */
		return new Intent();
	}

	public Looper getMainLooper() {
		return Looper.getMainLooper();
	}

	public String getPackageName() {
		return getApplicationInfo().packageName;
	}

	public String getPackageCodePath() {
		return getApplicationInfo().sourceDir;
	}

	public int getColor(int resId) {
		return this.getResources().getColor(resId);
	}

	public final String getString(int resId) {
		return this.getResources().getString(resId);
	}

	public final String getString(int resId, Object... formatArgs) {
		return this.getResources().getString(resId, formatArgs);
	}

	public PackageManager getPackageManager() {
		return package_manager;
	}

	public abstract Resources getResources();

	public AssetManager getAssets() {
		return getResources().getAssets();
	}

	private File makeFilename(File base, String name) {
		if (name.indexOf(File.separatorChar) < 0) {
			return new File(base, name);
		}
		throw new IllegalArgumentException(
		    "File " + name + " contains a path separator");
	}

	private File getDataDirFile() {
		if (data_dir == null) {
			data_dir = android.os.Environment.getExternalStorageDirectory();
		}
		return data_dir;
	}

	public File getDataDir() {
		return getDataDirFile();
	}

	public File getFilesDir() {
		if (files_dir == null) {
			files_dir = new File(getDataDirFile(), "files");
		}
		if (!files_dir.exists()) {
			if (!files_dir.mkdirs()) {
				if (files_dir.exists()) {
					// spurious failure; probably racing with another process for this app
					return files_dir;
				}
				Slog.w(TAG, "Unable to create files directory " + files_dir.getPath());
				return null;
			}
		}
		return files_dir;
	}

	public File getExternalFilesDir(String type) {
		return getFilesDir();
	}

	public File[] getExternalFilesDirs(String type) {
		return new File[] {getExternalFilesDir(type)};
	}

	public File getObbDir() {
		if (obb_dir == null) {
			obb_dir = new File(getDataDirFile(), "Android/obb/" + getPackageName());
		}
		if (!obb_dir.exists()) {
			if (!obb_dir.mkdirs()) {
				if (obb_dir.exists()) {
					// spurious failure; probably racing with another process for this app
					return obb_dir;
				}
				Slog.w(TAG, "Unable to create obb directory >" + obb_dir.getPath() + "<");
				return null;
			}
		}
		return obb_dir;
	}

	public File[] getObbDirs() {
		return new File[] {getObbDir()};
	}

	public File getCacheDir() {
		if (cache_dir == null) {
			cache_dir = new File("/tmp/atl_cache/" + getPackageName());
		}
		if (!cache_dir.exists()) {
			if (!cache_dir.mkdirs()) {
				if (cache_dir.exists()) {
					// spurious failure; probably racing with another process for this app
					return cache_dir;
				}
				Slog.w(TAG, "Unable to create cache directory >" + cache_dir.getPath() + "<");
				return null;
			}
		}
		return cache_dir;
	}

	public File getExternalCacheDir() {
		return getCacheDir();
	}

	public File[] getExternalCacheDirs() {
		return new File[] {getCacheDir()};
	}

	public File getNoBackupFilesDir() {
		if (nobackup_dir == null) {
			nobackup_dir = new File(getDataDirFile(), "no_backup/" + getPackageName());
		}
		if (!nobackup_dir.exists()) {
			if (!nobackup_dir.mkdirs()) {
				if (nobackup_dir.exists()) {
					// spurious failure; probably racing with another process for this app
					return nobackup_dir;
				}
				Slog.w(TAG, "Unable to create no_backup directory >" + nobackup_dir.getPath() + "<");
				return null;
			}
		}
		return nobackup_dir;
	}

	private File getPreferencesDir() {
		if (prefs_dir == null) {
			prefs_dir = new File(getDataDirFile(), "shared_prefs");
		}
		return prefs_dir;
	}

	public File[] getExternalMediaDirs() {
		return getExternalFilesDirs("media");
	}

	public File getDir(String name, int mode) {
		File dir = new File(getFilesDir(), name);
		if (!dir.exists()) {
			if (!dir.mkdirs()) {
				if (dir.exists()) {
					// spurious failure; probably racing with another process for this app
					return dir;
				}
				Slog.w(TAG, "Unable to create directory >" + dir.getPath() + "<");
				return null;
			}
		}
		return dir;
	}

	public File getFileStreamPath(String name) {
		return makeFilename(getFilesDir(), name);
	}

	public File getSharedPrefsFile(String name) {
		return makeFilename(getPreferencesDir(), name + ".xml");
	}

	private static Map<String, SharedPreferences> sharedPrefs = new HashMap<String, SharedPreferences>();

	public SharedPreferences getSharedPreferences(String name, int mode) {
		Slog.v(TAG, "\n\n...> getSharedPreferences(" + name + ")\n\n");
		if (sharedPrefs.containsKey(name)) {
			return sharedPrefs.get(name);
		} else {
			File prefsFile = getSharedPrefsFile(name);
			SharedPreferences prefs = new SharedPreferencesImpl(prefsFile, mode);
			sharedPrefs.put(name, prefs);
			return prefs;
		}
	}

	public abstract ClassLoader getClassLoader();

	public abstract ComponentName startService(Intent intent);

	// TODO: do these both work? make them look more alike
	public FileInputStream openFileInput(String name) throws FileNotFoundException {
		Slog.v(TAG, "openFileInput called for: '" + name + "'");
		File file = new File(getFilesDir(), name);

		return new FileInputStream(file);
	}

	public FileOutputStream openFileOutput(String name, int mode) throws java.io.FileNotFoundException {
		Slog.v(TAG, "openFileOutput called for: '" + name + "'");
		return new FileOutputStream(android.os.Environment.getExternalStorageDirectory().getPath() + "/files/" + name);
	}

	public int checkCallingOrSelfPermission(String permission) {
		return getPackageManager().checkPermission(permission, getPackageName());
	}

	public int checkSelfPermission(String permission) {
		return checkCallingOrSelfPermission(permission);
	}

	public void registerComponentCallbacks(ComponentCallbacks callbacks) {}

	public void unregisterComponentCallbacks(ComponentCallbacks callbacks) {}

	public abstract boolean bindService(final Intent intent, final ServiceConnection serviceConnection, int flags);

	/* For use from native code */
	static Activity resolveActivityInternal(Intent intent) throws ReflectiveOperationException {
		String className = null;
		ATLLoadedApp primary = ATLLoadedApp.getPrimaryApplication();
		if (intent.getComponent() != null) {
			className = intent.getComponent().getClassName();
		} else {
			int best_score = -5;
			for (PackageParser.Activity activity : primary.pkg.activities) {
				for (PackageParser.IntentInfo intentInfo : activity.intents) {
					int score = intentInfo.match(intent.getAction(), intent.getType(), intent.getScheme(), intent.getData(), intent.getCategories(), "Context");
					if (score > best_score && score > 0) {
						className = activity.className;
						best_score = score;
					}
				}
			}
		}
		if (className != null) {
			return Activity.internalCreateActivity(className, primary.getNativeWindow(), intent);
		} else {
			return null;
		}
	}

	public abstract void startActivity(Intent intent);

	public void startActivity(Intent intent, Bundle options) {
		startActivity(intent);
	}

	public void startActivities(Intent[] intents, Bundle options) {
		for (Intent intent : intents)
			startActivity(intent, options);
	}

	public void startActivities(Intent[] intents) {
		startActivities(intents, null);
	}

	public final TypedArray obtainStyledAttributes(AttributeSet set, int[] attrs) {
		return getTheme().obtainStyledAttributes(set, attrs, 0, 0);
	}
	public final TypedArray obtainStyledAttributes(AttributeSet set, int[] attrs, int defStyleAttr, int defStyleRes) {
		return getTheme().obtainStyledAttributes(set, attrs, defStyleAttr, defStyleRes);
	}
	public final TypedArray obtainStyledAttributes(int resid, int[] attrs) {
		return getTheme().obtainStyledAttributes(resid, attrs);
	}
	public final TypedArray obtainStyledAttributes(int[] attrs) {
		return getTheme().obtainStyledAttributes(attrs);
	}

	public abstract void setTheme(int resId);

	public final CharSequence getText(int resId) {
		return getResources().getText(resId);
	}

	public final ColorStateList getColorStateList(int id) {
		return getResources().getColorStateList(id);
	}

	public final Drawable getDrawable(int resId) {
		return getResources().getDrawable(resId, getTheme());
	}

	public abstract boolean isRestricted();

	/** @hide */
	public boolean canLoadUnsafeResources() { return true; }

	public File getDatabasePath(String dbName) {
		File databaseDir = new File(getDataDirFile(), "databases");
		if (!databaseDir.exists())
			databaseDir.mkdirs();
		return new File(databaseDir, dbName);
	}

	public void sendBroadcast(Intent intent) {
		if ("org.unifiedpush.android.distributor.REGISTER".equals(intent.getAction())) {
			nativeRegisterUnifiedPush(intent.getStringExtra("token"), intent.getStringExtra("application"));
		}
		for (IntentFilter filter : receiverMap.keySet()) {
			if (filter.matchAction(intent.getAction())) {
				receiverMap.get(filter).onReceive(this, intent);
			}
		}
		ATLLoadedApp.getPrimaryApplication().receiveBroadcast(this, intent);
	}

	/** Stop a running service from its own stopSelf(); see Service. */
	public static boolean stopRunningService(Service service) {
		return service != null && service.get_atl_loaded_app().stopRunningService(service);
	}

	public abstract boolean stopService(Intent intent);

	public void unbindService(ServiceConnection serviceConnection) {}

	public void unregisterReceiver(BroadcastReceiver receiver) {
		while (receiverMap.values().remove(receiver))
			;
	}

	public static final int CONTEXT_INCLUDE_CODE = 0x00000001;
	public static final int CONTEXT_IGNORE_SECURITY = 0x00000002;
	public static final int CONTEXT_RESTRICTED = 0x00000004;

	public abstract Context createPackageContext(String packageName, int flags);

	public void grantUriPermission(String dummy, Uri dummy2, int dummy3) {
		System.out.println("grantUriPermission(" + dummy + ", " + dummy2 + ", " + dummy3 + ") called");
	}

	public static final int MODE_ENABLE_WRITE_AHEAD_LOGGING = 0x0008;
	public static final int MODE_NO_LOCALIZED_COLLATORS = 0x0010;

	public SQLiteDatabase openOrCreateDatabase(String name, int mode, SQLiteDatabase.CursorFactory factory) {
		return openOrCreateDatabase(name, mode, factory, null);
	}

	public SQLiteDatabase openOrCreateDatabase(String filename, int mode, SQLiteDatabase.CursorFactory factory, DatabaseErrorHandler errorHandler) {
		int flags = SQLiteDatabase.CREATE_IF_NECESSARY;
		if ((mode & MODE_ENABLE_WRITE_AHEAD_LOGGING) != 0) {
			flags |= SQLiteDatabase.ENABLE_WRITE_AHEAD_LOGGING;
		}
		if ((mode & MODE_NO_LOCALIZED_COLLATORS) != 0) {
			flags |= SQLiteDatabase.NO_LOCALIZED_COLLATORS;
		}
		SQLiteDatabase db = SQLiteDatabase.openDatabase(filename, factory, flags, errorHandler);
		return db;
	}

	public boolean deleteDatabase(String name) {
		File dbFile = getDatabasePath(name);
		return dbFile.delete();
	}

	public abstract Context createConfigurationContext(Configuration configuration);

	public void sendOrderedBroadcast(Intent intent, String receiverPermission, BroadcastReceiver resultReceiver, Handler handler, int flags, String extra, Bundle options) {
		System.out.println("sendOrderedBroadcast(" + intent + ", " + receiverPermission + ", " + resultReceiver + ", " + handler + ", " + flags + ", " + extra + ", " + options + ") called");
	}

	public abstract Context createDisplayContext(Display display);

	public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter, String broadcastPermission, Handler scheduler) {
		return registerReceiver(receiver, filter);
	}

	/* API 26+: the RECEIVER_EXPORTED/RECEIVER_NOT_EXPORTED flags variant */
	public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter, int flags) {
		return registerReceiver(receiver, filter);
	}

	public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter, String broadcastPermission, Handler scheduler, int flags) {
		return registerReceiver(receiver, filter);
	}

	public String[] fileList() {
		return new String[0];
	}

	public void revokeUriPermission(Uri uri, int mode) {
		System.out.println("revokeUriPermission(" + uri + ", " + mode + ") called");
	}

	public String getAttributionTag() {
		return null;
	}
	public abstract boolean isDeviceProtectedStorage();

	public Drawable getWallpaper() {
		return null;
	}

	public String[] databaseList() {
		File databaseDir = new File(getDataDirFile(), "databases");
		if (databaseDir.exists()) {
			return databaseDir.list();
		} else {
			return new String[0];
		}
	}

	public abstract Context createDeviceProtectedStorageContext();

	public boolean moveSharedPreferencesFrom(Context sourceContext, String name) {
		File sourceFile = sourceContext.getSharedPrefsFile(name);
		if (!sourceFile.exists()) {
			return true;
		}
		deleteSharedPreferences(name);
		return sourceFile.renameTo(getSharedPrefsFile(name));
	}

	public boolean deleteSharedPreferences(String name) {
		getSharedPrefsFile(name).delete();
		sharedPrefs.remove(name);
		return true;
	}

	public String getPackageResourcePath() {
		return native_get_apk_path();
	}

	public abstract int getThemeResId();

	public android.content.ComponentName startForegroundService(Intent intent) {
		// we don't track the foreground state, so this is just a plain service start
		return startService(intent);
	}

	public android.content.Context createWindowContext(android.view.Display a0, int a1, android.os.Bundle a2) { return null; }

	public android.view.Display getDisplay() { return null; }


	public boolean bindIsolatedService(android.content.Intent a0, int a1, java.lang.String a2, java.util.concurrent.Executor a3, android.content.ServiceConnection a4) { return false; }

	/* Not null: callers hand this straight to APIs that dereference it -
	 * androidx.biometric NPEs on it before a Fragment can even start. */
	public java.util.concurrent.Executor getMainExecutor() {
		final android.os.Handler handler = new android.os.Handler(getMainLooper());
		return new java.util.concurrent.Executor() {
			@Override
			public void execute(Runnable command) {
				handler.post(command);
			}
		};
	}

	public static final int BIND_AUTO_CREATE = 1;

	public static final int BIND_IMPORTANT = 64;

	public static final int BIND_WAIVE_PRIORITY = 32;

	public static final java.lang.String ACCESSIBILITY_SERVICE = "accessibility";

	public static final java.lang.String CAMERA_SERVICE = "camera";

	public static final java.lang.String CONNECTIVITY_SERVICE = "connectivity";

	public static final java.lang.String CREDENTIAL_SERVICE = "credential";

	public static final java.lang.String INPUT_SERVICE = "input";

	public static final java.lang.String MEDIA_PROJECTION_SERVICE = "media_projection";

	public static final java.lang.String PRINT_SERVICE = "print";

	public static final java.lang.String SENSOR_SERVICE = "sensor";

	public static final java.lang.String UI_MODE_SERVICE = "uimode";

	public static final java.lang.String USER_SERVICE = "user";

	public static final java.lang.String WIFI_SERVICE = "wifi";

	public void updateServiceGroup(android.content.ServiceConnection a0, int a1, int a2) { }

	public int checkUriPermission(android.net.Uri a0, int a1, int a2, int a3) { return 0; }


	public boolean bindService(android.content.Intent a0, int a1, java.util.concurrent.Executor a2, android.content.ServiceConnection a3) { return false; }

	public void sendBroadcast(android.content.Intent a0, java.lang.String a1) { }

	public abstract ATLLoadedApp get_atl_loaded_app();
}
