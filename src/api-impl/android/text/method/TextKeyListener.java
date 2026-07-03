package android.text.method;

import android.text.Editable;
import android.view.KeyEvent;

public class TextKeyListener {

	/* inherited from MetaKeyKeyListener in AOSP */
	public static final int META_SHIFT_ON = KeyEvent.META_SHIFT_ON;
	public static final int META_ALT_ON = KeyEvent.META_ALT_ON;
	public static final int META_SELECTING = 0x800 /*KeyEvent.META_SELECTING*/;

	public static void clear(Editable content) {}

	public static int getMetaState(CharSequence text, int meta) {
		return 0;
	}
}
