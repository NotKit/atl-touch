package android.print;

import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;

/**
 * Printing is not wired up under ATL; this exists so callers that subclass the
 * adapter compile and link. The callbacks are never invoked.
 */
public abstract class PrintDocumentAdapter {
	public static final String EXTRA_PRINT_PREVIEW = "EXTRA_PRINT_PREVIEW";

	public static abstract class LayoutResultCallback {
		public void onLayoutCancelled() {}

		public void onLayoutFailed(CharSequence error) {}

		public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {}
	}

	public static abstract class WriteResultCallback {
		public void onWriteCancelled() {}

		public void onWriteFailed(CharSequence error) {}

		public void onWriteFinished(PageRange[] pages) {}
	}

	public void onStart() {}

	public abstract void onLayout(PrintAttributes oldAttributes, PrintAttributes newAttributes,
	    CancellationSignal cancellationSignal, LayoutResultCallback callback, Bundle extras);

	public abstract void onWrite(PageRange[] pages, ParcelFileDescriptor destination,
	    CancellationSignal cancellationSignal, WriteResultCallback callback);

	public void onFinish() {}
}
