package android.widget;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewRootImpl;
import android.view.WindowManager;
import android.view.WindowManagerGlobal;

/**
 * PopupWindow rendered in-scene: the content view is attached as a panel on
 * the view root of the window it pops up over (anchored below/above its
 * anchor view), instead of a separate native popup surface.
 */
public class PopupWindow {

	int input_method_mode = 0;

	private Context context;
	private View contentView;
	private FrameLayout decor; // wraps the content so the popup background doesn't clobber the content's own
	private Drawable background;
	private float elevation;
	private int width = ViewGroup.LayoutParams.WRAP_CONTENT;
	private int height = ViewGroup.LayoutParams.WRAP_CONTENT;
	private boolean focusable;
	private boolean showing;
	private ViewRootImpl hostRoot;
	private WindowManager.LayoutParams panelParams;
	private OnDismissListener onDismissListener;

	private final ViewRootImpl.PanelCallbacks panelCallbacks = new ViewRootImpl.PanelCallbacks() {
		@Override
		public boolean onPanelBack() {
			dismiss();
			return true;
		}

		@Override
		public boolean onPanelOutsideTouch() {
			dismiss();
			return true;
		}
	};

	public PopupWindow(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		this.context = context;
		if (context != null) {
			android.content.res.TypedArray a = context.obtainStyledAttributes(attrs,
			    com.android.internal.R.styleable.PopupWindow, defStyleAttr, defStyleRes);
			background = a.getDrawable(com.android.internal.R.styleable.PopupWindow_popupBackground);
			elevation = a.getDimension(com.android.internal.R.styleable.PopupWindow_popupElevation, 0);
			a.recycle();
		}
	}

	public PopupWindow(Context context) {
		this(context, null, 0, 0);
	}

	public PopupWindow() {
		this(null, null, 0, 0);
	}

	public PopupWindow(View contentView, int width, int height, boolean focusable) {
		this(contentView != null ? contentView.getContext() : null, null, 0, 0);
		setContentView(contentView);
		setWidth(width);
		setHeight(height);
		setFocusable(focusable);
	}

	public PopupWindow(View contentView, int width, int height) {
		this(contentView, width, height, true);
	}

	public PopupWindow(View contentView) {
		this(contentView, 0, 0, false);
	}

	public interface OnDismissListener {
		public void onDismiss();
	}

	public void setBackgroundDrawable(Drawable background) {
		this.background = background;
		if (decor != null)
			decor.setBackgroundDrawable(background);
	}

	public void setInputMethodMode(int mode) {
		input_method_mode = mode;
	}

	public int getInputMethodMode() {
		return input_method_mode;
	}

	public boolean isShowing() {
		return showing;
	}

	public void setFocusable(boolean focusable) {
		this.focusable = focusable;
	}

	public boolean isFocusable() {
		return focusable;
	}

	public Drawable getBackground() {
		return background;
	}

	public void setContentView(View view) {
		if (showing)
			return;
		contentView = view;
		if (context == null && view != null)
			context = view.getContext();
	}

	public View getContentView() {
		return contentView;
	}

	public int getMaxAvailableHeight(View anchor, int yOffset) {
		return getMaxAvailableHeight(anchor, yOffset, false);
	}

	public int getMaxAvailableHeight(View anchor, int yOffset, boolean ignoreKeyboard) {
		ViewRootImpl root = rootFor(anchor);
		if (root == null || root.getHeight() == 0)
			return 500;
		Rect anchorRect = new Rect();
		anchor.getGlobalVisibleRect(anchorRect);
		int below = root.getHeight() - anchorRect.bottom - yOffset;
		int above = anchorRect.top + yOffset;
		return Math.max(below, above);
	}

	public void setOutsideTouchable(boolean touchable) {}

	public void setTouchInterceptor(View.OnTouchListener listener) {}

	public void setTouchable(boolean touchable) {}

	public void setTouchModal(boolean touchModal) {}

	private ViewRootImpl rootFor(View anchor) {
		ViewRootImpl root = anchor != null ? anchor.getViewRootImpl() : null;
		return root != null ? root : WindowManagerGlobal.getActiveViewRoot();
	}

	private FrameLayout makeDecor() {
		decor = new FrameLayout(context != null ? context : contentView.getContext());
		decor.setElevation(elevation);
		if (background != null)
			decor.setBackgroundDrawable(background);
		if (contentView.getParent() instanceof ViewGroup)
			((ViewGroup)contentView.getParent()).removeView(contentView);
		decor.addView(contentView, new FrameLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
		return decor;
	}

	/** measure the decor as the panel layout will, so show-position math can use real sizes */
	private void measureDecor(ViewRootImpl root) {
		int ws = width >= 0
		    ? View.MeasureSpec.makeMeasureSpec(Math.min(width, root.getWidth()), View.MeasureSpec.EXACTLY)
		    : View.MeasureSpec.makeMeasureSpec(root.getWidth(),
		        width == ViewGroup.LayoutParams.MATCH_PARENT ? View.MeasureSpec.EXACTLY : View.MeasureSpec.AT_MOST);
		int hs = height >= 0
		    ? View.MeasureSpec.makeMeasureSpec(Math.min(height, root.getHeight()), View.MeasureSpec.EXACTLY)
		    : View.MeasureSpec.makeMeasureSpec(root.getHeight(),
		        height == ViewGroup.LayoutParams.MATCH_PARENT ? View.MeasureSpec.EXACTLY : View.MeasureSpec.AT_MOST);
		decor.measure(ws, hs);
	}

	private void addPanel(ViewRootImpl root, int x, int y, int gravity) {
		WindowManager.LayoutParams lp = new WindowManager.LayoutParams(width, height, 0, 0, 0);
		lp.x = x;
		lp.y = y;
		lp.gravity = gravity == Gravity.NO_GRAVITY ? (Gravity.TOP | Gravity.LEFT) : gravity;
		if (!focusable)
			lp.flags |= WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
		panelParams = lp;
		hostRoot = root;
		root.addPanel(decor, lp, panelCallbacks);
		showing = true;
	}

	public void showAsDropDown(View anchor) {
		showAsDropDown(anchor, 0, 0, Gravity.NO_GRAVITY);
	}

	public void showAsDropDown(View anchor, int xoff, int yoff) {
		showAsDropDown(anchor, xoff, yoff, Gravity.NO_GRAVITY);
	}

	public void showAsDropDown(View anchor, int xoff, int yoff, int gravity) {
		if (showing || contentView == null)
			return;
		if (!anchor.isAttachedToWindow()) {
			Log.e("PopupWindow", "anchor is not attached to window");
			return;
		}
		ViewRootImpl root = rootFor(anchor);
		if (root == null)
			return;
		makeDecor();
		measureDecor(root);
		Rect anchorRect = new Rect();
		anchor.getGlobalVisibleRect(anchorRect);
		int x = anchorRect.left + xoff;
		if ((gravity & Gravity.RIGHT) == Gravity.RIGHT)
			x = anchorRect.right - decor.getMeasuredWidth() + xoff;
		int y = anchorRect.bottom + yoff;
		// not enough room below and more above: pop up over the anchor instead
		int h = decor.getMeasuredHeight();
		if (y + h > root.getHeight() && anchorRect.top - h - yoff >= 0)
			y = anchorRect.top - h - yoff;
		addPanel(root, x, y, Gravity.TOP | Gravity.LEFT);
	}

	public void showAtLocation(View parent, int gravity, int x, int y) {
		if (showing || contentView == null)
			return;
		ViewRootImpl root = rootFor(parent);
		if (root == null)
			return;
		makeDecor();
		addPanel(root, x, y, gravity);
	}

	public void dismiss() {
		if (!showing)
			return;
		showing = false;
		if (hostRoot != null) {
			hostRoot.removePanel(decor);
			hostRoot = null;
		}
		if (decor != null) {
			decor.removeAllViews(); // free the content for the next show()
			decor = null;
		}
		if (onDismissListener != null)
			onDismissListener.onDismiss();
	}

	public void setOnDismissListener(OnDismissListener listener) {
		onDismissListener = listener;
	}

	public void setAnimationStyle(int animationStyle) {}

	public void setElevation(float elevation) {
		this.elevation = elevation;
		if (decor != null)
			decor.setElevation(elevation);
	}

	public float getElevation() {
		return elevation;
	}

	public void update(View anchor, int xoff, int yoff, int width, int height) {
		if (width != -1 && width != ViewGroup.LayoutParams.WRAP_CONTENT)
			this.width = width;
		if (height != -1 && height != ViewGroup.LayoutParams.WRAP_CONTENT)
			this.height = height;
		if (!showing || hostRoot == null || panelParams == null)
			return;
		Rect anchorRect = new Rect();
		anchor.getGlobalVisibleRect(anchorRect);
		panelParams.width = this.width;
		panelParams.height = this.height;
		panelParams.x = anchorRect.left + xoff;
		panelParams.y = anchorRect.bottom + yoff;
		hostRoot.updatePanel(decor, panelParams);
	}

	public void update(int x, int y, int width, int height) {
		// a negative width/height means "leave the size unchanged" (AOSP contract).
		// don't treat -1 as MATCH_PARENT here — that would resize a WRAP_CONTENT popup
		// to full screen on every reposition.
		if (width >= 0)
			this.width = width;
		if (height >= 0)
			this.height = height;
		if (!showing || hostRoot == null || panelParams == null)
			return;
		panelParams.width = this.width;
		panelParams.height = this.height;
		panelParams.x = x;
		panelParams.y = y;
		hostRoot.updatePanel(decor, panelParams);
	}

	public void setWindowLayoutType(int type) {}

	public void setIsClippedToScreen(boolean isClippedToScreen) {}

	public void setEpicenterBounds(Rect bounds) {}

	public void setClippingEnabled(boolean enabled) {}

	public void setWidth(int width) {
		this.width = width;
	}

	public void setHeight(int height) {
		this.height = height;
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	public void setWindowLayoutMode(int widthSpec, int heightSpec) {}

	public boolean isTouchable() {
		return true;
	}

	public void setOverlapAnchor(boolean overlap) {}

	public void setSoftInputMode(int mode) {}
}
