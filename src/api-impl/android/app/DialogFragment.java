package android.app;

import android.content.DialogInterface;
import android.os.Bundle;

/** Stub: enough for apps that subclass it to link and construct. */
public class DialogFragment extends Fragment implements DialogInterface.OnCancelListener,
		DialogInterface.OnDismissListener {
	public Dialog onCreateDialog(Bundle savedInstanceState) {
		return null;
	}

	public Dialog getDialog() {
		return null;
	}

	public void show(FragmentManager manager, String tag) {}

	public void dismiss() {}

	@Override
	public void onCancel(DialogInterface dialog) {}

	@Override
	public void onDismiss(DialogInterface dialog) {}
}
