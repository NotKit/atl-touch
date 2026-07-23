package android.atl;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager.LayoutParams;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * Source-app chooser for a content-hub import (Gallery, Files, ...). content-hub
 * enumerates the peers; the UI is ours. Styled like {@link ATLFilePicker}.
 *
 * The result is delivered exactly once: the chosen peer id, or null if canceled.
 */
public class ATLContentHubPicker extends Dialog {

	public interface ResultListener {
		void onResult(String peerId);
	}

	private final ResultListener listener;
	private final float density;
	private boolean delivered = false;

	public ATLContentHubPicker(Context context, String title, ATLContentHub.Peer[] peers,
	                           ResultListener listener) {
		super(context);
		this.listener = listener;
		density = context.getResources().getDisplayMetrics().density;

		if (title == null)
			title = "Choose Source";
		LinearLayout root = new LinearLayout(context);
		root.setOrientation(LinearLayout.VERTICAL);
		// apps' dialogs bring a themed background; this one has to paint its own
		root.setBackgroundColor(Color.WHITE);

		TextView titleView = new TextView(context);
		titleView.setText(title);
		titleView.setTextSize(20);
		titleView.setTypeface(Typeface.DEFAULT_BOLD);
		titleView.setTextColor(Color.BLACK);
		titleView.setPadding(dp(24), dp(20), dp(24), dp(12));
		root.addView(titleView, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

		LinearLayout entriesView = new LinearLayout(context);
		entriesView.setOrientation(LinearLayout.VERTICAL);
		ScrollView scroll = new ScrollView(context);
		scroll.addView(entriesView, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
		root.addView(scroll, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

		if (peers != null) {
			for (ATLContentHub.Peer peer : peers)
				addEntry(entriesView, peer);
		}

		LinearLayout buttonBar = new LinearLayout(context);
		buttonBar.setOrientation(LinearLayout.HORIZONTAL);
		buttonBar.setGravity(Gravity.RIGHT | Gravity.CENTER_VERTICAL);
		buttonBar.setPadding(dp(8), dp(8), dp(8), dp(8));

		Button cancelButton = new Button(context);
		cancelButton.setText("Cancel");
		cancelButton.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				dismiss();
			}
		});
		LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
		blp.leftMargin = dp(8);
		buttonBar.addView(cancelButton, blp);
		root.addView(buttonBar, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

		setContentView(root);
		setCanceledOnTouchOutside(false);
		setOnDismissListener(new DialogInterface.OnDismissListener() {
			@Override
			public void onDismiss(DialogInterface dialog) {
				deliver(null);
			}
		});

		int widthPixels = context.getResources().getDisplayMetrics().widthPixels;
		int heightPixels = context.getResources().getDisplayMetrics().heightPixels;
		LayoutParams lp = getWindow().getAttributes();
		lp.width = widthPixels > 0 ? Math.min(widthPixels * 9 / 10, dp(420)) : dp(420);
		lp.height = heightPixels > 0 ? Math.min(heightPixels * 3 / 4, dp(480)) : dp(480);
		lp.gravity = Gravity.CENTER;
		lp.flags |= LayoutParams.FLAG_DIM_BEHIND;
		lp.dimAmount = 0.6f;
	}

	private void addEntry(LinearLayout entriesView, final ATLContentHub.Peer peer) {
		TextView entry = new TextView(getContext());
		entry.setText(peer.name);
		entry.setTextSize(16);
		entry.setTextColor(Color.BLACK);
		entry.setPadding(dp(24), dp(14), dp(24), dp(14));
		entry.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				deliver(peer.id);
				dismiss();
			}
		});
		entriesView.addView(entry, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
	}

	private int dp(int dp) {
		return (int)(dp * density + 0.5f);
	}

	private void deliver(String peerId) {
		if (!delivered) {
			delivered = true;
			listener.onResult(peerId);
		}
	}
}
