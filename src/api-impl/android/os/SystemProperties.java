package android.os;

import java.util.Properties;

/**
 * The system properties an app sees: ATL's own answers first, then the host
 * Android property store where there is one (a Halium port's container, read
 * through libhybris - see android_os_SystemProperties.c), then ATL's fallbacks
 * for a host with no opinion.
 *
 * The order matters. The SDK level is ATL's to decide and must not come from
 * the device; ro.product.* is the device's to answer and ATL only ever had a
 * placeholder for it.
 */
public class SystemProperties {
	/* ATL's own answers, plus whatever set() stored */
	private static final Properties properties = new Properties();
	/* used only when neither ATL nor the host has an answer */
	private static final Properties defaults = new Properties();
	/* what the host store said, including "nothing" */
	private static final Properties hostCache = new Properties();

	private static final String NO_VALUE = "";

	static {
		/* the app-visible SDK level lives in one place; see android.atl.ATLSdkLevel */
		properties.put("ro.build.version.sdk", "" + android.atl.ATLSdkLevel.SDK_INT);
		properties.put("ro.build.version.release", android.atl.ATLSdkLevel.RELEASE);
		properties.put("ro.build.version.codename", android.atl.ATLSdkLevel.CODENAME);
		properties.put("ro.build.version.release_or_codename",
		    "REL".equals(android.atl.ATLSdkLevel.CODENAME) ? android.atl.ATLSdkLevel.RELEASE
		                                                   : android.atl.ATLSdkLevel.CODENAME);
		properties.put("ro.build.tags", "release-keys");
		properties.put("ro.build.type", "user");
		defaults.put("ro.product.brand", "google");
		defaults.put("ro.product.manufacturer", "HTC"); /* picked at random, long ago */
		// TODO how to actually get the system's supported abis?
		switch (System.getProperty("os.arch")) {
		case "x86_64":
			properties.put("ro.product.cpu.abi", "x86_64");
			properties.put("ro.product.cpu.abi2", "x86");
			properties.put("ro.product.cpu.abilist", "x86_64,x86");
			break;
		case "aarch64":
			properties.put("ro.product.cpu.abi", "arm64-v8a");
			properties.put("ro.product.cpu.abi2", "armeabi-v7a");
			properties.put("ro.product.cpu.abilist", "arm64-v8a,armeabi-v7a,armeabi");
			break;
		case "arm":
			properties.put("ro.product.cpu.abi", "armeabi-v7a");
			properties.put("ro.product.cpu.abi2", "armeabi");
			properties.put("ro.product.cpu.abilist", "armeabi-v7a,armeabi");
			break;
		}
		loadOverrides(System.getenv("ATL_SYSTEM_PROPERTIES"));
	}

	/**
	 * ATL_SYSTEM_PROPERTIES=key=value[,key=value...], applied last so it wins
	 * over the host store as well as over ATL's own answers.
	 *
	 * Apps carry feature flags that a property can switch: Google Camera looks
	 * every one of its flags up as a property before it asks Phenotype or
	 * GServices, neither of which ATL has, so a flag it would otherwise read
	 * off Google's servers is settable from the launch environment.
	 */
	private static void loadOverrides(String spec) {
		if (spec == null || spec.isEmpty())
			return;

		for (String entry : spec.split(",")) {
			int equals = entry.indexOf('=');

			if (equals <= 0) {
				android.util.Log.w("SystemProperties",
				    "ATL_SYSTEM_PROPERTIES: ignoring '" + entry + "', not key=value");
				continue;
			}
			String key = entry.substring(0, equals).trim();
			String value = entry.substring(equals + 1).trim();

			if (key.isEmpty())
				continue;
			properties.put(key, value);
			android.util.Log.i("SystemProperties", "ATL_SYSTEM_PROPERTIES: " + key + " = " + value);
		}
	}

	/* the hidden setter apps reach by reflection; there is no system property store
	 * behind this, so the value only comes back out of get() */
	public static void set(String prop, String value) {
		android.util.Log.i("SystemProperties", "Setting String prop " + prop + " = " + value);
		if (value == null)
			properties.remove(prop);
		else
			properties.put(prop, value);
	}

	public static String get(String prop) {
		String value = lookup(prop);

		android.util.Log.i("SystemProperties", "Grabbing String prop " + prop + " = " + value);
		return value;
	}

	public static String get(String prop, String def) {
		String value = lookup(prop);

		android.util.Log.i("SystemProperties", "Grabbing String prop " + prop + ", default " + def
		    + " = " + value);
		return value == null ? def : value;
	}

	public static boolean getBoolean(String prop, boolean def) {
		String val = lookup(prop);
		android.util.Log.i("SystemProperties", "Grabbing boolean prop " + prop + ", default " + def);
		return val == null ? def : Boolean.parseBoolean(val);
	}

	public static int getInt(String prop, int def) {
		String val = lookup(prop);
		android.util.Log.i("SystemProperties", "Grabbing int prop " + prop + ", default " + def);
		try {
			return val == null ? def : Integer.parseInt(val.trim());
		} catch (NumberFormatException e) {
			return def;
		}
	}

	public static long getLong(String prop, long def) {
		String val = lookup(prop);
		android.util.Log.i("SystemProperties", "Grabbing long prop " + prop + ", default " + def);
		try {
			return val == null ? def : Long.parseLong(val.trim());
		} catch (NumberFormatException e) {
			return def;
		}
	}

	private static String lookup(String prop) {
		String value = properties.getProperty(prop);

		if (value == null)
			value = host(prop);
		if (value == null)
			value = defaults.getProperty(prop);
		return value;
	}

	/** the host Android property store, asked once per key and then remembered */
	private static synchronized String host(String prop) {
		String cached = hostCache.getProperty(prop);

		if (cached == null) {
			try {
				cached = native_get(prop);
			} catch (UnsatisfiedLinkError e) {
				/* asked before the native library was loaded: no answer, and
				 * nothing to remember either */
				return null;
			}
			hostCache.put(prop, cached == null ? NO_VALUE : cached);
		}
		return NO_VALUE.equals(cached) ? null : cached;
	}

	/** null when this host has no Android property store, or does not know the key */
	private static native String native_get(String prop);
}
