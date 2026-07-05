package android.app;

import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * Framework AlertDialog, rebuilt as a plain view hierarchy over the in-scene
 * dialog panel (apps using appcompat/material bring their own AlertDialog and
 * only go through Dialog; this class serves apps and libraries that use the
 * framework one directly).
 */
public class AlertDialog extends Dialog implements DialogInterface {

	private final LinearLayout root;         // title / scrolling content / button bar
	private final TextView titleView;
	private final TextView messageView;
	private final LinearLayout itemsContainer;
	private final FrameLayout customHolder;
	private final LinearLayout buttonBar;
	private final Button[] buttons;          // positive, negative, neutral
	private final float density;

	public AlertDialog(Context context) {
		this(context, 0);
	}

	public AlertDialog(Context context, int themeResId) {
		super(context, themeResId);
		density = context.getResources().getDisplayMetrics().density;

		root = new LinearLayout(context);
		root.setOrientation(LinearLayout.VERTICAL);

		titleView = new TextView(context);
		titleView.setTextSize(20);
		titleView.setTypeface(Typeface.DEFAULT_BOLD);
		titleView.setTextColor(Color.BLACK);
		titleView.setPadding(dp(24), dp(20), dp(24), dp(8));
		titleView.setVisibility(View.GONE);
		root.addView(titleView, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

		LinearLayout content = new LinearLayout(context);
		content.setOrientation(LinearLayout.VERTICAL);

		messageView = new TextView(context);
		messageView.setTextSize(16);
		messageView.setPadding(dp(24), dp(8), dp(24), dp(8));
		messageView.setVisibility(View.GONE);
		content.addView(messageView, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

		itemsContainer = new LinearLayout(context);
		itemsContainer.setOrientation(LinearLayout.VERTICAL);
		content.addView(itemsContainer, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

		customHolder = new FrameLayout(context);
		content.addView(customHolder, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

		ScrollView scroll = new ScrollView(context);
		scroll.addView(content, new FrameLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
		// weight 1 so long content scrolls instead of pushing the buttons off the dialog
		root.addView(scroll, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

		buttonBar = new LinearLayout(context);
		buttonBar.setOrientation(LinearLayout.HORIZONTAL);
		buttonBar.setGravity(Gravity.RIGHT | Gravity.CENTER_VERTICAL);
		buttonBar.setPadding(dp(8), dp(8), dp(8), dp(8));
		buttonBar.setVisibility(View.GONE);
		// order matches Android: neutral first, then negative, positive rightmost
		buttons = new Button[3];
		int[] order = {DialogInterface.BUTTON_NEUTRAL, DialogInterface.BUTTON_NEGATIVE, DialogInterface.BUTTON_POSITIVE};
		for (int which : order) {
			Button button = new Button(context);
			button.setVisibility(View.GONE);
			buttons[-1 - which] = button;
			LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(
			    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
			blp.leftMargin = dp(8);
			buttonBar.addView(button, blp);
		}
		root.addView(buttonBar, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

		setContentView(root);
	}

	private int dp(int dp) {
		return (int)(dp * density + 0.5f);
	}

	@Override
	public void setTitle(CharSequence title) {
		titleView.setText(title);
		titleView.setVisibility(title != null && title.length() > 0 ? View.VISIBLE : View.GONE);
	}

	public void setMessage(CharSequence message) {
		messageView.setText(message);
		messageView.setVisibility(message != null ? View.VISIBLE : View.GONE);
	}

	public void setButton(int whichButton, CharSequence text, final OnClickListener listener) {
		final int which = whichButton;
		Button button = getButton(whichButton);
		button.setText(text);
		button.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				if (listener != null)
					listener.onClick(AlertDialog.this, which);
				dismiss();
			}
		});
		button.setVisibility(View.VISIBLE);
		buttonBar.setVisibility(View.VISIBLE);
	}

	public Button getButton(int whichButton) {
		// BUTTON_POSITIVE = -1, BUTTON_NEGATIVE = -2, BUTTON_NEUTRAL = -3
		return buttons[-1 - whichButton];
	}

	public void setView(View view) {
		customHolder.removeAllViews();
		if (view != null)
			customHolder.addView(view, new FrameLayout.LayoutParams(
			    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
	}

	public void setItems(CharSequence[] items, final DialogInterface.OnClickListener listener) {
		itemsContainer.removeAllViews();
		if (items == null)
			return;
		for (int i = 0; i < items.length; i++) {
			final int index = i;
			TextView item = new TextView(getContext());
			item.setText(items[i]);
			item.setTextSize(16);
			item.setPadding(dp(24), dp(14), dp(24), dp(14));
			item.setOnClickListener(new View.OnClickListener() {
				@Override
				public void onClick(View v) {
					if (listener != null)
						listener.onClick(AlertDialog.this, index);
					dismiss();
				}
			});
			itemsContainer.addView(item, new LinearLayout.LayoutParams(
			    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
		}
	}

	public static class Builder {
		private AlertDialog dialog;

		public Builder(Context context) {
			dialog = new AlertDialog(context);
		}

		public Builder(Context context, int themeResId) {
			dialog = new AlertDialog(context, themeResId);
		}

		public AlertDialog.Builder setPositiveButton(int textId, DialogInterface.OnClickListener listener) {
			return setPositiveButton(dialog.getContext().getText(textId), listener);
		}

		public AlertDialog.Builder setPositiveButton(CharSequence text, DialogInterface.OnClickListener listener) {
			dialog.setButton(DialogInterface.BUTTON_POSITIVE, text, listener);
			return this;
		}

		public AlertDialog.Builder setNegativeButton(CharSequence text, DialogInterface.OnClickListener listener) {
			dialog.setButton(DialogInterface.BUTTON_NEGATIVE, text, listener);
			return this;
		}

		public AlertDialog.Builder setNegativeButton(int textId, DialogInterface.OnClickListener listener) {
			return setNegativeButton(dialog.getContext().getText(textId), listener);
		}

		public AlertDialog.Builder setNeutralButton(CharSequence text, DialogInterface.OnClickListener listener) {
			dialog.setButton(DialogInterface.BUTTON_NEUTRAL, text, listener);
			return this;
		}

		public AlertDialog.Builder setNeutralButton(int textId, DialogInterface.OnClickListener listener) {
			return setNeutralButton(dialog.getContext().getText(textId), listener);
		}

		public AlertDialog.Builder setCancelable(boolean cancelable) {
			dialog.setCancelable(cancelable);
			return this;
		}

		public AlertDialog.Builder setIcon(int iconId) {
			return this;
		}

		public AlertDialog.Builder setTitle(CharSequence title) {
			dialog.setTitle(title);
			return this;
		}

		public AlertDialog.Builder setTitle(int title) {
			return setTitle(dialog.getContext().getText(title));
		}

		public AlertDialog.Builder setMessage(CharSequence message) {
			dialog.setMessage(message);
			return this;
		}

		public AlertDialog.Builder setMessage(int message) {
			return setMessage(dialog.getContext().getText(message));
		}

		public AlertDialog.Builder setView(View view) {
			dialog.setView(view);
			return this;
		}

		public AlertDialog.Builder setItems(CharSequence[] items, final DialogInterface.OnClickListener listener) {
			dialog.setItems(items, listener);
			return this;
		}

		public AlertDialog.Builder setItems(int itemsId, final DialogInterface.OnClickListener listener) {
			return setItems(dialog.getContext().getResources().getTextArray(itemsId), listener);
		}

		public Builder setOnCancelListener(OnCancelListener onCancelListener) {
			dialog.setOnCancelListener(onCancelListener);
			return this;
		}

		public Builder setOnDismissListener(OnDismissListener onDismissListener) {
			dialog.setOnDismissListener(onDismissListener);
			return this;
		}

		public AlertDialog create() {
			return dialog;
		}

		public AlertDialog show() {
			dialog.show();
			return dialog;
		}
	}
}
