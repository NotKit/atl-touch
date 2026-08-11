package android.security;

public class KeyChain {
	public static final String ACTION_KEYCHAIN_CHANGED = "android.security.action.KEYCHAIN_CHANGED";
	public static final String ACTION_KEY_ACCESS_CHANGED = "android.security.action.KEY_ACCESS_CHANGED";
	public static final String ACTION_STORAGE_CHANGED = "android.security.STORAGE_CHANGED";
	public static final String ACTION_TRUST_STORE_CHANGED = "android.security.action.TRUST_STORE_CHANGED";
	public static final String EXTRA_CERTIFICATE = "CERT";
	public static final String EXTRA_KEY_ACCESSIBLE = "android.security.extra.KEY_ACCESSIBLE";
	public static final String EXTRA_KEY_ALIAS = "android.security.extra.KEY_ALIAS";
	public static final String EXTRA_NAME = "name";
	public static final String EXTRA_PKCS12 = "PKCS12";
	public static final String KEY_ALIAS_SELECTION_DENIED = "android:alias-selection-denied";
}
