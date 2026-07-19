package android.app;

import android.R;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.TypedArray;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Slog;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewRootImpl;
import android.view.Window;
import android.view.WindowManager.LayoutParams;
import android.view.WindowManagerGlobal;

/**
 * Dialogs render in-scene: the dialog's decor view is attached as a panel on
 * the view root currently driving the (shared) native window, floating above
 * the activity content with a dimmed scrim — like Android composites dialog
 * windows over the activity on one surface, and unlike a separate OS window.
 */
public class Dialog implements Window.Callback, DialogInterface {
	private static final String TAG = "Dialog";

	private Context context;
	private Window window;
	private OnCancelListener onCancelListener;
	private OnDismissListener onDismissListener;
	private OnShowListener onShowListener;
	private boolean mCreated = false;
	private boolean showing = false;
	private boolean cancelable = true;
	private Boolean canceledOnTouchOutside; // null: default by windowIsFloating at show() time
	private ViewRootImpl hostRoot;          // the view root the decor is attached to while showing

	private final ViewRootImpl.PanelCallbacks panelCallbacks = new ViewRootImpl.PanelCallbacks() {
		@Override
		public boolean onPanelBack() {
			onBackPressed();
			return true;
		}

		@Override
		public boolean onPanelOutsideTouch() {
			if (cancelable && !Boolean.FALSE.equals(canceledOnTouchOutside)) {
				cancel();
				return true;
			}
			return false;
		}
	};

	public Dialog(Context context, int themeResId) {
		this.context = context;
		window = new Window(context, this);
	}

	public Dialog(Context context) {
		this(context, 0);
	}

	public final boolean requestWindowFeature(int featureId) {
		return false;
	}

	public Context getContext() {
		return context;
	}

	public void setContentView(View view) {
		getWindow().setContentView(view);
	}

	public void setContentView(int layoutResId) {
		setContentView(LayoutInflater.from(context).inflate(layoutResId, null));
	}

	public void setTitle(CharSequence title) {
		getWindow().getAttributes().setTitle(title);
	}

	public void setTitle(int titleId) {
		setTitle(context.getString(titleId));
	}

	public void setOwnerActivity(Activity activity) {}

	public void setCancelable(boolean cancelable) {
		this.cancelable = cancelable;
	}

	public void setCanceledOnTouchOutside(boolean cancel) {
		if (cancel)
			cancelable = true;
		canceledOnTouchOutside = cancel;
	}

	public void setOnCancelListener(OnCancelListener onCancelListener) {
		this.onCancelListener = onCancelListener;
	}

	public void setOnDismissListener(OnDismissListener onDismissListener) {
		this.onDismissListener = onDismissListener;
	}

	public View findViewById(int id) {
		return window.findViewById(id);
	}

	public void show() {
		Runnable action = new Runnable() {
			@Override
			public void run() {
				if (showing) {
					// was hide()-den, not dismissed: just make the decor visible again
					View decor = window.getDecorView();
					decor.setVisibility(View.VISIBLE);
					decor.invalidate();
					return;
				}
				// AOSP dispatchOnCreate: onCreate must run exactly once per
				// Dialog instance. androidx's ComponentDialog restores its
				// SavedStateRegistry in onCreate and throws if that happens
				// twice, which is what a DialogFragment shown across two
				// onStart()s would otherwise trigger.
				if (!mCreated) {
					onCreate(null);
					mCreated = true;
				}
				onStart();

				ViewRootImpl root = WindowManagerGlobal.getActiveViewRoot();
				if (root == null) {
					Slog.e(TAG, "show: no active view root to attach the dialog to");
					return;
				}

				// Floating dialogs get the windowMinWidth* fraction of the window as a
				// fixed width (capped at Material's 560dp max); their height wraps the
				// content. The fraction is resolved lazily in ViewRootImpl.layoutPanel
				// against the live window size — a dialog shown before the window is
				// measured (e.g. on launch) would otherwise bake a 0 width here.
				// Non-floating dialogs are technically dialogs but behave like full-size
				// activities: their (typically MATCH_PARENT) params stand.
				TypedArray a = context.obtainStyledAttributes(R.styleable.Window);
				boolean floating = a.getBoolean(R.styleable.Window_windowIsFloating, false);
				LayoutParams lp = getWindow().getAttributes();
				if (floating) {
					if (lp.width < 0) {
						lp.floatingWidthMajor = a.getFraction(R.styleable.Window_windowMinWidthMajor, 1, 1, 1);
						lp.floatingWidthMinor = a.getFraction(R.styleable.Window_windowMinWidthMinor, 1, 1, 1);
					}
					if (lp.gravity == 0)
						lp.gravity = Gravity.CENTER;
					lp.flags |= LayoutParams.FLAG_DIM_BEHIND;
					lp.dimAmount = 0.6f;
				}
				a.recycle();

				View decor = getWindow().getDecorView();
				decor.setVisibility(View.VISIBLE); // may have been hidden by hide()
				hostRoot = root;
				root.addPanel(decor, lp, panelCallbacks);
				showing = true;

				if (onShowListener != null)
					onShowListener.onShow(Dialog.this);
			}
		};
		if (Looper.myLooper() == Looper.getMainLooper()) {
			action.run();
		} else {
			new Handler(Looper.getMainLooper()).post(action);
		}
	}

	public boolean isShowing() {
		return showing;
	}

	public void dismiss() {
		// HACK: dismissing the Dialog takes some time in AOSP, as the request goes back and forth between the application
		// and the system server. We replicate this behavior by adding 10 ms delay.
		// This Hack is required for NewPipe RouterActivity which has a race condition. It subscribes an rxJava observable
		// and immediately calls Dialog.dismiss(). The OnDismissListener would unsubscribes the observable again.
		new Handler(Looper.getMainLooper()).postDelayed(new Runnable() {
			@Override
			public void run() {
				if (!showing)
					return;
				showing = false;
				if (hostRoot != null) {
					hostRoot.removePanel(window.getDecorView());
					hostRoot = null;
				}
				onStop();
				if (onDismissListener != null)
					onDismissListener.onDismiss(Dialog.this);
			}
		}, 10);
	}

	public Window getWindow() {
		return window;
	}

	public class Builder {
		public Builder(Context context) {
			System.out.println("making a Dialog$Builder");
		}
	}

	@Override
	public void onContentChanged() {
	}
	@Override
	public boolean onCreatePanelMenu(int featureId, Menu menu) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onCreatePanelMenu'");
	}
	@Override
	public View onCreatePanelView(int featureId) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onCreatePanelView'");
	}
	@Override
	public boolean onPreparePanel(int featureId, View view, Menu menu) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onPreparePanel'");
	}
	@Override
	public boolean onMenuItemSelected(int featureId, MenuItem item) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onMenuItemSelected'");
	}
	@Override
	public void onPanelClosed(int featureId, Menu menu) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onPanelClosed'");
	}

	@Override
	public boolean onMenuOpened(int featureId, Menu menu) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onMenuOpened'");
	}

	protected void onCreate(Bundle savedInstanceState) {
	}

	protected void onStart() {
	}

	protected void onStop() {
	}

	/** AOSP contract: back cancels a cancelable dialog. androidx's ComponentDialog
	 *  overrides this to route through its OnBackPressedDispatcher. */
	public void onBackPressed() {
		if (cancelable)
			cancel();
	}

	public void hide() {
		// AOSP hide(): the dialog stays showing, its decor just goes invisible
		if (showing)
			window.getDecorView().setVisibility(View.GONE);
	}

	@Override
	public void cancel() {
		if (onCancelListener != null)
			onCancelListener.onCancel(this);
		dismiss();
	}

	public void setOnShowListener(OnShowListener onShowListener) {
		this.onShowListener = onShowListener;
	}

	public void setCancelMessage(Message msg) {}

	public void setDismissMessage(Message msg) {}

	public boolean onTouchEvent(MotionEvent event) {
		return false;
	}

	public void setOnKeyListener(OnKeyListener onKeyListener) {}
}
