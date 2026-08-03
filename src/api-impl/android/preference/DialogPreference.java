package android.preference;

import android.content.Context;
import android.content.DialogInterface;
import android.util.AttributeSet;
import android.view.View;

/** Stub, see {@link PreferenceFragment}: apps only need it to link. */
public class DialogPreference extends Preference implements DialogInterface.OnClickListener {
	public DialogPreference(Context context, AttributeSet attrs) {}

	public DialogPreference(Context context, AttributeSet attrs, int defStyleAttr) {}

	@Override
	public void onClick(DialogInterface dialog, int which) {}

	protected View onCreateDialogView() {
		return null;
	}

	protected void onBindDialogView(View view) {}

	protected void onDialogClosed(boolean positiveResult) {}

	public void setDialogTitle(CharSequence dialogTitle) {}
}
