package android.text.method;

public abstract class MetaKeyKeyListener {
	public static final int META_ALT_LOCKED = 512;
	public static final int META_ALT_ON = 2;
	public static final int META_CAP_LOCKED = 256;
	public static final int META_SHIFT_ON = 1;
	public static final int META_SYM_LOCKED = 1024;
	public static final int META_SYM_ON = 4;

	public static long adjustMetaAfterKeypress(long a0) { return 0L; }

	public static void adjustMetaAfterKeypress(android.text.Spannable a0) { }
}
