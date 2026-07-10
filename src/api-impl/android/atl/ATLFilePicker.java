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
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * In-scene file picker dialog, the replacement for the GtkFileDialog paths:
 * picking a file (ACTION_OPEN_DOCUMENT / ACTION_GET_CONTENT), choosing a
 * directory and file name to save under (ACTION_CREATE_DOCUMENT), and picking
 * a folder (ACTION_OPEN_DOCUMENT_TREE, ATLMediaContentProvider's media folder).
 *
 * The result is delivered exactly once: with the chosen File, or null if the
 * dialog is canceled in any way (Cancel button, back key).
 */
public class ATLFilePicker extends Dialog {

	// values match Activity.FILE_CHOOSER_ACTIONS order
	public static final int ACTION_OPEN = 0;
	public static final int ACTION_SAVE = 1;
	public static final int ACTION_SELECT_FOLDER = 2;

	public interface ResultListener {
		void onResult(File file);
	}

	private final int action;
	private final ResultListener listener;
	private final float density;

	private final TextView pathView;
	private final LinearLayout entriesView;
	private final EditText nameField;

	private File currentDir;
	private boolean delivered = false;

	public ATLFilePicker(Context context, int action, String title, String initialName, ResultListener listener) {
		super(context);
		this.action = action;
		this.listener = listener;
		density = context.getResources().getDisplayMetrics().density;

		if (title == null)
			title = action == ACTION_SAVE ? "Save File"
			      : action == ACTION_SELECT_FOLDER ? "Select Folder"
			      : "Open File";
		LinearLayout root = new LinearLayout(context);
		root.setOrientation(LinearLayout.VERTICAL);
		// apps' dialogs bring a themed background; this one has to paint its own
		root.setBackgroundColor(Color.WHITE);

		TextView titleView = new TextView(context);
		titleView.setText(title);
		titleView.setTextSize(20);
		titleView.setTypeface(Typeface.DEFAULT_BOLD);
		titleView.setTextColor(Color.BLACK);
		titleView.setPadding(dp(24), dp(20), dp(24), dp(4));
		root.addView(titleView, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

		pathView = new TextView(context);
		pathView.setTextSize(13);
		pathView.setTextColor(Color.GRAY);
		pathView.setPadding(dp(24), 0, dp(24), dp(8));
		root.addView(pathView, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

		entriesView = new LinearLayout(context);
		entriesView.setOrientation(LinearLayout.VERTICAL);
		ScrollView scroll = new ScrollView(context);
		scroll.addView(entriesView, new FrameLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
		// weight 1: the list takes the dialog height not used by title/name/buttons
		root.addView(scroll, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

		if (action == ACTION_SAVE) {
			nameField = new EditText(context);
			nameField.setTextSize(16);
			nameField.setPadding(dp(24), dp(8), dp(24), dp(8));
			if (initialName != null)
				nameField.setText(initialName);
			root.addView(nameField, new LinearLayout.LayoutParams(
			    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
		} else {
			nameField = null;
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
		addButton(buttonBar, cancelButton);

		if (action == ACTION_SAVE) {
			Button saveButton = new Button(context);
			saveButton.setText("Save");
			saveButton.setOnClickListener(new View.OnClickListener() {
				@Override
				public void onClick(View v) {
					String name = nameField.getText().toString().trim();
					if (!name.isEmpty())
						finishWithResult(new File(currentDir, name));
				}
			});
			addButton(buttonBar, saveButton);
		} else if (action == ACTION_SELECT_FOLDER) {
			Button selectButton = new Button(context);
			selectButton.setText("Select this folder");
			selectButton.setOnClickListener(new View.OnClickListener() {
				@Override
				public void onClick(View v) {
					finishWithResult(currentDir);
				}
			});
			addButton(buttonBar, selectButton);
		}
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

		/* Not relying on windowIsFloating theme attributes (the app's theme may
		 * not have them): explicitly a centered, dimmed panel with a fixed size —
		 * the entry list should get real estate. */
		int widthPixels = context.getResources().getDisplayMetrics().widthPixels;
		int heightPixels = context.getResources().getDisplayMetrics().heightPixels;
		LayoutParams lp = getWindow().getAttributes();
		lp.width = widthPixels > 0 ? Math.min(widthPixels * 9 / 10, dp(420)) : dp(420);
		lp.height = heightPixels > 0 ? heightPixels * 3 / 4 : dp(480);
		lp.gravity = Gravity.CENTER;
		lp.flags |= LayoutParams.FLAG_DIM_BEHIND;
		lp.dimAmount = 0.6f;

		String home = System.getenv("HOME");
		if (home == null)
			home = System.getProperty("user.home", "/");
		File start = new File(home);
		navigate(start.isDirectory() ? start : new File("/"));
	}

	private void addButton(LinearLayout buttonBar, Button button) {
		LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
		lp.leftMargin = dp(8);
		buttonBar.addView(button, lp);
	}

	private int dp(int dp) {
		return (int)(dp * density + 0.5f);
	}

	private void deliver(File file) {
		if (!delivered) {
			delivered = true;
			listener.onResult(file);
		}
	}

	private void finishWithResult(File file) {
		deliver(file);
		dismiss();
	}

	private void navigate(File dir) {
		currentDir = dir;
		pathView.setText(dir.getAbsolutePath());
		entriesView.removeAllViews();

		if (dir.getParentFile() != null)
			addEntry("..", dir.getParentFile(), true);

		File[] files = dir.listFiles();
		if (files == null)
			return;
		List<File> dirs = new ArrayList<File>();
		List<File> regular = new ArrayList<File>();
		for (File f : files) {
			if (f.getName().startsWith("."))
				continue;
			(f.isDirectory() ? dirs : regular).add(f);
		}
		Comparator<File> byName = new Comparator<File>() {
			@Override
			public int compare(File a, File b) {
				return a.getName().compareToIgnoreCase(b.getName());
			}
		};
		Collections.sort(dirs, byName);
		Collections.sort(regular, byName);

		for (File f : dirs)
			addEntry(f.getName() + "/", f, true);
		if (action != ACTION_SELECT_FOLDER) {
			for (File f : regular)
				addEntry(f.getName(), f, false);
		}
	}

	private void addEntry(String label, final File file, final boolean isDirectory) {
		TextView entry = new TextView(getContext());
		entry.setText(label);
		entry.setTextSize(16);
		entry.setTextColor(Color.BLACK);
		if (isDirectory)
			entry.setTypeface(Typeface.DEFAULT_BOLD);
		entry.setPadding(dp(24), dp(12), dp(24), dp(12));
		entry.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				if (isDirectory) {
					navigate(file);
				} else if (action == ACTION_SAVE) {
					nameField.setText(file.getName());
				} else {
					finishWithResult(file);
				}
			}
		});
		entriesView.addView(entry, new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
	}
}
