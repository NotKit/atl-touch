package android.atl;

import android.util.Log;

/**
 * The SDK level ATL claims to an app, resolved once at startup.
 *
 * Every user of a version number (Build.VERSION, SystemProperties,
 * PackageParser) reads it from here, so an app only ever sees one answer.
 * Each value comes from a JVM system property (what --sdk-int and -X -D set)
 * or, if that is absent, from an environment variable - see doc/Envs.md.
 */
public class ATLSdkLevel {
	private static final String TAG = "ATLSdkLevel";

	/** the level ATL has always claimed by default */
	public static final int DEFAULT_SDK_INT = 9 /* Build.VERSION_CODES.GINGERBREAD */;

	public static final int SDK_INT;
	public static final int RESOURCES_SDK_INT;
	public static final String RELEASE;
	public static final String CODENAME;

	/**
	 * Whether a level was actually asked for. When nothing was, Build.VERSION
	 * takes the app's own minSdk instead of this class's default - an app that
	 * said nothing gets the platform it was built against, not Gingerbread.
	 */
	public static final boolean EXPLICIT;

	static {
		EXPLICIT = getString("Build.VERSION.SDK_INT", "ATL_SDK_INT", null) != null;
		int sdk = getInt("Build.VERSION.SDK_INT", "ATL_SDK_INT", DEFAULT_SDK_INT);
		int resources_sdk = getInt("Build.VERSION.RESOURCES_SDK_INT", "ATL_RESOURCES_SDK_INT", sdk);

		/* the resource level picks the app's -vNN buckets, so an app that reads
		 * SDK_INT and then a resource sees two different platforms; apps do
		 * crash on that, hence the warning rather than a silent fixup */
		if (resources_sdk != sdk)
			Log.w(TAG, "RESOURCES_SDK_INT " + resources_sdk + " differs from SDK_INT " + sdk +
			               ": the app's -vNN resource buckets and its Build.VERSION checks will disagree");

		SDK_INT = sdk;
		RESOURCES_SDK_INT = resources_sdk;
		CODENAME = getString("Build.VERSION.CODENAME", "ATL_SDK_CODENAME", "REL");
		RELEASE = getString("Build.VERSION.RELEASE", "ATL_SDK_RELEASE", releaseForSdk(sdk));

		Log.i(TAG, "claiming SDK " + SDK_INT + " (Android " + RELEASE + ", codename " + CODENAME +
		               "), resources at " + RESOURCES_SDK_INT);
	}

	private static int getInt(String property, String env, int fallback) {
		String value = getString(property, env, null);
		if (value == null)
			return fallback;
		try {
			return Integer.parseInt(value.trim());
		} catch (NumberFormatException e) {
			Log.e(TAG, property + "/" + env + " is '" + value + "', not a number; using " + fallback);
			return fallback;
		}
	}

	private static String getString(String property, String env, String fallback) {
		String value = System.getProperty(property);
		if (value == null)
			value = System.getenv(env);
		return (value != null && !value.isEmpty()) ? value : fallback;
	}

	/** the user-visible Android version for an SDK level; apps do parse this */
	public static String releaseForSdk(int sdk) {
		switch (sdk) {
			case 9: return "2.3";
			case 10: return "2.3.3";
			case 11: return "3.0";
			case 12: return "3.1";
			case 13: return "3.2";
			case 14: return "4.0";
			case 15: return "4.0.3";
			case 16: return "4.1";
			case 17: return "4.2";
			case 18: return "4.3";
			case 19: return "4.4";
			case 20: return "4.4W";
			case 21: return "5.0";
			case 22: return "5.1";
			case 23: return "6.0";
			case 24: return "7.0";
			case 25: return "7.1";
			case 26: return "8.0";
			case 27: return "8.1";
			case 28: return "9";
			case 29: return "10";
			case 30: return "11";
			case 31: return "12";
			case 32: return "12L";
			case 33: return "13";
			case 34: return "14";
			case 35: return "15";
			case 36: return "16";
			/* one release per level from here on, which has held since 10 */
			default: return sdk > 36 ? String.valueOf(sdk - 20) : String.valueOf(sdk);
		}
	}
}
