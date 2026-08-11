package android.view.inputmethod;

import android.os.Bundle;
import android.os.LocaleList;

public class EditorInfo {
	public int actionId = 0;
	public CharSequence actionLabel = null;
	public Bundle extras = null;
	public int fieldId = 0;
	public String fieldName = null;
	public CharSequence hintText = null;
	public int imeOptions = 0x0;
	public int initialCapsMode = 0;
	public int initialSelStart = -1;
	public int initialSelEnd = -1;
	public int inputType = /*0x0*/ 0x00000001; /* TYPE_NULL */ /* TYPE_CLASS_TEXT */
	public CharSequence label = null;
	public String packageName = "com.example.FIXME";
	public String privateImeOptions = null;
	public LocaleList hintLocales = null;

	public java.lang.String[] contentMimeTypes;

	public static final int IME_ACTION_DONE = 6;

	public static final int IME_ACTION_GO = 2;

	public static final int IME_ACTION_NEXT = 5;

	public static final int IME_ACTION_NONE = 1;

	public static final int IME_ACTION_PREVIOUS = 7;

	public static final int IME_ACTION_SEARCH = 3;

	public static final int IME_ACTION_SEND = 4;

	public static final int IME_FLAG_NO_EXTRACT_UI = 268435456;

	public static final int IME_FLAG_NO_FULLSCREEN = 33554432;

	public static final int IME_FLAG_NO_PERSONALIZED_LEARNING = 16777216;

	public void setInitialSurroundingText(java.lang.CharSequence a0) { }
}
