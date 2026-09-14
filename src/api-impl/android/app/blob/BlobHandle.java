package android.app.blob;

/**
 * The identity of a blob in the shared blob store: its SHA-256 digest plus a
 * label, an expiry and a tag. Two handles that name the same bytes are equal,
 * which is what a caller uses them for.
 */
public final class BlobHandle {

	private final byte[] sha256Digest;
	private final CharSequence label;
	private final long expiryTimeMillis;
	private final String tag;

	private BlobHandle(byte[] sha256Digest, CharSequence label, long expiryTimeMillis, String tag) {
		this.sha256Digest = sha256Digest.clone();
		this.label = label;
		this.expiryTimeMillis = expiryTimeMillis;
		this.tag = tag;
	}

	public static BlobHandle createWithSha256(byte[] digest, CharSequence label,
	    long expiryTimeMillis, String tag) {
		if (digest == null || digest.length != 32)
			throw new IllegalArgumentException("a blob handle needs a 32 byte SHA-256 digest");
		if (label == null || tag == null)
			throw new NullPointerException("a blob handle needs a label and a tag");
		return new BlobHandle(digest, label, expiryTimeMillis, tag);
	}

	public byte[] getSha256Digest() {
		return sha256Digest.clone();
	}

	public CharSequence getLabel() {
		return label;
	}

	public long getExpiryTimeMillis() {
		return expiryTimeMillis;
	}

	public String getTag() {
		return tag;
	}

	/** the file name this blob is stored under */
	String fileName() {
		StringBuilder name = new StringBuilder(sha256Digest.length * 2);

		for (byte b : sha256Digest)
			name.append(Character.forDigit((b >> 4) & 0xf, 16)).append(Character.forDigit(b & 0xf, 16));
		return name.toString();
	}

	@Override
	public boolean equals(Object other) {
		if (!(other instanceof BlobHandle))
			return false;
		BlobHandle o = (BlobHandle) other;
		return java.util.Arrays.equals(sha256Digest, o.sha256Digest) && tag.equals(o.tag)
		    && expiryTimeMillis == o.expiryTimeMillis && label.toString().equals(o.label.toString());
	}

	@Override
	public int hashCode() {
		return java.util.Arrays.hashCode(sha256Digest) * 31 + tag.hashCode();
	}

	@Override
	public String toString() {
		return "BlobHandle{" + fileName() + " tag=" + tag + "}";
	}
}
