package android.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Menu;
import android.view.View;
import android.view.ViewGroup;

/**
 * The framework Toolbar's row of action buttons. Nothing in ATL builds one --
 * an appcompat Toolbar fills itself with androidx.appcompat.widget.ActionMenuView,
 * a different class -- but the type has to exist, because apps test for it.
 *
 * Fenix's setToolbarColors does `when (child) { is ActionMenuView -> ... }` over
 * the toolbar's children, and with the class missing that instanceof threw
 * NoClassDefFoundError out of LibraryPageView.updateToolbar, on the main thread,
 * uncaught: History, Bookmarks and Downloads each killed the process.
 */
public class ActionMenuView extends LinearLayout {

	public interface OnMenuItemClickListener {
		boolean onMenuItemClick(android.view.MenuItem item);
	}

	private OnMenuItemClickListener mOnMenuItemClickListener;
	private int mPopupTheme;

	public ActionMenuView(Context context) {
		super(context);
	}

	public ActionMenuView(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public void setPopupTheme(int resId) {
		mPopupTheme = resId;
	}

	public int getPopupTheme() {
		return mPopupTheme;
	}

	public void setOnMenuItemClickListener(OnMenuItemClickListener listener) {
		mOnMenuItemClickListener = listener;
	}

	public Menu getMenu() {
		return null;
	}

	public void dismissPopupMenus() {}

	public boolean isOverflowMenuShowing() {
		return false;
	}

	public boolean hideOverflowMenu() {
		return false;
	}

	public boolean showOverflowMenu() {
		return false;
	}

	public static class LayoutParams extends LinearLayout.LayoutParams {
		public boolean isOverflowButton;
		public int cellsUsed;
		public int extraPixels;
		public boolean preventEdgeOffset;

		public LayoutParams(Context c, AttributeSet attrs) {
			super(c, attrs);
		}

		public LayoutParams(int width, int height) {
			super(width, height);
		}

		public LayoutParams(ViewGroup.LayoutParams other) {
			super(other);
		}
	}
}
