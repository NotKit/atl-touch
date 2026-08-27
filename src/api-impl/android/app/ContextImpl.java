package android.app;

import android.annotation.UnsupportedAppUsage;
import android.app.SearchManager;
import android.app.job.JobScheduler;
import android.bluetooth.BluetoothManager;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.pm.ApplicationInfo;
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
import android.net.wifi.WifiManager;
import android.os.PowerManager;
import android.os.UserManager;
import android.os.Vibrator;
import android.os.storage.StorageManager;
import android.telephony.TelephonyManager;
import android.util.Slog;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.WindowManagerImpl;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.CaptioningManager;
import android.view.inputmethod.InputMethodManager;
import java.lang.reflect.InvocationTargetException;

public final class ContextImpl extends Context {
	private final static String TAG = "ContextImpl";
	private final Resources resources;
	private final ApplicationInfo application_info;
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

	public ContextImpl(Resources resources, ApplicationInfo applicationInfo, Resources.Theme theme) {
		this(resources, applicationInfo, 0);
		getTheme().setTo(theme);
	}

	public ContextImpl(Resources resources, ApplicationInfo applicationInfo, int themeResource) {
		this.resources = resources;
		this.application_info = applicationInfo;
		mThemeResource = themeResource;
	}

	@Override
	public ApplicationInfo getApplicationInfo() {
		return application_info;
	}

	@Override
	public void setTheme(int resId) {
		mThemeResource = resId;
		if (mTheme != null) {
			mTheme.applyStyle(resId, true);
		}
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
	public Context createPackageContext(String packageName, int flags) {
		return this;
	}

	@Override
	public Context createConfigurationContext(Configuration configuration) {
		return new ContextImpl(getResources(), getApplicationInfo(), getTheme());
	}

	@Override
	public Context createDisplayContext(Display display) {
		return new ContextImpl(getResources(), getApplicationInfo(), getTheme());
	}

	@Override
	public Context createDeviceProtectedStorageContext() {
		return this;
	}

	@Override
	public int getThemeResId() {
		return mThemeResource;
	}
}
