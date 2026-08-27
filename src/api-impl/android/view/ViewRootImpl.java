package android.view;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Outline;
import android.graphics.Rect;

import java.util.ArrayList;

/**
 * ViewRootImpl: the bridge between a window (one native ATLSceneWidget) and
 * the pure-Java view hierarchy. The native side calls in for layout, drawing
 * and input; the Java side schedules frames through it.
 *
 * Besides the main view (the activity decor), a view root composites a stack
 * of panels: independent view trees layered above the main view within the
 * same native window. Panels are how Dialog, PopupWindow and
 * WindowManager.addView render in-scene — Android's floating sub-windows,
 * without spawning separate OS windows.
 */
public class ViewRootImpl implements ViewParent {

	public long scene; // ATLSceneWidget*, set by native code on attach
	public Window window;
	/** damage accumulated since the last frame, in window coordinates; read
	 *  (and clipped to) by native atl_window_render, reset by performDraw */
	private final Rect mDirty = new Rect();
	private View view;
	private View focusedView; // the view currently receiving key input, if any
	private int width;
	private int height;
	private int imeInset; // window height covered by the soft keyboard, in px

	/** callbacks a panel owner (Dialog, PopupWindow) uses to react to modal input */
	public interface PanelCallbacks {
		/** back key released while this panel is topmost and its content didn't consume it */
		boolean onPanelBack();
		/** a gesture started outside the panel while it is touch-modal */
		boolean onPanelOutsideTouch();
	}

	private static class Panel {
		final View view;
		WindowManager.LayoutParams params;
		final PanelCallbacks callbacks;

		Panel(View view, WindowManager.LayoutParams params, PanelCallbacks callbacks) {
			this.view = view;
			this.params = params;
			this.callbacks = callbacks;
		}
	}

	private final ArrayList<Panel> panels = new ArrayList<>();
	private View touchTarget;        // view tree owning the current gesture (a panel view or the main view)
	private boolean gestureConsumed; // gesture started outside a touch-modal panel: swallow it entirely
	// A panel may call WindowManager.updateViewLayout() from inside its own
	// onLayout() (Compose's PopupLayout does), which lands back in layoutPanel.
	// AOSP schedules a traversal there; lay the panel out on the next frame
	// instead of re-entering, or the two call each other until the stack is gone.
	private boolean inPanelLayout;
	private boolean panelLayoutPending;

	public ViewRootImpl(Window window) {
		this.window = window;
	}

	public void setView(View view) {
		if (this.view == view)
			return;
		if (this.view != null)
			this.view.dispatchDetachedFromWindow();
		focusedView = null;
		this.view = view;
		if (view != null) {
			view.parent = this;
			view.viewRootImpl = this;
			view.dispatchAttachedToWindow();
			requestLayout();
		}
	}

	public View getView() {
		return view;
	}

	/**
	 * The window resized, so the Configuration the view tree measured itself
	 * against changed. AOSP dispatches this from ActivityThread once
	 * Resources' configuration has been updated; ATLWindow calls it here, just
	 * before the relayout.
	 *
	 * Unlike AOSP nothing is recreated when the activity did not declare the
	 * change in android:configChanges -- ATL has no activity relaunch -- so the
	 * callback is delivered either way.
	 */
	public void dispatchConfigurationChanged() {
		if (Context.r == null)
			return;
		Configuration config = Context.r.getConfiguration();
		Window.Callback callback = window != null ? window.getCallback() : null;
		if (callback instanceof Activity)
			((Activity)callback).onConfigurationChanged(config);
		if (view != null)
			view.dispatchConfigurationChanged(config);
		for (Panel panel : panels.toArray(new Panel[0]))
			panel.view.dispatchConfigurationChanged(config);
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	/* --- soft keyboard ---
	 *
	 * The window is never resized for the keyboard: like AOSP, the root view
	 * keeps the full window height and the keyboard is taken out of the content
	 * as decor padding. Apps derive the keyboard height from the difference
	 * between the two (getRootView().getHeight() minus the visible display
	 * frame), so shrinking both would report a keyboard height of zero.
	 */

	/* called from native when the IME panel geometry changes */
	protected void setImeInset(int inset) {
		imeInset = Math.max(0, inset);
	}

	/** part of the keyboard the content is laid out around (adjustResize) */
	private int contentImeInset() {
		if (imeInset <= 0)
			return 0;
		int mode = window != null
		    ? window.getAttributes().softInputMode & WindowManager.LayoutParams.SOFT_INPUT_MASK_ADJUST : 0;
		// pan/nothing leave the layout alone; the app is told about the keyboard
		// through WindowInsets instead
		if (mode == WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN
		    || mode == WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING)
			return 0;
		return Math.min(imeInset, height);
	}

	/** window height not covered by the soft keyboard */
	public int getVisibleHeight() {
		return height - contentImeInset();
	}

	/** insets the layout did not already account for, as reported to the app */
	public WindowInsets getWindowInsets() {
		return new WindowInsets(imeInset - contentImeInset());
	}

	/* --- panels --- */

	public void addPanel(View panelView, WindowManager.LayoutParams params, PanelCallbacks callbacks) {
		if (findPanel(panelView) != null)
			return;
		Panel panel = new Panel(panelView, params, callbacks);
		panels.add(panel);
		panelView.parent = this;
		panelView.viewRootImpl = this;
		panelView.dispatchAttachedToWindow();
		// the layer below must not keep key/text input while a panel covers it
		setFocusedView(null);
		if (width > 0 && height > 0)
			layoutPanel(panel);
		WindowManagerGlobal.onPanelAdded(panelView);
		invalidate();
	}

	public void removePanel(View panelView) {
		Panel panel = findPanel(panelView);
		if (panel == null)
			return;
		panels.remove(panel);
		WindowManagerGlobal.onPanelRemoved(panelView);
		if (focusedView != null && isInTree(focusedView, panelView))
			setFocusedView(null);
		if (touchTarget == panelView)
			touchTarget = null;
		panelView.dispatchDetachedFromWindow();
		panelView.parent = null;
		panelView.viewRootImpl = null;
		invalidate();
	}

	public void updatePanel(View panelView, WindowManager.LayoutParams params) {
		Panel panel = findPanel(panelView);
		if (panel == null)
			return;
		panel.params = params;
		if (width > 0 && height > 0)
			layoutPanel(panel);
		invalidate();
	}

	public boolean hasPanel(View panelView) {
		return findPanel(panelView) != null;
	}

	private Panel findPanel(View panelView) {
		for (Panel panel : panels) {
			if (panel.view == panelView)
				return panel;
		}
		return null;
	}

	private static boolean isInTree(View candidate, View root) {
		for (View v = candidate; v != null; v = v.parent instanceof View ? (View)v.parent : null) {
			if (v == root)
				return true;
		}
		return false;
	}

	/** measure the panel against the window per its LayoutParams and position it by gravity */
	private void layoutPanel(Panel panel) {
		if (inPanelLayout) {
			panelLayoutPending = true;
			return;
		}
		inPanelLayout = true;
		try {
			layoutPanelInner(panel);
		} finally {
			inPanelLayout = false;
		}
	}

	private void layoutPanelInner(Panel panel) {
		WindowManager.LayoutParams lp = panel.params;
		// panels (dialogs, popups) live above the keyboard, not under it
		int height = getVisibleHeight();
		// floating dialog: resolve its width from the live window size now, not from
		// the (possibly still-zero) size the window had when the dialog was shown
		int lpWidth = lp.width;
		if (lpWidth < 0 && (lp.floatingWidthMajor > 0 || lp.floatingWidthMinor > 0)) {
			float fraction = width > this.height ? lp.floatingWidthMajor : lp.floatingWidthMinor;
			if (fraction > 0) {
				float density = panel.view.getResources().getDisplayMetrics().density;
				lpWidth = Math.min((int)(width * fraction), (int)(560 * density));
			}
		}
		int widthSpec = lpWidth >= 0
		    ? View.MeasureSpec.makeMeasureSpec(Math.min(lpWidth, width), View.MeasureSpec.EXACTLY)
		    : lpWidth == WindowManager.LayoutParams.MATCH_PARENT
		        ? View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY)
		        : View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.AT_MOST);
		int heightSpec = lp.height >= 0
		    ? View.MeasureSpec.makeMeasureSpec(Math.min(lp.height, height), View.MeasureSpec.EXACTLY)
		    : lp.height == WindowManager.LayoutParams.MATCH_PARENT
		        ? View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY)
		        : View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.AT_MOST);
		panel.view.measure(widthSpec, heightSpec);
		int w = panel.view.getMeasuredWidth();
		int h = panel.view.getMeasuredHeight();
		Rect container = new Rect(0, 0, width, height);
		Rect out = new Rect();
		int gravity = lp.gravity != 0 ? lp.gravity : Gravity.CENTER;
		Gravity.apply(gravity, w, h, container, lp.x, lp.y, out);
		// keep the panel inside the window
		if (out.right > width)
			out.offset(width - out.right, 0);
		if (out.bottom > height)
			out.offset(0, height - out.bottom);
		if (out.left < 0)
			out.offset(-out.left, 0);
		if (out.top < 0)
			out.offset(0, -out.top);
		panel.view.layout(out.left, out.top, out.left + w, out.top + h);
	}

	/* called from native (ATLSceneWidget size_allocate) */
	protected void performLayout(int width, int height) {
		this.width = width;
		this.height = height;
		if (view != null) {
			// keyboard comes out of the content as decor padding, not out of the
			// window; guard the write, setPadding always requests a layout
			int inset = contentImeInset();
			if (view.getPaddingBottom() != inset)
				view.setPadding(view.getPaddingLeft(), view.getPaddingTop(), view.getPaddingRight(), inset);
			view.measure(View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
			             View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
			view.layout(0, 0, width, height);
		}
		panelLayoutPending = false;
		for (Panel panel : panels)
			layoutPanel(panel);
		if (DUMP_HIERARCHY) {
			if (view != null)
				dumpHierarchy(view, "");
			for (Panel panel : panels)
				dumpHierarchy(panel.view, "[panel] ");
		}
	}

	private static final boolean DUMP_HIERARCHY = System.getenv("ATL_DUMP_HIERARCHY") != null;
	private static final boolean DEBUG_INVALIDATE = System.getenv("ATL_DEBUG_INVALIDATE") != null;

	private static void dumpHierarchy(View v, String indent) {
		ViewGroup.LayoutParams lp = v.getLayoutParams();
		System.out.println("ATL_DUMP: " + indent + v.getClass().getName()
		    + " id=0x" + Integer.toHexString(v.getId())
		    + " bounds=" + v.getLeft() + "," + v.getTop() + "-" + v.getRight() + "," + v.getBottom()
		    + " measured=" + v.getMeasuredWidth() + "x" + v.getMeasuredHeight()
		    + " minH=" + v.getMinimumHeight()
		    + (lp != null ? " lp=" + lp.width + "x" + lp.height : "")
		    + " vis=" + v.getVisibility()
		    + " pad=" + v.getPaddingLeft() + "," + v.getPaddingTop() + "," + v.getPaddingRight() + "," + v.getPaddingBottom()
		    + (v instanceof android.widget.TextView
		        ? " textSize=" + ((android.widget.TextView)v).getTextSize()
		            + " text='" + ((android.widget.TextView)v).getText() + "'"
		        : ""));
		if (v instanceof ViewGroup) {
			ViewGroup vg = (ViewGroup)v;
			for (int i = 0; i < vg.getChildCount(); i++)
				dumpHierarchy(vg.getChildAt(i), indent + "  ");
		}
	}

	/* called from native (ATLSceneWidget snapshot); canvas_ptr is an ATLCanvas*
	 * already clipped to this frame's damage region (native read mDirty first) */
	protected void performDraw(long canvas_ptr, int width, int height) {
		mDirty.setEmpty(); // invalidations from here on belong to the next frame
		if (view == null && panels.isEmpty())
			return;
		boolean layoutNeeded = width != this.width || height != this.height
		    || (view != null && view.isLayoutRequested());
		for (Panel panel : panels)
			layoutNeeded |= panel.view.isLayoutRequested();
		layoutNeeded |= panelLayoutPending;
		if (layoutNeeded)
			performLayout(width, height);
		if (window != null && window.view_tree_observer != null)
			window.view_tree_observer.dispatchOnDraw();
		Canvas canvas = new DisplayListCanvas(canvas_ptr);
		if (view != null)
			view.draw(canvas);
		for (Panel panel : panels) {
			if (panel.view.getVisibility() != View.VISIBLE)
				continue;
			if ((panel.params.flags & WindowManager.LayoutParams.FLAG_DIM_BEHIND) != 0)
				canvas.drawColor(((int)(panel.params.dimAmount * 255) << 24));
			drawPanelShadow(canvas, panel.view);
			canvas.save();
			canvas.translate(panel.view.getLeft(), panel.view.getTop());
			// a panel is a window of its own size: clip like AOSP's surface would,
			// so panel damage (the panel's bounds) covers everything it can draw
			canvas.clipRect(0, 0, panel.view.getWidth(), panel.view.getHeight());
			// ...and give it that surface. On AOSP a panel draws into its own
			// transparent buffer which the compositor blends over the window, so a
			// non-SrcOver paint in the panel never touches the layer below. Drawing
			// straight onto the shared canvas would let it: Telegram's bottom sheets
			// paint their dim with PorterDuff.SRC, which replaced the whole activity
			// with black instead of shading it.
			int layer = canvas.saveLayer(0, 0, panel.view.getWidth(), panel.view.getHeight(), null);
			panel.view.draw(canvas);
			canvas.restoreToCount(layer);
			canvas.restore();
		}
	}

	/* Android default light and shadow parameters (ThreadedRenderer / config.xml) */
	private static final float LIGHT_Z_DP = 500;
	private static final float LIGHT_RADIUS_DP = 800;
	private static final int AMBIENT_SHADOW_COLOR = 0x0A000000; // alpha 0.039
	private static final int SPOT_SHADOW_COLOR = 0x30000000;    // alpha 0.19

	/** On Android the parent draws the elevation shadow of a child; for panels
	 *  (popups, dialogs) that parent is the view root. Round-rect outlines only. */
	private void drawPanelShadow(Canvas canvas, View v) {
		float elevation = v.getElevation();
		ViewOutlineProvider provider = v.getOutlineProvider();
		if (elevation <= 0 || provider == null)
			return;
		if (v.getBackground() != null) // BACKGROUND provider queries the drawable's bounds
			v.getBackground().setBounds(0, 0, v.getWidth(), v.getHeight());
		Outline outline = new Outline();
		provider.getOutline(v, outline);
		if (outline.mMode != Outline.MODE_ROUND_RECT || outline.mRect.isEmpty())
			return;
		float density = v.getResources().getDisplayMetrics().density;
		canvas.drawShadow(v.getLeft() + outline.mRect.left, v.getTop() + outline.mRect.top,
		                  v.getLeft() + outline.mRect.right, v.getTop() + outline.mRect.bottom,
		                  Math.max(outline.mRadius, 0), elevation,
		                  width / 2.f, 0, LIGHT_Z_DP * density, LIGHT_RADIUS_DP * density,
		                  AMBIENT_SHADOW_COLOR, SPOT_SHADOW_COLOR);
	}

	/* called from native (ATLSceneWidget event controllers) */
	protected boolean dispatchTouchEvent(MotionEvent event) {
		if (event == null)
			return false;
		int action = event.getAction();
		if (action == MotionEvent.ACTION_DOWN) {
			touchTarget = null;
			gestureConsumed = false;
			float x = event.getX();
			float y = event.getY();
			for (int i = panels.size() - 1; i >= 0; i--) {
				Panel panel = panels.get(i);
				View pv = panel.view;
				if (pv.getVisibility() != View.VISIBLE)
					continue;
				if ((panel.params.flags & WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE) != 0)
					continue;  // input-transparent overlay: not a target, not modal either
				if (x >= pv.getLeft() && x < pv.getRight() && y >= pv.getTop() && y < pv.getBottom()) {
					touchTarget = pv;
					break;
				}
				if ((panel.params.flags & WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL) == 0) {
					// touch-modal panel: an outside gesture belongs to nobody below it
					if (panel.callbacks != null)
						panel.callbacks.onPanelOutsideTouch();
					gestureConsumed = true;
					break;
				}
			}
			if (touchTarget == null && !gestureConsumed)
				touchTarget = view;
		}
		if (gestureConsumed) {
			if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL)
				gestureConsumed = false;
			return true;
		}
		View target = touchTarget;
		if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL)
			touchTarget = null;
		if (target == null)
			return false;
		if (target != view)
			event.offsetLocation(-target.getLeft(), -target.getTop());
		try {
			return target.dispatchTouchEvent(event);
		} catch (Throwable t) {
			// A throwing input handler would otherwise be swallowed silently at
			// the native boundary; log it so the failure is visible.
			System.err.println("exception dispatching touch event:");
			t.printStackTrace();
			return true;
		}
	}

	private static final boolean DEBUG_INPUT = System.getenv("ATL_DEBUG_INPUT") != null;

	protected boolean dispatchKeyEvent(KeyEvent event) {
		if (event.getAction() == KeyEvent.ACTION_DOWN)
			lastKeyDownUnicode = event.getUnicodeChar();
		if (DEBUG_INPUT)
			android.util.Log.i("ATLInput", "dispatchKeyEvent code=" + event.getKeyCode()
			                   + " action=" + event.getAction() + " focused=" + focusedView);
		// Route to the focused view first (e.g. a focused EditText), then fall back
		// to the decor (back button, accelerators, etc.).
		if (focusedView != null && focusedView != view && focusedView.dispatchKeyEvent(event))
			return true;
		// topmost focusable panel gets the keys; panels are key-modal, so the
		// layer below never sees them
		for (int i = panels.size() - 1; i >= 0; i--) {
			Panel panel = panels.get(i);
			if (panel.view.getVisibility() != View.VISIBLE
			    || (panel.params.flags & WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE) != 0)
				continue;
			if (panel.view.dispatchKeyEvent(event))
				return true;
			if (event.getKeyCode() == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP
			    && panel.callbacks != null)
				panel.callbacks.onPanelBack();
			return true;
		}
		return view != null && view.dispatchKeyEvent(event);
	}

	/* called from native (scroll callback): a SOURCE_MOUSE ACTION_SCROLL event,
	 * which Android routes down the generic-motion path, not the touch path. */
	protected boolean dispatchGenericMotionEvent(MotionEvent event) {
		if (DEBUG_INPUT)
			android.util.Log.i("ATLInput", "dispatchGenericMotionEvent action="
			                   + event.getActionMasked() + " vscroll="
			                   + event.getAxisValue(MotionEvent.AXIS_VSCROLL));
		for (int i = panels.size() - 1; i >= 0; i--) {
			Panel panel = panels.get(i);
			if (panel.view.getVisibility() != View.VISIBLE)
				continue;
			return panel.view.dispatchGenericMotionEvent(event);
		}
		return view != null && view.dispatchGenericMotionEvent(event);
	}

	/* called from native (ATLSceneWidget char callback): a typed Unicode codepoint,
	 * already resolved through the OS keyboard layout. */
	protected boolean dispatchCharacter(int codePoint) {
		if (DEBUG_INPUT)
			android.util.Log.i("ATLInput", "dispatchCharacter U+" + Integer.toHexString(codePoint)
			                   + " focused=" + focusedView);
		if (focusedView == null)
			return false;
		if (focusedView.onTextInput(codePoint))
			return true;
		// The key callback already delivered this keystroke; repeat it as a typed
		// character only if that event carried no character of its own, which is
		// the case for every key ATL maps to KEYCODE_UNKNOWN.
		if (lastKeyDownUnicode != 0)
			return false;
		return typeCodePoint(codePoint);
	}

	/* IME text: a finalized string replacing any composing region. */
	protected boolean dispatchCommitText(String text, int replaceStart, int replaceLength, int cursorPos) {
		if (focusedView == null)
			return false;
		if (focusedView.onCommitText(text, replaceStart, replaceLength, cursorPos))
			return true;
		boolean any = false;
		for (int i = 0; i < text.length(); ) {
			int cp = text.codePointAt(i);
			any |= typeCodePoint(cp);
			i += Character.charCount(cp);
		}
		return any;
	}

	/* Codepoint of the key event dispatched most recently, so a typed character
	 * is not delivered twice. */
	private int lastKeyDownUnicode;

	/* Deliver a codepoint that has no keycode of its own as a key event: views
	 * that do not implement ATL's onTextInput -- Compose, and every AOSP text
	 * widget -- type from KeyEvent.getUnicodeChar() and see nothing otherwise. */
	private boolean typeCodePoint(int codePoint) {
		KeyEvent down = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_UNKNOWN, 0, 0);
		down.unicodeValue = codePoint;
		boolean handled = focusedView.dispatchKeyEvent(down);
		KeyEvent up = new KeyEvent(0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_UNKNOWN, 0, 0);
		up.unicodeValue = codePoint;
		focusedView.dispatchKeyEvent(up);
		return handled;
	}

	/* IME preedit: provisional (underlined) text, replaced in place until
	 * committed. */
	protected boolean dispatchComposingText(String text, int replaceStart, int replaceLength, int cursorPos) {
		return focusedView != null && focusedView.onComposingText(text, replaceStart, replaceLength, cursorPos);
	}

	protected void dispatchFinishComposing() {
		if (focusedView != null)
			focusedView.onFinishComposing();
	}

	protected void dispatchImeSetSelection(int start, int length) {
		if (focusedView != null)
			focusedView.onImeSetSelection(start, length);
	}

	/* The input method asks for the selected text (maliit's selection()); "" is
	 * a valid answer meaning "nothing selected". */
	protected String dispatchImeGetSelection() {
		if (focusedView == null)
			return null;
		CharSequence text = focusedView.getImeSurroundingText();
		if (text == null)
			return null;
		int start = Math.min(focusedView.getImeCursorPosition(), focusedView.getImeAnchorPosition());
		int end = Math.max(focusedView.getImeCursorPosition(), focusedView.getImeAnchorPosition());
		start = Math.max(0, Math.min(start, text.length()));
		end = Math.max(0, Math.min(end, text.length()));
		return text.subSequence(start, end).toString();
	}

	/* The input method closed its own panel (the user dismissed the keyboard).
	 * Maliit's Qt context drops the editor's focus here, so the app stops
	 * showing a caret it can no longer type into. */
	protected void dispatchImInitiatedHide() {
		if (focusedView != null)
			focusedView.onFinishComposing();
		setFocusedView(null);
	}

	/* The window gained or lost keyboard focus (compositor-side), or was
	 * hidden. An input method context must not outlive that: the panel belongs
	 * to whoever has focus now. */
	protected void dispatchWindowFocusChanged(boolean hasFocus) {
		if (!hasFocus && focusedView != null)
			focusedView.onFinishComposing();
		android.view.inputmethod.InputMethodManager.onWindowFocusChanged(hasFocus, focusedView);
	}

	public View getFocusedView() {
		return focusedView;
	}

	/** Move input focus to the given view (or null to clear), notifying both. */
	public void setFocusedView(View v) {
		if (focusedView == v)
			return;
		View old = focusedView;
		focusedView = v;
		if (old != null) {
			// the composing region is the input method's, and it does not
			// follow focus to the next editor
			old.onFinishComposing();
			old.dispatchFocusChanged(false);
		}
		if (v != null)
			v.dispatchFocusChanged(true);
		android.view.inputmethod.InputMethodManager.onFocusChanged(v);
	}

	/** Invalidate the whole window. */
	public void invalidate() {
		// pre-layout the size is unknown; union something large, native clamps
		// the damage to the framebuffer
		mDirty.union(0, 0, width > 0 ? width : Integer.MAX_VALUE / 2,
		             height > 0 ? height : Integer.MAX_VALUE / 2);
		if (scene != 0)
			nativeInvalidate(scene);
	}

	/* AOSP software invalidation: a view whose parent is this root (the main
	 * view or a panel root) reports its damage here, in its own coordinates. */
	@Override
	public void invalidateChild(View child, Rect dirty) {
		ViewGroup.mapRect(child.getMatrix(), dirty);
		invalidateChildInParent(new int[] {child.getLeft(), child.getTop()}, dirty);
	}

	@Override
	public ViewParent invalidateChildInParent(int[] location, Rect dirty) {
		if (dirty == null) {
			invalidate();
			return null;
		}
		if (dirty.isEmpty())
			return null;
		if (location != null)
			dirty.offset(location[0], location[1]);
		if (DEBUG_INVALIDATE) {
			System.err.println("ATL_INVAL " + dirty);
			StackTraceElement[] st = Thread.currentThread().getStackTrace();
			for (int i = 2; i < Math.min(st.length, 14); i++)
				System.err.println("  " + st[i]);
		}
		mDirty.union(dirty.left, dirty.top, dirty.right, dirty.bottom);
		if (width > 0 && height > 0 && !mDirty.intersect(0, 0, width, height))
			mDirty.setEmpty();
		if (scene != 0)
			nativeInvalidate(scene);
		return null;
	}

	public void requestLayout() {
		if (scene != 0)
			nativeRequestLayout(scene);
	}

	private static native void nativeInvalidate(long scene);
	private static native void nativeRequestLayout(long scene);

	/* --- ViewParent --- */

	@Override
	public ViewParent getParent() {
		return null;
	}

	@Override
	public boolean isLayoutRequested() {
		return false;
	}

	@Override
	public void requestDisallowInterceptTouchEvent(boolean disallowIntercept) {}

	@Override
	public boolean onStartNestedScroll(View child, View target, int nestedScrollAxes) {
		return false;
	}

	@Override
	public boolean onNestedPreFling(View target, float velocityX, float velocityY) {
		return false;
	}

	@Override
	public boolean onNestedFling(View target, float velocityX, float velocityY, boolean consumed) {
		return false;
	}

	@Override
	public void onNestedScrollAccepted(View child, View target, int nestedScrollAxes) {}

	@Override
	public void onNestedPreScroll(View target, int dx, int dy, int[] consumed) {}

	@Override
	public void onNestedScroll(View target, int dxConsumed, int dyConsumed, int dxUnconsumed, int dyUnconsumed) {}

	@Override
	public void onStopNestedScroll(View target) {}

	@Override
	public void onDescendantInvalidated(View child, View target) {}
}
