package android.security.keystore;

import java.security.Provider;

/**
 * The JCE provider that backs KeyStore.getInstance("AndroidKeyStore") and the
 * key generators apps ask it for.
 *
 * It is a named class rather than the anonymous subclass Context used to build
 * inline, because a provider that only exists as an anonymous class cannot be
 * named to anything that has to register it ahead of time. GraalVM's
 * native-image is the case in hand: JceSecurity builds its verification map at
 * image build time, and a provider added later gets
 * "Trying to verify a provider that was not registered at build time:
 * AndroidKeyStore version 1.0" the first time an app calls
 * KeyGenerator.getInstance() - which android-components does on every start,
 * through SecureAbove22Preferences.
 *
 * The name, version and entries are exactly what Context registered before, so
 * nothing an app sees changes.
 */
public class AndroidKeyStoreProvider extends Provider {

	private static final long serialVersionUID = 1L;

	@SuppressWarnings("deprecation") /* the String-version constructor is JDK 9+ */
	public AndroidKeyStoreProvider() {
		super("AndroidKeyStore", 1.0, "Android KeyStore provider");
		put("KeyStore.AndroidKeyStore", "android.security.keystore.AndroidKeyStore");
		put("KeyGenerator.AES", "android.security.keystore.KeyGenerator$AES");
		put("KeyGenerator.HmacSHA512", "android.security.keystore.KeyGenerator$HmacSHA512");
	}
}
