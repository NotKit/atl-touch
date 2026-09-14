package android.security.keystore;

import java.security.InvalidAlgorithmParameterException;
import java.security.KeyPair;
import java.security.KeyPairGeneratorSpi;
import java.security.NoSuchAlgorithmException;
import java.security.Provider;
import java.security.SecureRandom;
import java.security.Security;
import java.security.spec.AlgorithmParameterSpec;

/*
 * KeyPairGenerator.getInstance(alg, "AndroidKeyStore").
 *
 * On a device the key lives in the secure element and never leaves it; here the
 * runtime's own generator makes an ordinary key pair and the private half is
 * remembered under the KeyGenParameterSpec alias, exactly as
 * KeyGenerator does for secret keys. Nothing is attested and nothing is
 * hardware-backed, so an app that verifies an attestation chain will find none.
 */
public abstract class KeyPairGenerator extends KeyPairGeneratorSpi {

	private java.security.KeyPairGenerator generator;
	private String alias;

	protected abstract String algorithm();

	@Override
	public void initialize(int keysize, SecureRandom random) {
		generator().initialize(keysize, random);
	}

	@Override
	public void initialize(AlgorithmParameterSpec params, SecureRandom random)
	    throws InvalidAlgorithmParameterException {
		if (!(params instanceof KeyGenParameterSpec)) {
			generator().initialize(params, random);
			return;
		}
		KeyGenParameterSpec spec = (KeyGenParameterSpec)params;
		alias = spec.getKeystoreAlias();
		if (spec.getAlgorithmParameterSpec() != null)
			generator().initialize(spec.getAlgorithmParameterSpec(), random);
		else if (spec.getKeySize() > 0)
			generator().initialize(spec.getKeySize(), random);
	}

	@Override
	public KeyPair generateKeyPair() {
		KeyPair pair = generator().generateKeyPair();

		if (alias != null)
			AndroidKeyStore.map.put(alias, pair.getPrivate());
		return pair;
	}

	/* Any provider but this one: getInstance(alg) alone could pick
	 * "AndroidKeyStore" back up and recurse. */
	private java.security.KeyPairGenerator generator() {
		if (generator != null)
			return generator;
		for (Provider provider : Security.getProviders()) {
			if ("AndroidKeyStore".equals(provider.getName()))
				continue;
			try {
				generator = java.security.KeyPairGenerator.getInstance(algorithm(), provider);
				return generator;
			} catch (NoSuchAlgorithmException e) {
				/* try the next provider */
			}
		}
		throw new UnsupportedOperationException("no " + algorithm() + " key pair generator in this runtime");
	}

	public static class EC extends KeyPairGenerator {
		@Override
		protected String algorithm() { return "EC"; }
	}

	public static class RSA extends KeyPairGenerator {
		@Override
		protected String algorithm() { return "RSA"; }
	}
}
