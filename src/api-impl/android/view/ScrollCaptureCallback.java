package android.view;

import android.graphics.Rect;
import android.os.CancellationSignal;
import java.util.function.Consumer;

// API 31 views expose this type even though ATL does not initiate scroll capture.
public interface ScrollCaptureCallback {
	void onScrollCaptureSearch(CancellationSignal signal, Consumer<Rect> onReady);
	void onScrollCaptureStart(ScrollCaptureSession session, CancellationSignal signal, Runnable onReady);
	void onScrollCaptureImageRequest(ScrollCaptureSession session, CancellationSignal signal,
	                                Rect captureArea, Consumer<Rect> onComplete);
	void onScrollCaptureEnd(Runnable onReady);
}
