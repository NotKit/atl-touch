package android.webkit;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.util.Base64;
import android.view.ViewGroup;

public class WebView extends ViewGroup {

	private WebViewClient webViewClient;

	/* native WPE WebKit peer (webview_peer*); created lazily once a window/EGL
	 * context exists. 0 until then. */
	private long peer;

	public WebView(Context context) {
		this(context, null);
	}

	public WebView(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	public WebView(Context context, AttributeSet attrs, int defStyleAttr) {
		super(context, attrs, defStyleAttr);
	}

	private long ensurePeer() {
		if (peer == 0) {
			int w = getWidth() > 0 ? getWidth() : 1;
			int h = getHeight() > 0 ? getHeight() : 1;
			peer = native_create(w, h);
		}
		return peer;
	}

	@Override
	protected void onSizeChanged(int w, int h, int oldw, int oldh) {
		super.onSizeChanged(w, h, oldw, oldh);
		if (peer != 0 && w > 0 && h > 0)
			native_setSize(peer, w, h);
	}

	@Override
	public void onDraw(Canvas canvas) {
		long p = ensurePeer();
		if (p != 0)
			native_draw(p, canvas.getNativeCanvasWrapper(), getWidth(), getHeight());
	}

	public WebSettings getSettings() {
		return new WebSettings();
	}

	public void setDownloadListener(DownloadListener downloadListener) {}

	public void setScrollBarStyle(int scrollBarStyle) {}

	public void setWebViewClient(WebViewClient webViewClient) {
		this.webViewClient = webViewClient;
	}

	// to be used by native code
	void internalLoadChanged(int loadState, String url) {
		if (loadState == /*WEBKIT_LOAD_STARTED*/ 0 && webViewClient != null) {
			webViewClient.onPageStarted(this, url);
		} else if (loadState == /*WEBKIT_LOAD_FINISHED*/ 3 && webViewClient != null) {
			webViewClient.onPageFinished(this, url);
		}
	}

	public void setVerticalScrollBarEnabled(boolean enabled) {}
	public void setVerticalScrollbarOverlay(boolean overlay) {}
	public void setInitialScale(int scaleInPercent) {}

	public void addJavascriptInterface(Object object, String name) {
		// HACK: directly call onRenderingDone for OctoDroid, as the javascript interface is not implemented yet
		if (object.getClass().getName().startsWith("com.gh4a") && "NativeClient".equals(name)) {
			try {
				object.getClass().getMethod("onRenderingDone").invoke(object);
			} catch (ReflectiveOperationException e) {
				e.printStackTrace();
			}
		}
	}

	public void setWebChromeClient(WebChromeClient dummy) {}

	public void removeAllViews() {}

	public void destroy() {}

	public void loadDataWithBaseURL(String baseUrl, String data, String mimeType, String encoding, String historyUrl) {
		if ("base64".equals(encoding)) {
			data = new String(Base64.decode(data, 0));
		}
		if (mimeType != null && mimeType.contains(";")) {
			mimeType = mimeType.substring(0, mimeType.indexOf(";"));
		}
		/* WebKit won't let a page fetch the file:// scheme; map the app's asset
		 * URLs onto our android-asset:// scheme (served from the AssetManager by
		 * native code) and load the page at that same origin so the stylesheets
		 * and scripts the card links to count as same-origin subresources. */
		data = data.replace("file:///android_asset/", "android-asset:///");
		native_loadHtml(ensurePeer(), data, "android-asset:///");
	}

	public void loadUrl(String url) {
		if (url.startsWith("javascript:")) {
			System.out.println("loadUrl: " + url + " - not implemented yet");
			return;
		}
		native_loadUrl(ensurePeer(), url);
	}

	public void stopLoading() {}

	// to be used by native code
	AssetManager internalGetAssetManager() {
		return getContext().getResources().getAssets();
	}

	public void onPause() {}

	public void onResume() {}

	public boolean isPaused() {
		return false;
	}

	public void resumeTimers() {}

	public void loadData(String data, String mimeType, String encoding) {
		loadDataWithBaseURL("about:blank", data, mimeType, encoding, "about:blank");
	}

	public void evaluateJavascript(String script, ValueCallback resultCallback) {}

	public static void setWebContentsDebuggingEnabled(boolean enabled) {}

	public void clearFormData() {}

	public void clearHistory() {}

	public void clearMatches() {}

	public void clearSslPreferences() {}

	public void clearCache(boolean includeDiskFiles) {}

	// directly accessed by androidx WebViewGlueCommunicator to get ClassLoader
	private static Object getFactory() {
		return new Object();
	}

	private native long native_create(int width, int height);
	private native void native_setSize(long peer, int width, int height);
	private native void native_loadUrl(long peer, String url);
	private native void native_loadHtml(long peer, String html, String baseUri);
	private native void native_draw(long peer, long canvasPtr, int width, int height);
}
