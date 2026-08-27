package android.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.text.Editable;
import android.text.Layout;
import android.text.SpannableStringBuilder;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.view.MotionEvent;
import java.util.ArrayList;

public class EditText extends TextView {
	private CharSequence hint = "";
	/** Caret position; selStart==selEnd means a plain cursor (no range selection). */
	private int selStart = 0, selEnd = 0;
	/** Active IME composing region; composeStart==-1 means none. */
	private int composeStart = -1, composeEnd = -1;
	private final ArrayList<TextWatcher> watchers = new ArrayList<>();
	private OnEditorActionListener editorActionListener;
	private final Paint cursorPaint = new Paint();

	public EditText(Context context) {
		super(context);
		initDefaultSize();
	}

	public EditText(Context context, AttributeSet attrs) {
		super(context, attrs);
		initDefaultSize();
	}

	public EditText(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
		initDefaultSize();
	}

	public EditText(Context context, AttributeSet attrs, int defStyle, int defStyleRes) {
		super(context, attrs, defStyle, defStyleRes);
		initDefaultSize();
	}

	/**
	 * Give the field a touch-target-sized minimum height and vertically center its
	 * content. ATL doesn't resolve the editbox/OutlinedBox style padding that would
	 * normally supply this, so without a floor a single-line field collapses to the
	 * bare text height (~23px) and is effectively invisible/unusable. Only applied
	 * when nothing more specific was set.
	 */
	private void initDefaultSize() {
		if (getMinimumHeight() <= 0)
			setMinimumHeight((int)(48 * getResources().getDisplayMetrics().density));
		setGravity(Gravity.CENTER_VERTICAL | Gravity.START);
		cursorPaint.setColor(0xFF000000);
	}

	/** The current editable content as a plain String. */
	private String content() {
		CharSequence t = super.getText();
		return t == null ? "" : t.toString();
	}

	public Editable getText() {
		return new SpannableStringBuilder(content());
	}

	public Editable getEditableText() {
		// Must NOT call getText(): AppCompat/TextInputEditText overrides getText()
		// to call super.getEditableText(), which would recurse infinitely.
		return new SpannableStringBuilder(content());
	}

	@Override
	public void setText(CharSequence text, BufferType type) {
		String next = text == null ? "" : text.toString();
		// watchers is null while TextView's constructor runs setText (field
		// initializers haven't executed yet); skip notifications then.
		if (watchers != null && !watchers.isEmpty()) {
			fireBeforeTextChanged(content(), next);
			super.setText(next, type);
			fireTextChanged(next);
		} else {
			super.setText(next, type);
		}
		selStart = selEnd = next.length();
		// The app replaced the content (a sent message clearing the field, a
		// draft being restored): the composing region and everything the input
		// method still holds refer to text that no longer exists.
		composeStart = composeEnd = -1;
		InputMethodManager.onEditorContentReplaced(this);
	}

	// --- editing primitives ---

	private void setContent(String next, int caret) {
		fireBeforeTextChanged(content(), next);
		super.setText(next, BufferType.NORMAL);
		selStart = selEnd = Math.max(0, Math.min(caret, next.length()));
		fireTextChanged(next);
		InputMethodManager.onEditorStateChanged(this);
		invalidate();
	}

	private void replaceSelection(String insert) {
		String cur = content();
		int s = Math.max(0, Math.min(Math.min(selStart, selEnd), cur.length()));
		int e = Math.max(0, Math.min(Math.max(selStart, selEnd), cur.length()));
		setContent(cur.substring(0, s) + insert + cur.substring(e), s + insert.length());
	}

	private void deleteBackward() {
		String cur = content();
		int s = Math.min(selStart, selEnd), e = Math.max(selStart, selEnd);
		s = Math.max(0, Math.min(s, cur.length()));
		e = Math.max(0, Math.min(e, cur.length()));
		if (s != e) {
			setContent(cur.substring(0, s) + cur.substring(e), s);
		} else if (s > 0) {
			setContent(cur.substring(0, s - 1) + cur.substring(s), s - 1);
		}
	}

	private void deleteForward() {
		String cur = content();
		int s = Math.min(selStart, selEnd), e = Math.max(selStart, selEnd);
		s = Math.max(0, Math.min(s, cur.length()));
		e = Math.max(0, Math.min(e, cur.length()));
		if (s != e) {
			setContent(cur.substring(0, s) + cur.substring(e), s);
		} else if (s < cur.length()) {
			setContent(cur.substring(0, s) + cur.substring(s + 1), s);
		}
	}

	/**
	 * A caret move the input method did not cause invalidates its composing
	 * region: the next preedit update would splice into where the word used to
	 * be instead of at the caret. Drop the region and tell the IME.
	 */
	private void caretMoved() {
		if (composeStart >= 0) {
			composeStart = composeEnd = -1;
			InputMethodManager.onEditorCaretMoved(this);
		} else {
			InputMethodManager.onEditorStateChanged(this);
		}
		invalidate();
	}

	private void moveCaret(int delta) {
		int len = content().length();
		selStart = selEnd = Math.max(0, Math.min(selEnd + delta, len));
		caretMoved();
	}

	private void setCaret(int pos) {
		int len = content().length();
		selStart = selEnd = Math.max(0, Math.min(pos, len));
		caretMoved();
	}

	// --- key / touch input ---

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		switch (keyCode) {
			case KeyEvent.KEYCODE_DEL:
				deleteBackward();
				return true;
			case KeyEvent.KEYCODE_FORWARD_DEL:
				deleteForward();
				return true;
			case KeyEvent.KEYCODE_DPAD_LEFT:
				moveCaret(-1);
				return true;
			case KeyEvent.KEYCODE_DPAD_RIGHT:
				moveCaret(1);
				return true;
			case KeyEvent.KEYCODE_MOVE_HOME:
				setCaret(0);
				return true;
			case KeyEvent.KEYCODE_MOVE_END:
				setCaret(content().length());
				return true;
			case KeyEvent.KEYCODE_ENTER:
				if (editorActionListener != null)
					editorActionListener.onEditorAction(this, 6 /* EditorInfo.IME_ACTION_DONE */, event);
				return true; // don't insert a newline into a single-line field
		}
		// Printable characters arrive via onTextInput() (the GLFW char callback),
		// which resolves the OS keyboard layout (Cyrillic, dead keys, ...). Don't
		// insert from the keycode here or every character would be entered twice.
		return super.onKeyDown(keyCode, event);
	}

	/**
	 * Take the composing region (and, if the IME asked for one, a replacement
	 * range around it) out of the text and return where the new text goes.
	 * Result: {content without those ranges, insertion offset}.
	 */
	private Object[] removeComposingAndReplacement(int replaceStart, int replaceLength) {
		String cur = content();
		int at, end;
		if (composeStart >= 0) {
			at = clamp(composeStart, cur.length());
			end = clamp(composeEnd, cur.length());
		} else {
			at = clamp(Math.min(selStart, selEnd), cur.length());
			// with no composing region, a plain commit replaces the selection
			end = replaceLength > 0 ? at : clamp(Math.max(selStart, selEnd), cur.length());
		}
		cur = cur.substring(0, at) + cur.substring(end);
		// replaceStart counts from the start of the composing region, i.e. from
		// where the composing text just was, and is normally negative (replace
		// the word just before the cursor). A range that starts before the text
		// is dropped rather than clamped, which would delete from the start of
		// the field instead — the reference guards it the same way.
		if (replaceLength > 0 && at + replaceStart >= 0) {
			int from = clamp(at + replaceStart, cur.length());
			int to = clamp(from + replaceLength, cur.length());
			cur = cur.substring(0, from) + cur.substring(to);
			at = from;
		}
		return new Object[] {cur, Integer.valueOf(at)};
	}

	private static int clamp(int v, int len) {
		return Math.max(0, Math.min(v, len));
	}

	@Override
	public boolean onComposingText(CharSequence text, int replaceStart, int replaceLength, int cursorPos) {
		Object[] r = removeComposingAndReplacement(replaceStart, replaceLength);
		String cur = (String)r[0];
		int at = ((Integer)r[1]).intValue();
		String ins = text == null ? "" : text.toString();
		// an empty preedit means there is none, not a zero-length one
		composeStart = ins.isEmpty() ? -1 : at;
		composeEnd = ins.isEmpty() ? -1 : at + ins.length();
		// cursorPos is an offset inside the composing text, -1 meaning its end
		int caret = cursorPos >= 0 ? at + Math.min(cursorPos, ins.length()) : at + ins.length();
		setContent(cur.substring(0, at) + ins + cur.substring(at), caret);
		return true;
	}

	@Override
	public boolean onCommitText(CharSequence text, int replaceStart, int replaceLength, int cursorPos) {
		Object[] r = removeComposingAndReplacement(replaceStart, replaceLength);
		String cur = (String)r[0];
		int at = ((Integer)r[1]).intValue();
		String ins = text == null ? "" : text.toString();
		composeStart = composeEnd = -1;
		int caret = cursorPos >= 0 ? at + cursorPos : at + ins.length();
		setContent(cur.substring(0, at) + ins + cur.substring(at), caret);
		return true;
	}

	@Override
	public void onFinishComposing() {
		if (composeStart < 0)
			return;
		composeStart = composeEnd = -1;
		// the text itself is unchanged, but it is committed text now, so the
		// input method's copy of the surrounding text has to grow by it
		InputMethodManager.onEditorStateChanged(this);
		invalidate();
	}

	@Override
	public void onImeSetSelection(int start, int length) {
		// the input method counts in surrounding-text coordinates, which skip
		// the composing region; this must not go through caretMoved(), the
		// region is still the IME's own
		selStart = fromSurrounding(start);
		selEnd = fromSurrounding(start + length);
		InputMethodManager.onEditorStateChanged(this);
		invalidate();
	}

	/* --- the editor state the input method mirrors ---
	 *
	 * The composing region is the input method's own, unconfirmed text: Qt keeps
	 * it outside the widget's content and maliit's server expects the same, so
	 * report the text without it and put the cursor where it starts. Leaving it
	 * in would make the keyboard see its own preedit as context and type it
	 * twice. */

	@Override
	public CharSequence getImeSurroundingText() {
		String cur = content();
		if (composeStart < 0)
			return cur;
		int s = clamp(composeStart, cur.length()), e = clamp(composeEnd, cur.length());
		return cur.substring(0, s) + cur.substring(e);
	}

	/** Map an offset in the surrounding text back into the content. */
	private int fromSurrounding(int pos) {
		String cur = content();
		if (composeStart < 0)
			return clamp(pos, cur.length());
		int s = clamp(composeStart, cur.length()), e = clamp(composeEnd, cur.length());
		pos = clamp(pos, cur.length() - (e - s));
		return pos <= s ? pos : pos + (e - s);
	}

	/** Map an offset in the content onto the same spot in the surrounding text. */
	private int toSurrounding(int pos) {
		String cur = content();
		if (composeStart < 0)
			return clamp(pos, cur.length());
		int s = clamp(composeStart, cur.length()), e = clamp(composeEnd, cur.length());
		pos = clamp(pos, cur.length());
		if (pos <= s)
			return pos;
		return pos >= e ? pos - (e - s) : s;
	}

	@Override
	public int getImeCursorPosition() {
		return toSurrounding(selEnd);
	}

	@Override
	public int getImeAnchorPosition() {
		return toSurrounding(selStart);
	}

	@Override
	public boolean onTextInput(int codePoint) {
		if (codePoint == 0)
			return false;
		composeStart = composeEnd = -1;
		replaceSelection(new String(Character.toChars(codePoint)));
		return true;
	}

	@Override
	public boolean onCheckIsTextEditor() {
		return true;
	}

	@Override
	public boolean onTouchEvent(MotionEvent event) {
		if (event.getAction() == MotionEvent.ACTION_UP) {
			requestFocus();
			setCaret(caretFromXY(event.getX(), event.getY()));
			InputMethodManager imm = (InputMethodManager)getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
			if (imm != null)
				imm.showSoftInput(this, 0);
		}
		super.onTouchEvent(event);
		return true; // a text field always consumes touches
	}

	@Override
	public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
		outAttrs.inputType = getInputType();
		outAttrs.imeOptions = getImeOptions();
		outAttrs.initialSelStart = getSelectionStart();
		outAttrs.initialSelEnd = getSelectionEnd();
		return new BaseInputConnection(this, true);
	}

	/** Map a touch (view coords) to the nearest character boundary. */
	private int caretFromXY(float x, float y) {
		Layout layout = getLayout();
		int line = layout.getLineForVertical((int)(y - getLayoutOffsetY()));
		return layout.getOffsetForHorizontal(line, x - getLayoutOffsetX());
	}

	@Override
	public void onDraw(Canvas canvas) {
		super.onDraw(canvas);
		if (!isFocused())
			return;
		/* the caret and the composing underline have to follow the text where
		 * TextView actually drew it, gravity and all - not a line box of their own */
		String cur = content();
		Layout layout = getLayout();
		float ox = getLayoutOffsetX(), oy = getLayoutOffsetY();
		int caret = Math.max(0, Math.min(selEnd, cur.length()));
		int caretLine = layout.getLineForOffset(caret);
		if (composeStart >= 0 && composeEnd > composeStart && composeEnd <= cur.length()) {
			int line = layout.getLineForOffset(composeStart);
			float x0 = ox + layout.getPrimaryHorizontal(composeStart);
			float x1 = ox + layout.getPrimaryHorizontal(composeEnd);
			float y = oy + layout.getLineBaseline(line) + getPaint().getUnderlinePosition();
			canvas.drawLine(x0, y, x1, y, getPaint());
		}
		if (!isCursorVisible())
			return;
		float cursorX = ox + layout.getPrimaryHorizontal(caret);
		float top = oy + layout.getLineTop(caretLine), bottom = oy + layout.getLineBottom(caretLine);
		Drawable cursor = getTextCursorDrawable();
		if (cursor == null) {
			canvas.drawLine(cursorX, top, cursorX, bottom, cursorPaint);
			return;
		}
		/* the drawable is the caret (API 29+), so it decides its own width; an
		 * intrinsic-less one gets the hairline the fallback above would draw */
		int width = Math.max(1, cursor.getIntrinsicWidth());
		cursor.setBounds((int)cursorX, (int)top, (int)cursorX + width, (int)bottom);
		cursor.draw(canvas);
	}

	// --- TextWatcher plumbing ---

	@Override
	public void addTextChangedListener(TextWatcher watcher) {
		if (watcher != null && !watchers.contains(watcher))
			watchers.add(watcher);
	}

	@Override
	public void removeTextChangedListener(TextWatcher watcher) {
		watchers.remove(watcher);
	}

	private void fireBeforeTextChanged(String before, String after) {
		for (TextWatcher w : new ArrayList<>(watchers))
			w.beforeTextChanged(before, 0, before.length(), after.length());
	}

	private void fireTextChanged(String after) {
		for (TextWatcher w : new ArrayList<>(watchers))
			w.onTextChanged(after, 0, after.length(), after.length());
		Editable editable = new SpannableStringBuilder(after);
		for (TextWatcher w : new ArrayList<>(watchers))
			w.afterTextChanged(editable);
	}

	// --- misc ---

	@Override
	public void setOnEditorActionListener(OnEditorActionListener l) {
		editorActionListener = l;
	}

	@Override
	public void setCompoundDrawables(Drawable left, Drawable top, Drawable right, Drawable bottom) {}

	@Override
	public void setHint(CharSequence s) {
		hint = s == null ? "" : s.toString();
	}

	@Override
	public CharSequence getHint() {
		return hint;
	}

	public void selectAll() {
		selStart = 0;
		selEnd = content().length();
		caretMoved();
	}

	public void setSelection(int start, int stop) {
		int len = content().length();
		selStart = Math.max(0, Math.min(start, len));
		selEnd = Math.max(0, Math.min(stop, len));
		caretMoved();
	}

	public void setSelection(int index) {
		setSelection(index, index);
	}

	@Override
	public int getSelectionStart() {
		return Math.min(selStart, content().length());
	}

	@Override
	public int getSelectionEnd() {
		return Math.min(selEnd, content().length());
	}
}
