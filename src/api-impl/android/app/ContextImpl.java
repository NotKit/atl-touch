package android.app;

import android.annotation.Nullable;
import android.annotation.UnsupportedAppUsage;
import android.app.SearchManager;
import android.app.job.JobScheduler;
import android.atl.ATLLoadedApp;
import android.bluetooth.BluetoothManager;
import android.content.*;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageParser;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.hardware.SensorManager;
import android.hardware.camera2.CameraManager;
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
import android.os.*;
import android.os.storage.StorageManager;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.util.Slog;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.WindowManagerImpl;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.CaptioningManager;
import android.view.inputmethod.InputMethodManager;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;

public final class ContextImpl extends Context {
	private final static String TAG = "ContextImpl";
	private final Resources resources;
	private final ATLLoadedApp atl_loaded_app;
	/**
	 * While not being part of the official Android API, some applications use it to reset/reload the theme in the context.
	 */
	@UnsupportedAppUsage
	private Resources.Theme mTheme;
	/**
	 * While not being part of the official Android API, some application use it to get the theme resource ID.
	 */
	@UnsupportedAppUsage
	private int mThemeResource;

	private final LayoutInflater layout_inflater = new LayoutInflater(this);
	private final JobScheduler job_scheduler = new JobScheduler(this);

	public ContextImpl(Resources resources, ATLLoadedApp loadedApp, Resources.Theme theme) {
		this(resources, loadedApp, 0);
		getTheme().setTo(theme);
	}

	public ContextImpl(Resources resources, ATLLoadedApp loadedApp, int themeResource) {
		this.resources = resources;
		this.atl_loaded_app = loadedApp;
		mThemeResource = themeResource;
	}

	@Nullable
	private ATLLoadedApp atl_get_intent_target(Intent intent) {
		ATLLoadedApp targetApp = null;
		String targetPackage = intent.getComponent() != null ? intent.getComponent().getPackageName() : null;
		if (targetPackage != null || (targetPackage = intent.getPackage()) != null) {
			if (targetPackage.equals(this.atl_loaded_app.pkg.packageName)) {
				targetApp = this.atl_loaded_app;
			} else if (targetPackage.equals(ATLLoadedApp.getPrimaryApplication().pkg.packageName)) {
				targetApp = ATLLoadedApp.getPrimaryApplication();
			}
		} else {
			targetApp = this.atl_loaded_app;
		}
		return targetApp;
	}

	@Override
	public ApplicationInfo getApplicationInfo() {
		return atl_loaded_app.pkg.applicationInfo;
	}

	@Override
	public void setTheme(int resId) {
		mThemeResource = resId;
		if (mTheme != null) {
			mTheme.applyStyle(resId, true);
		}
	}

	@Override
	public boolean stopService(Intent intent) {
		ATLLoadedApp targetApp = this.atl_get_intent_target(intent);
		if (targetApp == null) {
			return false;
		}
		return targetApp.stopService(intent);
	}

	@Override
	public Resources.Theme getTheme() {
		if (mTheme == null) {
			mTheme = resources.newTheme();
			if (mThemeResource != 0) {
				mTheme.applyStyle(mThemeResource, true);
			}
		}
		return mTheme;
	}

	@Override
	public Object getSystemService(String name) {
		switch (name) {
			case "window":
				return new WindowManagerImpl();
			case "clipboard":
				return new ClipboardManager();
			case "sensor":
				return new SensorManager();
			case "connectivity":
				return new ConnectivityManager();
			case "keyguard":
				return new KeyguardManager();
			case "phone":
				return new TelephonyManager();
			case "audio":
				return new AudioManager();
			case "activity":
				return new ActivityManager();
			case "usb":
				return new UsbManager();
			case "vibrator":
				return (vibrator != null) ? vibrator : (vibrator = new Vibrator());
			case "power":
				return new PowerManager();
			case "display":
				return new DisplayManager();
			case "media_router":
				return new MediaRouter();
			case "notification":
				return new NotificationManager();
			case "alarm":
				return new AlarmManager();
			case "input":
				return new InputManager();
			case "location":
				return new LocationManager();
			case "uimode":
				return new UiModeManager();
			case "input_method":
				return new InputMethodManager();
			case "accessibility":
				return new AccessibilityManager();
			case "layout_inflater":
				return layout_inflater;
			case "wifi":
				return new WifiManager();
			case "bluetooth":
				return new BluetoothManager();
			case "jobscheduler":
				return job_scheduler;
			case "appops":
				return new AppOpsManager();
			case "user":
				return new UserManager();
			case "captioning":
				return new CaptioningManager();
			case "statusbar":
				return new StatusBarManager();
			case "camera":
				return new CameraManager();
			case "color_display":
				return new ColorDisplayManager();
			case "search":
				return new SearchManager();
			case "storage":
				return new StorageManager();
			default:
				Slog.e(TAG, "!!!!!!! getSystemService: case >" + name + "< is not implemented yet");
				return null;
		}
	}

	@Override
	@SuppressWarnings("unchecked")
	public <T> T getSystemService(Class<T> serviceClass) {
		if (serviceClass == LayoutInflater.class)
			return (T)layout_inflater;
		if (serviceClass == JobScheduler.class)
			return (T)job_scheduler;
		// The name-keyed switch above is the real table; go through it rather
		// than reflecting, or an interface (WindowManager) has no constructor
		// at all and getConstructors()[0] throws AIOOBE past the catch.
		String name = SYSTEM_SERVICE_NAMES.get(serviceClass.getName());
		if (name != null)
			return (T)getSystemService(name);
		try {
			return (T)serviceClass.getConstructors()[0].newInstance();
		} catch (ReflectiveOperationException | ArrayIndexOutOfBoundsException e) {
			return null;
		}
	}

	private static final java.util.Map<String, String> SYSTEM_SERVICE_NAMES = new java.util.HashMap<>();
	static {
		String[] pairs = {
			"android.view.WindowManager", "window",
			"android.content.ClipboardManager", "clipboard",
			"android.hardware.SensorManager", "sensor",
			"android.net.ConnectivityManager", "connectivity",
			"android.app.KeyguardManager", "keyguard",
			"android.telephony.TelephonyManager", "phone",
			"android.media.AudioManager", "audio",
			"android.app.ActivityManager", "activity",
			"android.hardware.usb.UsbManager", "usb",
			"android.os.Vibrator", "vibrator",
			"android.os.PowerManager", "power",
			"android.hardware.display.DisplayManager", "display",
			"android.media.MediaRouter", "media_router",
			"android.app.NotificationManager", "notification",
			"android.app.AlarmManager", "alarm",
			"android.hardware.input.InputManager", "input",
			"android.location.LocationManager", "location",
			"android.app.UiModeManager", "uimode",
			"android.view.inputmethod.InputMethodManager", "input_method",
			"android.view.accessibility.AccessibilityManager", "accessibility",
			"android.net.wifi.WifiManager", "wifi",
			"android.bluetooth.BluetoothManager", "bluetooth",
			"android.app.AppOpsManager", "appops",
			"android.os.UserManager", "user",
			"android.view.accessibility.CaptioningManager", "captioning",
			"android.app.StatusBarManager", "statusbar",
			"android.hardware.camera2.CameraManager", "camera",
			"android.hardware.display.ColorDisplayManager", "color_display",
			"android.app.SearchManager", "search",
			"android.os.storage.StorageManager", "storage",
		};
		for (int i = 0; i < pairs.length; i += 2)
			SYSTEM_SERVICE_NAMES.put(pairs[i], pairs[i + 1]);
	}

	@Override
	public Resources getResources() {
		return resources;
	}

	@Override
	public ClassLoader getClassLoader() {
		return this.atl_loaded_app.class_loader;
	}

	public ComponentName startService(Intent intent) {
		// Newer applications use a Messenger instead of a BroadcastReceiver for the GCM token return Intent.
		// To support new and old apps with a common interface, we wrap the Messenger in a BroadcastReceiver
		if ("com.google.android.c2dm.intent.REGISTER".equals(intent.getAction()) && intent.getParcelableExtra("google.messenger") instanceof Messenger) {
			final Messenger messenger = (Messenger)intent.getParcelableExtra("google.messenger");
			this.registerReceiver(new BroadcastReceiver() {
				@Override
				public void onReceive(Context context, Intent resultIntent) {
					try {
						messenger.send(Message.obtain(null, 0, resultIntent));
					} catch (RemoteException e) {
						e.printStackTrace();
					}
				}
			}, new IntentFilter("com.google.android.c2dm.intent.REGISTRATION"));
		}
		ATLLoadedApp targetApp = this.atl_get_intent_target(intent);
		if (targetApp == null) {
			// External package. Try to start using DBus Action
			nativeStartExternalService(intent);
			return null;
		}
		return targetApp.startOrBindService(intent, null);
	}

	@Override
	public boolean bindService(Intent intent, ServiceConnection serviceConnection, int flags) {
		ATLLoadedApp targetApp = this.atl_get_intent_target(intent);
		if (targetApp == null) {
			return false;
		}
		return targetApp.startOrBindService(intent, serviceConnection) != null;
	}

	/* the manifest launchMode of one of our own activities, standard if it is not ours */
	private int getActivityLaunchMode(String className) {
		for (PackageParser.Activity activity : this.atl_loaded_app.pkg.activities) {
			if (className.equals(activity.className))
				return activity.info.launchMode;
		}
		return ActivityInfo.LAUNCH_MULTIPLE;
	}

	@Override
	public void startActivity(Intent intent) {
		Slog.i(TAG, "startActivity(" + intent + ") called");
		if (intent.getAction() != null && intent.getAction().equals("android.intent.action.CHOOSER")) {
			intent = (Intent)intent.getExtras().get("android.intent.extra.INTENT");
		}

		String className = null;
		if (intent.getComponent() != null) {
			className = intent.getComponent().getClassName();
		} else {
			if (intent.getAction() != null && intent.getAction().equals("android.intent.action.SEND")) {
				Slog.i(TAG, "sharing intent via share dialog: " + intent);
				final String text = intent.getStringExtra("android.intent.extra.TEXT");
				ParcelFileDescriptor fd = null;
				if (intent.hasExtra(Intent.EXTRA_STREAM)) {
					try {
						fd = getContentResolver().openFileDescriptor((Uri)intent.getParcelableExtra(Intent.EXTRA_STREAM), "r");
					} catch (FileNotFoundException e) {
						e.printStackTrace();
					}
				}
				final ParcelFileDescriptor fd_final = fd;
				/* The XDG specification does not provide anything comparable to the
				 * Android share API, so we offer copying to the clipboard or sending
				 * per mail through the org.freedesktop.portal.Email portal. The fd
				 * stays open until the dialog is dismissed. */
				Runnable runnable = new Runnable() {
					@Override
					public void run() {
						String path = null;
						if (fd_final != null) {
							try {
								path = java.nio.file.Files.readSymbolicLink(
								    java.nio.file.Paths.get("/proc/self/fd/" + fd_final.getFd())).toString();
							} catch (IOException e) {
								e.printStackTrace();
							}
						}
						final String detail = path != null ? path : text;
						AlertDialog dialog = new AlertDialog(ContextImpl.this);
						dialog.setTitle("Share");
						if (detail != null)
							dialog.setMessage(detail);
						dialog.setButton(DialogInterface.BUTTON_NEGATIVE, "Cancel", null);
						dialog.setButton(DialogInterface.BUTTON_POSITIVE, "Copy", new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int which) {
								if (detail != null)
									new ClipboardManager().setPrimaryClip(ClipData.newPlainText(null, detail));
							}
						});
						dialog.setButton(DialogInterface.BUTTON_NEUTRAL, "Email", new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int which) {
								nativeComposeEmail(text, fd_final != null ? fd_final.getFd() : -1);
							}
						});
						dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
							@Override
							public void onDismiss(DialogInterface d) {
								if (fd_final != null) {
									try {
										fd_final.close();
									} catch (IOException e) {
										e.printStackTrace();
									}
								}
							}
						});
						dialog.show();
					}
				};
				if (Looper.myLooper() == Looper.getMainLooper()) {
					runnable.run();
				} else {
					new Handler(Looper.getMainLooper()).post(runnable);
				}
				return;
			} else if (intent.getData() != null) {
				Slog.i(TAG, "starting extern activity with intent: " + intent);
				if (intent.getData().getScheme().equals("content")) {
					try (ParcelFileDescriptor fd = getContentResolver().openFileDescriptor(intent.getData(), "r")) {
						if (fd != null) {
							nativeOpenFile(fd.getFd());
							return;
						}
					} catch (IOException e) {
						e.printStackTrace();
					}
				}
				Activity.nativeOpenURI(String.valueOf(intent.getData()));
				return;
			}
			for (PackageParser.Activity activity : this.atl_loaded_app.pkg.activities) {
				for (PackageParser.IntentInfo intentInfo : activity.intents) {
					if (intentInfo.matchAction(intent.getAction())) {
						className = activity.className;
						break;
					}
				}
			}
		}
		if (className == null) {
			Slog.w(TAG, "startActivity: intent could not be handled.");
			return;
		}
		final String className_ = className;
		final Intent intent_ = intent;
		int launchMode = getActivityLaunchMode(className);
		/* singleTop only reuses the instance that is already on top; we resume whichever
		 * instance exists, which is the same thing for the single-window backlog here. */
		final boolean resumeExisting = launchMode == ActivityInfo.LAUNCH_SINGLE_TASK
		    || launchMode == ActivityInfo.LAUNCH_SINGLE_INSTANCE
		    || launchMode == ActivityInfo.LAUNCH_SINGLE_TOP
		    || (intent.getFlags() & (Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP)) != 0;
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				try {
					if (resumeExisting) {
						boolean found = Activity.nativeResumeActivity(
						    ContextImpl.this.atl_loaded_app.loadClass(className_).asSubclass(Activity.class),
						    intent_);
						if (found)
							return;
					}
					Activity activity = Activity.internalCreateActivity(
					    className_, ContextImpl.this.atl_loaded_app.getNativeWindow(), intent_);
					Activity.nativeStartActivity(activity);
				} catch (Exception e) {
					e.printStackTrace();
				}
			}
		});
	}

	@Override
	public Context createPackageContext(String packageName, int flags) {
		if (packageName.equals(atl_loaded_app.pkg.packageName)) {
			return this;
		}
		ATLLoadedApp primaryApplication = ATLLoadedApp.getPrimaryApplication();
		if (primaryApplication.pkg.packageName.equals(packageName)) {
			return primaryApplication.getApplication();
		}
		if (packageName.equals("android")) {
			// Shortcut for system application
			ATLLoadedApp system = ATLLoadedApp.getSystemApplication();
			return new ContextImpl(Resources.getSystem(), system,
			                       Resources.selectDefaultTheme(0, Build.VERSION.SDK_INT));
		}
		// Return the application context as a fallback
		Log.e(TAG, "!!!!!!! createPackageContext: case >" + packageName + "< is not implemented yet");
		return primaryApplication.getApplication();
	}

	@Override
	public Context createConfigurationContext(Configuration configuration) {
		return this.atl_loaded_app.createContext(null, configuration, getTheme());
	}

	@Override
	public Context createDisplayContext(Display display) {
		return new ContextImpl(getResources(), this.atl_loaded_app, getTheme());
	}

	@Override
	public Context createDeviceProtectedStorageContext() {
		return this;
	}

	@Override
	public int getThemeResId() {
		return mThemeResource;
	}

	@Override
	public ATLLoadedApp get_atl_loaded_app() {
		return atl_loaded_app;
	}
}
