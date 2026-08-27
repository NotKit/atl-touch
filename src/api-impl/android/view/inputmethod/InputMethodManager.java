package android.view.inputmethod;

import android.os.IBinder;
import android.view.View;
import java.util.ArrayList;
import java.util.List;

/**
 * The app half of an input method connection.
 *
 * This is modelled on Maliit's Qt input context (MInputContext), because the
 * only input method transport the mobile shells offer is Maliit's and its
 * server keeps a *copy* of the editor state: the surrounding text, the cursor
 * and the anchor. The server predicts how that copy evolves (it inserts what it
 * commits, deletes on backspace) and the keyboard's word engine reads it back
 * to decide what word is being typed. So every change the app makes to an
 * editor has to be pushed here, or the keyboard keeps working from text that no
 * longer exists — which is how a word that was deleted, or a message that was
 * sent, comes back as the next commit.
 *
 * The state machine is therefore: an editor gains focus -> activate the
 * context; any edit -> push the new state; the editor loses focus, the window
 * loses focus or the app replaces the text under the IME -> reset (drop the
 * server's preedit) and push again.
 */
public class InputMethodManager {

	private static final boolean has_backend = nativeInit();

	/** The editor the input method is currently connected to, if any. */
	private static View activeView;
	/* The editor's declared attributes, taken once when the connection is
	 * established. onCreateInputConnection is not a cheap query — apps build
	 * listeners and MIME-type bundles in it — so it must not run per keystroke;
	 * only the text and the selection change that often. */
	private static int activeInputType;
	private static int activeImeOptions;

	private ArrayList<InputMethodInfo> input_method_list = new ArrayList<InputMethodInfo>();

	// --- state push ------------------------------------------------------

	/** Send the focused editor's state to the input method (updateWidgetInformation). */
	private static void update(View view, boolean focusChanged) {
		if (!has_backend)
			return;
		if (view == null) {
			nativeUpdate(null, 0, 0, 0, 0, false, focusChanged);
			return;
		}
		CharSequence surrounding = view.getImeSurroundingText();
		nativeUpdate(surrounding == null ? "" : surrounding.toString(),
		             view.getImeCursorPosition(), view.getImeAnchorPosition(),
		             activeInputType, activeImeOptions, true, focusChanged);
	}

	/**
	 * Establish (or re-establish) the connection to an editor and push its
	 * state. False if the view refused one, i.e. it is not really an editor.
	 */
	private static boolean connect(View view) {
		EditorInfo attrs = new EditorInfo();
		if (view.onCreateInputConnection(attrs) == null)
			return false;
		activeView = view;
		activeInputType = attrs.inputType;
		activeImeOptions = attrs.imeOptions;
		update(view, true);
		return true;
	}

	/**
	 * Drop the input method's uncommitted text. The editor keeps the characters
	 * — atlas splices the composing region into the editor's own content, so
	 * "the IME no longer owns them" means finishing the region, not deleting
	 * it. Leaving that out strands a composing region that the next preedit
	 * update would then replace, silently eating the word.
	 */
	private static void resetInput() {
		if (activeView != null)
			activeView.onFinishComposing();
		if (has_backend)
			nativeReset();
	}

	private static boolean setActiveView(View view) {
		if (activeView == view)
			return true;
		// Whatever the input method still has uncommitted belongs to the editor
		// being left, not to this one.
		resetInput();
		activeView = null;
		return connect(view);
	}

	/**
	 * The focused editor's text or selection changed. Called by the editor
	 * itself on every edit, including the ones the app makes behind the input
	 * method's back.
	 */
	public static void onEditorStateChanged(View view) {
		if (view == activeView)
			update(view, false);
	}

	/**
	 * The editor's content was replaced by the app (setText), so whatever the
	 * input method still holds refers to text that is gone. Drop its preedit
	 * before pushing the new state, or it will commit the old word back.
	 */
	public static void onEditorContentReplaced(View view) {
		if (view != activeView || !has_backend)
			return;
		// not a focus change: the server turns that into handleFocusChange on
		// every plugin, and an app that rewrites its text from a TextWatcher
		// would trigger it on every keystroke. The reset is what tells the
		// keyboard to forget the old word.
		nativeReset();
		update(view, false);
	}

	/**
	 * The caret moved for a reason the input method did not cause. Its
	 * composing region refers to where the word used to be, so the next preedit
	 * update would land there rather than at the caret.
	 */
	public static void onEditorCaretMoved(View view) {
		if (view != activeView || !has_backend)
			return;
		nativeReset();
		update(view, false);
	}

	// --- focus -----------------------------------------------------------

	/* Called when the focused view changes. An editor losing focus releases the
	 * context: the panel is hidden, the preedit dropped, and the server told
	 * that nothing accepts input. */
	public static void onFocusChanged(View newFocus) {
		boolean acceptsInput = newFocus != null && newFocus.onCheckIsTextEditor();
		if (acceptsInput) {
			setActiveView(newFocus);
			return;
		}
		if (activeView != null)
			release();
	}

	/**
	 * The window gained or lost keyboard focus, or was hidden. Losing it
	 * releases the input context: the keyboard now belongs to whatever the
	 * shell focused instead, and a context left active would keep receiving its
	 * commits.
	 */
	public static void onWindowFocusChanged(boolean hasFocus, View focusedView) {
		if (!hasFocus) {
			if (activeView != null)
				release();
			return;
		}
		if (focusedView != null && focusedView.onCheckIsTextEditor())
			setActiveView(focusedView);
	}

	private static void release() {
		resetInput();
		activeView = null;
		if (!has_backend)
			return;
		nativeUpdate(null, 0, 0, 0, 0, false, true);
		nativeHideSoftInput();
	}

	// --- public API ------------------------------------------------------

	public boolean hideSoftInputFromWindow(IBinder windowToken, int flags) {
		if (!has_backend)
			return false;
		resetInput();
		nativeHideSoftInput();
		return true;
	}

	public boolean showSoftInput(View view, int flags) {
		if (!has_backend || view == null)
			return false;
		// The state has to reach the server before the panel is asked for:
		// maliit shows the keyboard for whatever widget it last heard about.
		if (!setActiveView(view))
			return false;
		nativeShowSoftInput();
		return true;
	}

	public boolean isFullscreenMode() { return false; }

	public boolean isActive(View view) {
		return activeView == view;
	}

	public boolean isActive() {
		return activeView != null;
	}

	public List<InputMethodInfo> getEnabledInputMethodList() {
		return input_method_list;
	}

	public List<InputMethodInfo> getInputMethodList() {
		return input_method_list;
	}

	/** The editor was rebuilt or its input type changed: resynchronise fully. */
	public void restartInput(View view) {
		if (view != activeView || !has_backend)
			return;
		resetInput();
		connect(view);
	}

	public void updateSelection(View view, int selStart, int selEnd, int candidatesStart, int candidatesEnd) {
		onEditorStateChanged(view);
	}

	public InputMethodSubtype getCurrentInputMethodSubtype() {
		return new InputMethodSubtype();
	}


	/* Cursor-position and extracted-text reporting: maliit takes the editor
	 * state through nativeUpdate() instead, so these only have to link. */
	public void updateCursorAnchorInfo(android.view.View view, android.view.inputmethod.CursorAnchorInfo info) { }

	public void updateExtractedText(android.view.View view, int token, android.view.inputmethod.ExtractedText text) { }

	private static native boolean nativeInit();
	/** updateWidgetInformation: the editor state, plus whether focus just moved */
	private static native void nativeUpdate(String surroundingText, int cursorPosition, int anchorPosition,
	                                        int inputType, int imeOptions, boolean focused, boolean focusChanged);
	/** drop the input method's preedit without committing it */
	private static native void nativeReset();
	private static native void nativeShowSoftInput();
	private static native void nativeHideSoftInput();
}
