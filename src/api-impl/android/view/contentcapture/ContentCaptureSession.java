package android.view.contentcapture;

/* Content capture is a system-service feature we don't provide; the class exists
 * only so that View.getContentCaptureSession() can be declared (androidx and
 * Compose both treat a null session as "content capture disabled"). */
public abstract class ContentCaptureSession implements AutoCloseable {

	@Override
	public void close() {}
}
