package android.view;

/**
 * Listener for receiving rich content. ATL never delivers content to it, but
 * apps register one (e.g. via androidx ViewCompat) during view setup.
 */
public interface OnReceiveContentListener {
	ContentInfo onReceiveContent(View view, ContentInfo payload);
}
