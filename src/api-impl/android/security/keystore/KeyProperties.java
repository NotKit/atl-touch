package android.security.keystore;

/** The names and flags a KeyGenParameterSpec is built from; values match AOSP. */
public abstract class KeyProperties {

	private KeyProperties() {}

	public static final int PURPOSE_ENCRYPT = 1 << 0;
	public static final int PURPOSE_DECRYPT = 1 << 1;
	public static final int PURPOSE_SIGN = 1 << 2;
	public static final int PURPOSE_VERIFY = 1 << 3;
	public static final int PURPOSE_WRAP_KEY = 1 << 5;
	public static final int PURPOSE_AGREE_KEY = 1 << 6;
	public static final int PURPOSE_ATTEST_KEY = 1 << 7;

	public static final String KEY_ALGORITHM_RSA = "RSA";
	public static final String KEY_ALGORITHM_EC = "EC";
	public static final String KEY_ALGORITHM_AES = "AES";
	public static final String KEY_ALGORITHM_HMAC_SHA1 = "HmacSHA1";
	public static final String KEY_ALGORITHM_HMAC_SHA224 = "HmacSHA224";
	public static final String KEY_ALGORITHM_HMAC_SHA256 = "HmacSHA256";
	public static final String KEY_ALGORITHM_HMAC_SHA384 = "HmacSHA384";
	public static final String KEY_ALGORITHM_HMAC_SHA512 = "HmacSHA512";

	public static final String BLOCK_MODE_ECB = "ECB";
	public static final String BLOCK_MODE_CBC = "CBC";
	public static final String BLOCK_MODE_CTR = "CTR";
	public static final String BLOCK_MODE_GCM = "GCM";

	public static final String ENCRYPTION_PADDING_NONE = "NoPadding";
	public static final String ENCRYPTION_PADDING_PKCS7 = "PKCS7Padding";
	public static final String ENCRYPTION_PADDING_RSA_PKCS1 = "PKCS1Padding";
	public static final String ENCRYPTION_PADDING_RSA_OAEP = "OAEPPadding";

	public static final String SIGNATURE_PADDING_RSA_PKCS1 = "PKCS1";
	public static final String SIGNATURE_PADDING_RSA_PSS = "PSS";

	public static final String DIGEST_NONE = "NONE";
	public static final String DIGEST_MD5 = "MD5";
	public static final String DIGEST_SHA1 = "SHA-1";
	public static final String DIGEST_SHA224 = "SHA-224";
	public static final String DIGEST_SHA256 = "SHA-256";
	public static final String DIGEST_SHA384 = "SHA-384";
	public static final String DIGEST_SHA512 = "SHA-512";

	public static final int ORIGIN_GENERATED = 1 << 0;
	public static final int ORIGIN_IMPORTED = 1 << 1;
	public static final int ORIGIN_UNKNOWN = 1 << 2;
	public static final int ORIGIN_SECURELY_IMPORTED = 1 << 3;

	public static final int AUTH_DEVICE_CREDENTIAL = 1 << 0;
	public static final int AUTH_BIOMETRIC_STRONG = 1 << 1;

	public static final int SECURITY_LEVEL_UNKNOWN = -2;
	public static final int SECURITY_LEVEL_UNKNOWN_SECURE = -1;
	public static final int SECURITY_LEVEL_SOFTWARE = 0;
	public static final int SECURITY_LEVEL_TRUSTED_ENVIRONMENT = 1;
	public static final int SECURITY_LEVEL_STRONGBOX = 2;
}
