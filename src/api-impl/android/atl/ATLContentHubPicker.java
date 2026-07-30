package android.atl;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager.LayoutParams;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.Locale;

/**
 * Source-app chooser for a content-hub import (Gallery, Files, Camera, ...).
 * content-hub enumerates the peers and carries their icons; the UI is ours, and
 * mirrors Lomiri's own ContentPeerPicker — a full-screen page with a close
 * button and a grid of app icons — so it doesn't read as a foreign dialog.
 *
 * The result is delivered exactly once: the chosen peer id, or null if canceled.
 */
public class ATLContentHubPicker extends Dialog {

	public interface ResultListener {
		void onResult(String peerId);
	}

	private static final int COLOR_TEXT = 0xff1d1d1d;
	private static final int COLOR_MUTED = 0xff888888;
	private static final int COLOR_DIVIDER = 0xffe0e0e0;

	private final ResultListener listener;
	private final float density;
	private boolean delivered = false;

	public ATLContentHubPicker(Context context, String title, ATLContentHub.Peer[] peers,
	                           ResultListener listener) {
		super(context);
		this.listener = listener;
		density = context.getResources().getDisplayMetrics().density;

		if (title == null)
			title = "Choose from";
		LinearLayout root = new LinearLayout(context);
		root.setOrientation(LinearLayout.VERTICAL);
		// apps' dialogs bring a themed background; this one has to paint its own
		root.setBackgroundColor(Color.WHITE);

		root.addView(header(title), new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, dp(56)));
		root.addView(divider(), new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, Math.max(1, dp(1))));

		TextView section = new TextView(context);
		section.setText("Apps");
		section.setTextSize(15);
		section.setTextColor(COLOR_MUTED);
		section.setPadding(dp(16), dp(10), dp(16), dp(10));
		root.addView(section, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
		root.addView(divider(), new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, Math.max(1, dp(1))));

		IconGrid grid = new IconGrid(context, dp(96));
		grid.setPadding(dp(8), dp(12), dp(8), dp(12));
		ScrollView scroll = new ScrollView(context);
		scroll.addView(grid, new ViewGroup.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
		// weight 1: the grid takes whatever height the header doesn't
		root.addView(scroll, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

		fillGrid(grid, peers);

		setContentView(root);
		setCanceledOnTouchOutside(false);
		setOnDismissListener(new DialogInterface.OnDismissListener() {
			@Override
			public void onDismiss(DialogInterface dialog) {
				deliver(null);
			}
		});

		LayoutParams lp = getWindow().getAttributes();
		lp.width = ViewGroup.LayoutParams.MATCH_PARENT;
		lp.height = ViewGroup.LayoutParams.MATCH_PARENT;
		lp.gravity = Gravity.CENTER;
	}

	private View header(String title) {
		LinearLayout bar = new LinearLayout(getContext());
		bar.setOrientation(LinearLayout.HORIZONTAL);
		bar.setGravity(Gravity.CENTER_VERTICAL);
		bar.setPadding(dp(16), 0, dp(16), 0);

		View close = new CloseIcon(getContext());
		close.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				dismiss();
			}
		});
		bar.addView(close, new LinearLayout.LayoutParams(dp(24), dp(24)));

		TextView titleView = new TextView(getContext());
		titleView.setText(title);
		titleView.setTextSize(22);
		titleView.setTextColor(COLOR_TEXT);
		LinearLayout.LayoutParams tlp = new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
		tlp.leftMargin = dp(20);
		bar.addView(titleView, tlp);
		return bar;
	}

	private View divider() {
		View line = new View(getContext());
		line.setBackgroundColor(COLOR_DIVIDER);
		return line;
	}

	private void fillGrid(IconGrid grid, ATLContentHub.Peer[] peers) {
		if (peers == null || peers.length == 0) {
			TextView empty = new TextView(getContext());
			empty.setText("No app can supply this content.");
			empty.setTextSize(16);
			empty.setTextColor(COLOR_MUTED);
			empty.setPadding(dp(16), dp(24), dp(16), dp(24));
			grid.addView(empty);
			return;
		}
		for (ATLContentHub.Peer peer : peers)
			grid.addView(cell(peer));
	}

	/** Equal-width cells, as many columns as fit, filled left to right. */
	private static final class IconGrid extends ViewGroup {
		private final int minCellWidth;
		private int columns = 1;
		private int rowHeight = 0;

		IconGrid(Context context, int minCellWidth) {
			super(context);
			this.minCellWidth = minCellWidth;
		}

		@Override
		protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
			int width = MeasureSpec.getSize(widthMeasureSpec) - getPaddingLeft() - getPaddingRight();
			columns = Math.max(1, width / minCellWidth);
			int cellWidth = width / columns;

			rowHeight = 0;
			int childHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
			for (int i = 0; i < getChildCount(); i++) {
				View child = getChildAt(i);
				child.measure(MeasureSpec.makeMeasureSpec(cellWidth, MeasureSpec.EXACTLY), childHeightSpec);
				rowHeight = Math.max(rowHeight, child.getMeasuredHeight());
			}
			int rows = (getChildCount() + columns - 1) / columns;
			setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec),
			    rows * rowHeight + getPaddingTop() + getPaddingBottom());
		}

		@Override
		protected void onLayout(boolean changed, int l, int t, int r, int b) {
			int cellWidth = (getWidth() - getPaddingLeft() - getPaddingRight()) / columns;
			for (int i = 0; i < getChildCount(); i++) {
				int x = getPaddingLeft() + (i % columns) * cellWidth;
				int y = getPaddingTop() + (i / columns) * rowHeight;
				getChildAt(i).layout(x, y, x + cellWidth, y + rowHeight);
			}
		}
	}

	private View cell(final ATLContentHub.Peer peer) {
		LinearLayout cell = new LinearLayout(getContext());
		cell.setOrientation(LinearLayout.VERTICAL);
		cell.setGravity(Gravity.CENTER_HORIZONTAL);
		cell.setPadding(dp(4), dp(12), dp(4), dp(12));

		int size = dp(64);
		PeerIcon icon = new PeerIcon(getContext(), ATLContentHub.iconBitmap(peer, size), peer.name);
		cell.addView(icon, new LinearLayout.LayoutParams(size, size));

		TextView label = new TextView(getContext());
		label.setText(peer.name);
		label.setTextSize(15);
		label.setTextColor(COLOR_TEXT);
		label.setGravity(Gravity.CENTER_HORIZONTAL);
		LinearLayout.LayoutParams llp = new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
		llp.topMargin = dp(6);
		cell.addView(label, llp);

		cell.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				deliver(peer.id);
				dismiss();
			}
		});
		return cell;
	}

	/** The peer's icon, or a lettered tile when it couldn't be decoded. */
	private static final class PeerIcon extends View {
		private final Bitmap bitmap;
		private final String letter;
		private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
		private final RectF bounds = new RectF();
		private final Path shape = new Path();

		PeerIcon(Context context, Bitmap bitmap, String name) {
			super(context);
			this.bitmap = bitmap;
			this.letter = name == null || name.isEmpty()
			    ? "?" : name.substring(0, 1).toUpperCase(Locale.ROOT);
		}

		@Override
		public void onDraw(Canvas canvas) {
			bounds.set(0, 0, getWidth(), getHeight());
			// app icons come as plain squares; Lomiri rounds them off itself
			float radius = getWidth() / 6f;
			shape.reset();
			shape.addRoundRect(bounds, radius, radius, Path.Direction.CW);
			if (bitmap != null) {
				canvas.save();
				canvas.clipPath(shape);
				canvas.drawBitmap(bitmap, null, bounds, paint);
				canvas.restore();
				return;
			}
			paint.setColor(0xffd6d6d6);
			canvas.drawPath(shape, paint);
			paint.setColor(0xff5a5a5a);
			paint.setTextSize(getHeight() * 0.5f);
			paint.setTextAlign(Paint.Align.CENTER);
			Paint.FontMetrics fm = paint.getFontMetrics();
			canvas.drawText(letter, getWidth() / 2f,
			    (getHeight() - fm.ascent - fm.descent) / 2f, paint);
		}
	}

	/** The header's close button: two crossed strokes, like Lomiri's. */
	private static final class CloseIcon extends View {
		private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

		CloseIcon(Context context) {
			super(context);
			paint.setColor(0xff5a5a5a);
			paint.setStrokeWidth(context.getResources().getDisplayMetrics().density * 1.5f);
			setClickable(true);
		}

		@Override
		public void onDraw(Canvas canvas) {
			float pad = getWidth() * 0.15f;
			canvas.drawLine(pad, pad, getWidth() - pad, getHeight() - pad, paint);
			canvas.drawLine(getWidth() - pad, pad, pad, getHeight() - pad, paint);
		}
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
