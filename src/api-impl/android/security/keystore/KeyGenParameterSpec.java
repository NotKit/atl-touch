package android.security.keystore;

import java.security.spec.AlgorithmParameterSpec;
import java.util.Date;

/* the SDK's class implements AlgorithmParameterSpec; without it
 * javax.crypto.KeyGenerator.init(spec) fails the cast at its first call,
 * which no class-loading sweep can see */
public class KeyGenParameterSpec implements AlgorithmParameterSpec {

	private String keystoreAlias;
	private int purposes;
	private int keySize;
	private String[] blockModes;
	private String[] encryptionPaddings;
	private String[] digests;
	private boolean userAuthenticationRequired;
	private AlgorithmParameterSpec algorithmParameterSpec;
	private byte[] attestationChallenge;
	private Date certificateNotBefore;
	private Date certificateNotAfter;
	private boolean strongBoxBacked;
	private boolean devicePropertiesAttestationIncluded;

	public static class Builder {
		private KeyGenParameterSpec spec = new KeyGenParameterSpec();

		public Builder(String keystoreAlias, int purposes) {
			spec.keystoreAlias = keystoreAlias;
			spec.purposes = purposes;
		}

		public Builder setKeySize(int keySize) {
			spec.keySize = keySize;
			return this;
		}

		public Builder setBlockModes(String[] blockModes) {
			spec.blockModes = blockModes;
			return this;
		}

		public Builder setEncryptionPaddings(String[] encryptionPaddings) {
			spec.encryptionPaddings = encryptionPaddings;
			return this;
		}

		public Builder setUserAuthenticationRequired(boolean userAuthenticationRequired) {
			spec.userAuthenticationRequired = userAuthenticationRequired;
			return this;
		}

		public Builder setDigests(String... digests) {
			spec.digests = digests;
			return this;
		}

		public Builder setAlgorithmParameterSpec(AlgorithmParameterSpec params) {
			spec.algorithmParameterSpec = params;
			return this;
		}

		public Builder setAttestationChallenge(byte[] challenge) {
			spec.attestationChallenge = challenge;
			return this;
		}

		public Builder setCertificateNotBefore(Date date) {
			spec.certificateNotBefore = date;
			return this;
		}

		public Builder setCertificateNotAfter(Date date) {
			spec.certificateNotAfter = date;
			return this;
		}

		/* No secure element and no device attestation here; the setters exist so
		 * that a builder chain runs, and the flags read back as they were set. */
		public Builder setIsStrongBoxBacked(boolean strongBox) {
			spec.strongBoxBacked = strongBox;
			return this;
		}

		public Builder setDevicePropertiesAttestationIncluded(boolean included) {
			spec.devicePropertiesAttestationIncluded = included;
			return this;
		}

		public KeyGenParameterSpec build() {
			return spec;
		}
	}

	public int getKeySize() {
		return keySize;
	}

	public String[] getBlockModes() {
		return blockModes;
	}

	public int getPurposes() {
		return purposes;
	}

	public String[] getEncryptionPaddings() {
		return encryptionPaddings;
	}

	public boolean isUserAuthenticationRequired() {
		return userAuthenticationRequired;
	}

	public String getKeystoreAlias() {
		return keystoreAlias;
	}

	public String[] getDigests() {
		return digests;
	}

	public AlgorithmParameterSpec getAlgorithmParameterSpec() {
		return algorithmParameterSpec;
	}

	public byte[] getAttestationChallenge() {
		return attestationChallenge;
	}

	public Date getCertificateNotBefore() {
		return certificateNotBefore;
	}

	public Date getCertificateNotAfter() {
		return certificateNotAfter;
	}

	public boolean isStrongBoxBacked() {
		return strongBoxBacked;
	}

	public boolean isDevicePropertiesAttestationIncluded() {
		return devicePropertiesAttestationIncluded;
	}
}
