package android.text.method;

import android.text.Editable;
import android.view.KeyEvent;

public class TextKeyListener extends MetaKeyKeyListener implements KeyListener {

	/* inherited from MetaKeyKeyListener in AOSP */
	public static final int META_SHIFT_ON = KeyEvent.META_SHIFT_ON;
	public static final int META_ALT_ON = KeyEvent.META_ALT_ON;
	public static final int META_SELECTING = 0x800 /*KeyEvent.META_SELECTING*/;

	public static void clear(Editable content) {}

	private static TextKeyListener instance;

	/* AOSP hands out a singleton here; returning null made every caller that
	 * types through a key listener crash. The inherited KeyListener methods
	 * report "not handled", which leaves the key to the caller's own
	 * handling - enough for GeckoView, whose editable takes it from there. */
	public static android.text.method.TextKeyListener getInstance() {
		if (instance == null)
			instance = new TextKeyListener();
		return instance;
	}
}
