package android.widget;

import android.content.Context;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.text.Editable;
import android.text.Layout;
import android.text.SpannableStringBuilder;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.view.MotionEvent;
import java.util.ArrayList;

public class EditText extends TextView {
	/** Caret position; selStart==selEnd means a plain cursor (no range selection). */
	private int selStart = 0, selEnd = 0;
	/** Active IME composing region; composeStart==-1 means none. */
	private int composeStart = -1, composeEnd = -1;
	private final ArrayList<TextWatcher> watchers = new ArrayList<>();
	private OnEditorActionListener editorActionListener;
	private final Paint cursorPaint = new Paint();
	private boolean contextMenuLongPress;
	private PopupWindow textContextMenu;

	public EditText(Context context) {
		this(context, null);
	}

	public EditText(Context context, AttributeSet attrs) {
		this(context, attrs, com.android.internal.R.attr.editTextStyle);
	}

	public EditText(Context context, AttributeSet attrs, int defStyle) {
		this(context, attrs, defStyle, 0);
	}

	public EditText(Context context, AttributeSet attrs, int defStyle, int defStyleRes) {
		super(context, attrs, defStyle, defStyleRes);
		cursorPaint.setColor(0xFF000000);
		setLongClickable(true);
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
		dismissTextContextMenu();
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
		dismissTextContextMenu();
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
		dismissTextContextMenu();
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
		if (event.isCtrlPressed() && !event.isAltPressed()) {
			switch (keyCode) {
				case KeyEvent.KEYCODE_A: return onTextContextMenuItem(android.R.id.selectAll);
				case KeyEvent.KEYCODE_C: return onTextContextMenuItem(android.R.id.copy);
				case KeyEvent.KEYCODE_X: return onTextContextMenuItem(android.R.id.cut);
				case KeyEvent.KEYCODE_V: return onTextContextMenuItem(android.R.id.paste);
			}
		}
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

	public boolean onTextContextMenuItem(int id) {
		if (id == android.R.id.selectAll) {
			selectAll();
			return true;
		}
		ClipboardManager clipboard = (ClipboardManager)getContext().getSystemService(Context.CLIPBOARD_SERVICE);
		if (id == android.R.id.copy || id == android.R.id.cut) {
			int start = Math.min(selStart, selEnd), end = Math.max(selStart, selEnd);
			if (start == end)
				return false;
			clipboard.setPrimaryClip(ClipData.newPlainText(null, content().substring(start, end)));
			if (id == android.R.id.cut) {
				onFinishComposing();
				replaceSelection("");
			}
			return true;
		}
		if (id == android.R.id.paste || id == android.R.id.pasteAsPlainText) {
			ClipData clip = clipboard.getPrimaryClip();
			if (clip == null || clip.getItemCount() == 0)
				return false;
			CharSequence text = clip.getItemAt(0).coerceToText(getContext());
			if (text == null)
				return false;
			onFinishComposing();
			replaceSelection(text.toString());
			return true;
		}
		return false;
	}

	@Override
	public boolean performLongClick(float x, float y) {
		contextMenuLongPress = super.performLongClick(x, y) || showContextMenu();
		return contextMenuLongPress;
	}

	@Override
	public boolean showContextMenu(float x, float y) {
		return showContextMenu();
	}

	@Override
	public boolean showContextMenu() {
		dismissTextContextMenu();
		if (!isAttachedToWindow())
			return false;
		ArrayList<CharSequence> labels = new ArrayList<>();
		ArrayList<Integer> actions = new ArrayList<>();
		if (selStart != selEnd) {
			labels.add(getContext().getText(android.R.string.copy));
			actions.add(android.R.id.copy);
			labels.add(getContext().getText(android.R.string.cut));
			actions.add(android.R.id.cut);
		}
		ClipboardManager clipboard = (ClipboardManager)getContext().getSystemService(Context.CLIPBOARD_SERVICE);
		if (clipboard.hasPrimaryClip()) {
			labels.add(getContext().getText(android.R.string.paste));
			actions.add(android.R.id.paste);
		}
		if (!content().isEmpty()) {
			labels.add(getContext().getText(android.R.string.selectAll));
			actions.add(android.R.id.selectAll);
		}
		if (actions.isEmpty())
			return false;
		requestFocus();
		int color = getCurrentTextColor();
		boolean dark = ((color >> 16) & 255) + ((color >> 8) & 255) + (color & 255) > 384;
		LinearLayout toolbar = new LinearLayout(getContext());
		toolbar.setOrientation(LinearLayout.HORIZONTAL);
		toolbar.setPadding(menuDp(4), 0, menuDp(4), 0);
		for (int i = 0; i < actions.size(); i++) {
			final int action = actions.get(i);
			TextView item = new TextView(getContext());
			item.setText(labels.get(i));
			item.setTextSize(14);
			item.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
			item.setTextColor(dark ? 0xfff5f5f5 : 0xff212121);
			item.setGravity(Gravity.CENTER);
			item.setPadding(menuDp(16), 0, menuDp(16), 0);
			StateListDrawable feedback = new StateListDrawable();
			feedback.addState(new int[] {android.R.attr.state_pressed},
			    menuBackground(dark ? 0xff394550 : 0xffeeeeee));
			item.setBackground(feedback);
			item.setOnClickListener(v -> {
				dismissTextContextMenu();
				onTextContextMenuItem(action);
				if (action == android.R.id.selectAll)
					post(() -> showContextMenu());
			});
			toolbar.addView(item, new LinearLayout.LayoutParams(
			    ViewGroup.LayoutParams.WRAP_CONTENT, menuDp(48)));
		}
		Rect frame = new Rect(), editor = new Rect();
		getWindowVisibleDisplayFrame(frame);
		getGlobalVisibleRect(editor);
		toolbar.measure(MeasureSpec.makeMeasureSpec(Math.max(0, frame.width() - menuDp(16)), MeasureSpec.AT_MOST),
		    MeasureSpec.makeMeasureSpec(menuDp(48), MeasureSpec.EXACTLY));
		Layout layout = getLayout();
		int caret = clamp(Math.min(selStart, selEnd), content().length());
		int line = layout.getLineForOffset(caret);
		int x = editor.left + (int)(getLayoutOffsetX() + layout.getPrimaryHorizontal(caret))
		    - getScrollX() - toolbar.getMeasuredWidth() / 2;
		int top = editor.top + (int)getLayoutOffsetY() + layout.getLineTop(line) - getScrollY();
		int y = top - toolbar.getMeasuredHeight() - menuDp(8);
		if (y < frame.top + menuDp(8))
			y = top + layout.getLineBottom(line) - layout.getLineTop(line) + menuDp(8);
		x = Math.max(frame.left + menuDp(8), Math.min(x, frame.right - toolbar.getMeasuredWidth() - menuDp(8)));
		y = Math.max(frame.top + menuDp(8), Math.min(y, frame.bottom - toolbar.getMeasuredHeight() - menuDp(8)));
		// A non-focusable panel keeps the editor and its keyboard active.
		PopupWindow menu = new PopupWindow(toolbar, toolbar.getMeasuredWidth(), toolbar.getMeasuredHeight(), false);
		textContextMenu = menu;
		menu.setBackgroundDrawable(menuBackground(dark ? 0xff1d2935 : 0xffffffff));
		menu.setElevation(menuDp(6));
		menu.showAtLocation(this, Gravity.TOP | Gravity.LEFT, x, y);
		return menu.isShowing();
	}

	private int menuDp(int value) {
		return Math.round(value * getResources().getDisplayMetrics().density);
	}

	private GradientDrawable menuBackground(int color) {
		GradientDrawable background = new GradientDrawable();
		background.setColor(color);
		background.setCornerRadius(menuDp(12));
		return background;
	}

	private void dismissTextContextMenu() {
		if (textContextMenu != null) {
			textContextMenu.dismiss();
			textContextMenu = null;
		}
	}

	@Override
	protected void onDetachedFromWindow() {
		dismissTextContextMenu();
		super.onDetachedFromWindow();
	}

	@Override
	protected void onFocusChanged(boolean gainFocus, int direction, Rect previous) {
		if (!gainFocus)
			dismissTextContextMenu();
		super.onFocusChanged(gainFocus, direction, previous);
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
		if (event.getAction() == MotionEvent.ACTION_DOWN)
			contextMenuLongPress = false;
		if (event.getAction() == MotionEvent.ACTION_UP && !contextMenuLongPress) {
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
	protected void drawEditor(Canvas canvas) {
		if (!isFocused())
			return;
		// TextView has already translated to the text layout's origin.
		String cur = content();
		Layout layout = getLayout();
		int caret = Math.max(0, Math.min(selEnd, cur.length()));
		int caretLine = layout.getLineForOffset(caret);
		if (composeStart >= 0 && composeEnd > composeStart && composeEnd <= cur.length()) {
			int line = layout.getLineForOffset(composeStart);
			float x0 = layout.getPrimaryHorizontal(composeStart);
			float x1 = layout.getPrimaryHorizontal(composeEnd);
			float y = layout.getLineBaseline(line) + getPaint().getUnderlinePosition();
			canvas.drawLine(x0, y, x1, y, getPaint());
		}
		if (!isCursorVisible() || selStart != selEnd)
			return;
		float cursorX = layout.getPrimaryHorizontal(caret);
		float top = layout.getLineTop(caretLine), bottom = layout.getLineBottomWithoutSpacing(caretLine);
		Drawable cursor = getTextCursorDrawable();
		if (cursor == null) {
			canvas.drawLine(cursorX, top, cursorX, bottom, cursorPaint);
			return;
		}
		/* the drawable is the caret (API 29+), so it decides its own width; an
		 * intrinsic-less one gets the hairline the fallback above would draw */
		int width = Math.max(1, cursor.getIntrinsicWidth());
		Rect padding = new Rect();
		cursor.getPadding(padding);
		float horizontal = Math.max(0.5f, cursorX - 0.5f);
		int scrollX = getScrollX();
		int available = getWidth() - getCompoundPaddingLeft() - getCompoundPaddingRight();
		int left;
		if (horizontal - scrollX >= available - 1f)
			left = available + scrollX - (width - padding.right);
		else if (Math.abs(horizontal - scrollX) <= 1f)
			left = scrollX - padding.left;
		else
			left = (int)horizontal - padding.left;
		// Drawable bounds stay in layout coordinates, as in AOSP's Editor.
		cursor.setBounds(left, (int)top - padding.top, left + width, (int)bottom + padding.bottom);
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
