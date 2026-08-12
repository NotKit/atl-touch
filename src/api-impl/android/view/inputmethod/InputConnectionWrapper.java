package android.view.inputmethod;

import android.os.Bundle;
import android.os.Handler;
import android.view.KeyEvent;

/* AOSP's wrapper: every call goes to the target, which is what an IME's own
   subclass expects when it calls super. */
public class InputConnectionWrapper implements InputConnection {
	private InputConnection mTarget;
	private final boolean mMutable;

	public InputConnectionWrapper(InputConnection target, boolean mutable) {
		mMutable = mutable;
		mTarget = target;
	}

	public void setTarget(InputConnection target) {
		if (mTarget != null && !mMutable)
			throw new SecurityException("not mutable");
		mTarget = target;
	}

	public CharSequence getTextBeforeCursor(int n, int flags) {
		return mTarget.getTextBeforeCursor(n, flags);
	}

	public CharSequence getTextAfterCursor(int n, int flags) {
		return mTarget.getTextAfterCursor(n, flags);
	}

	public CharSequence getSelectedText(int flags) {
		return mTarget.getSelectedText(flags);
	}

	public int getCursorCapsMode(int reqModes) {
		return mTarget.getCursorCapsMode(reqModes);
	}

	public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
		return mTarget.getExtractedText(request, flags);
	}

	public boolean deleteSurroundingText(int beforeLength, int afterLength) {
		return mTarget.deleteSurroundingText(beforeLength, afterLength);
	}

	public boolean deleteSurroundingTextInCodePoints(int beforeLength, int afterLength) {
		return mTarget.deleteSurroundingTextInCodePoints(beforeLength, afterLength);
	}

	public boolean setComposingText(CharSequence text, int newCursorPosition) {
		return mTarget.setComposingText(text, newCursorPosition);
	}

	public boolean setComposingRegion(int start, int end) {
		return mTarget.setComposingRegion(start, end);
	}

	public boolean finishComposingText() {
		return mTarget.finishComposingText();
	}

	public boolean commitText(CharSequence text, int newCursorPosition) {
		return mTarget.commitText(text, newCursorPosition);
	}

	public boolean commitCompletion(CompletionInfo text) {
		return mTarget.commitCompletion(text);
	}

	public boolean commitCorrection(CorrectionInfo correctionInfo) {
		return mTarget.commitCorrection(correctionInfo);
	}

	public boolean setSelection(int start, int end) {
		return mTarget.setSelection(start, end);
	}

	public boolean performEditorAction(int editorAction) {
		return mTarget.performEditorAction(editorAction);
	}

	public boolean performContextMenuAction(int id) {
		return mTarget.performContextMenuAction(id);
	}

	public boolean beginBatchEdit() {
		return mTarget.beginBatchEdit();
	}

	public boolean endBatchEdit() {
		return mTarget.endBatchEdit();
	}

	public boolean sendKeyEvent(KeyEvent event) {
		return mTarget.sendKeyEvent(event);
	}

	public boolean clearMetaKeyStates(int states) {
		return mTarget.clearMetaKeyStates(states);
	}

	public boolean reportFullscreenMode(boolean enabled) {
		return mTarget.reportFullscreenMode(enabled);
	}

	public boolean performPrivateCommand(String action, Bundle data) {
		return mTarget.performPrivateCommand(action, data);
	}

	public Handler getHandler() {
		return mTarget.getHandler();
	}

	public boolean commitContent(InputContentInfo inputContentInfo, int flags, Bundle opts) {
		return mTarget.commitContent(inputContentInfo, flags, opts);
	}

	public boolean requestCursorUpdates(int cursorUpdateMode) {
		return mTarget.requestCursorUpdates(cursorUpdateMode);
	}

	public void closeConnection() {
		mTarget.closeConnection();
	}
}
