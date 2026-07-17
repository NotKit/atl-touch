package android.view;

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
	private View view;
	private View focusedView; // the view currently receiving key input, if any
	private int width;
	private int height;

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

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
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
		WindowManager.LayoutParams lp = panel.params;
		int widthSpec = lp.width >= 0
		    ? View.MeasureSpec.makeMeasureSpec(Math.min(lp.width, width), View.MeasureSpec.EXACTLY)
		    : lp.width == WindowManager.LayoutParams.MATCH_PARENT
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
			view.measure(View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
			             View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
			view.layout(0, 0, width, height);
		}
		for (Panel panel : panels)
			layoutPanel(panel);
	}

	/* called from native (ATLSceneWidget snapshot); canvas_ptr is an ATLCanvas* */
	protected void performDraw(long canvas_ptr, int width, int height) {
		if (view == null && panels.isEmpty())
			return;
		boolean layoutNeeded = width != this.width || height != this.height
		    || (view != null && view.isLayoutRequested());
		for (Panel panel : panels)
			layoutNeeded |= panel.view.isLayoutRequested();
		if (layoutNeeded)
			performLayout(width, height);
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
			panel.view.draw(canvas);
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
				if ((panel.params.flags & WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE) == 0
				    && x >= pv.getLeft() && x < pv.getRight() && y >= pv.getTop() && y < pv.getBottom()) {
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
		return target.dispatchTouchEvent(event);
	}

	protected boolean dispatchKeyEvent(KeyEvent event) {
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

	/* called from native (ATLSceneWidget char callback): a typed Unicode codepoint,
	 * already resolved through the OS keyboard layout. */
	protected boolean dispatchCharacter(int codePoint) {
		return focusedView != null && focusedView.onTextInput(codePoint);
	}

	/* IME text: a finalized string replacing any composing region. */
	protected boolean dispatchCommitText(String text) {
		return focusedView != null && focusedView.onCommitText(text);
	}

	/* IME preedit: provisional (underlined) text, replaced in place until
	 * committed. */
	protected boolean dispatchComposingText(String text) {
		return focusedView != null && focusedView.onComposingText(text);
	}

	protected void dispatchFinishComposing() {
		if (focusedView != null)
			focusedView.onFinishComposing();
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
		if (old != null)
			old.dispatchFocusChanged(false);
		if (v != null)
			v.dispatchFocusChanged(true);
		android.view.inputmethod.InputMethodManager.onFocusChanged(v);
	}

	public void invalidate() {
		if (scene != 0)
			nativeInvalidate(scene);
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
