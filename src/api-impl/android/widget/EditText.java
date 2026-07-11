package android.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.text.Editable;
import android.text.Layout;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
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
	}

	// --- editing primitives ---

	private void setContent(String next, int caret) {
		fireBeforeTextChanged(content(), next);
		super.setText(next, BufferType.NORMAL);
		selStart = selEnd = Math.max(0, Math.min(caret, next.length()));
		fireTextChanged(next);
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

	private void moveCaret(int delta) {
		int len = content().length();
		selStart = selEnd = Math.max(0, Math.min(selEnd + delta, len));
		invalidate();
	}

	private void setCaret(int pos) {
		int len = content().length();
		selStart = selEnd = Math.max(0, Math.min(pos, len));
		invalidate();
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

	@Override
	public boolean onComposingText(CharSequence text) {
		String cur = content();
		int s, e;
		if (composeStart >= 0) {
			s = Math.max(0, Math.min(composeStart, cur.length()));
			e = Math.max(0, Math.min(composeEnd, cur.length()));
		} else {
			s = Math.max(0, Math.min(Math.min(selStart, selEnd), cur.length()));
			e = Math.max(0, Math.min(Math.max(selStart, selEnd), cur.length()));
		}
		String ins = text == null ? "" : text.toString();
		composeStart = s;
		composeEnd = s + ins.length();
		setContent(cur.substring(0, s) + ins + cur.substring(e), composeEnd);
		return true;
	}

	@Override
	public boolean onCommitText(CharSequence text) {
		String cur = content();
		int s, e;
		if (composeStart >= 0) {
			s = Math.max(0, Math.min(composeStart, cur.length()));
			e = Math.max(0, Math.min(composeEnd, cur.length()));
		} else {
			s = Math.max(0, Math.min(Math.min(selStart, selEnd), cur.length()));
			e = Math.max(0, Math.min(Math.max(selStart, selEnd), cur.length()));
		}
		String ins = text == null ? "" : text.toString();
		composeStart = composeEnd = -1;
		setContent(cur.substring(0, s) + ins + cur.substring(e), s + ins.length());
		return true;
	}

	@Override
	public void onFinishComposing() {
		composeStart = composeEnd = -1;
		invalidate();
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
			setCaret(caretFromX(event.getX()));
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
		return new BaseInputConnection(this, true);
	}

	/** Map a touch x (view coords) to the nearest character boundary. */
	private int caretFromX(float x) {
		String cur = content();
		float local = x - paddingLeft;
		if (local <= 0)
			return 0;
		TextPaint p = getPaint();
		for (int i = 1; i <= cur.length(); i++) {
			if (Layout.getDesiredWidth(cur.subSequence(0, i), p) >= local) {
				float prev = Layout.getDesiredWidth(cur.subSequence(0, i - 1), p);
				float curw = Layout.getDesiredWidth(cur.subSequence(0, i), p);
				return (local - prev) < (curw - local) ? i - 1 : i;
			}
		}
		return cur.length();
	}

	@Override
	public void onDraw(Canvas canvas) {
		super.onDraw(canvas);
		if (!isFocused())
			return;
		String cur = content();
		int caret = Math.max(0, Math.min(selEnd, cur.length()));
		float cursorX = paddingLeft + Layout.getDesiredWidth(cur.subSequence(0, caret), getPaint());
		float lineHeight = getPaint().getTextSize() * 1.2f;
		int innerHeight = getHeight() - paddingTop - paddingBottom;
		float top = paddingTop + Math.max(0, (innerHeight - lineHeight) / 2f);
		if (composeStart >= 0 && composeEnd > composeStart && composeEnd <= cur.length()) {
			float x0 = paddingLeft + Layout.getDesiredWidth(cur.subSequence(0, composeStart), getPaint());
			float x1 = paddingLeft + Layout.getDesiredWidth(cur.subSequence(0, composeEnd), getPaint());
			float baseline = top + lineHeight - getPaint().descent();
			canvas.drawLine(x0, baseline + 2, x1, baseline + 2, getPaint());
		}
		canvas.drawLine(cursorX, top, cursorX, top + lineHeight, cursorPaint);
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
		invalidate();
	}

	public void setSelection(int start, int stop) {
		int len = content().length();
		selStart = Math.max(0, Math.min(start, len));
		selEnd = Math.max(0, Math.min(stop, len));
		invalidate();
	}

	public void setSelection(int index) {
		setSelection(index, index);
	}
}
