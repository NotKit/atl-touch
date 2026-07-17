package android.webkit;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.util.Base64;
import android.view.MotionEvent;
import android.view.ViewGroup;

import java.util.HashMap;
import java.util.Map;

public class WebView extends ViewGroup {

	private WebViewClient webViewClient;

	/* native backend peer (webview_host_peer*); created lazily once a
	 * window/EGL context exists. 0 until then. */
	private long peer;
	private boolean destroyed = false;

	/* pending evaluateJavascript callbacks by id (id 0 = no callback) */
	private long nextJsCallbackId = 1;
	private final Map<Long, ValueCallback<String>> jsCallbacks = new HashMap<>();

	public WebView(Context context) {
		this(context, null);
	}

	public WebView(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	public WebView(Context context, AttributeSet attrs, int defStyleAttr) {
		super(context, attrs, defStyleAttr);
	}

	/* size last pushed to the native WPE backend; the peer is often created at
	 * 1x1 (loadData runs before the view is laid out), so we must resize it once
	 * a real size is known — otherwise WebKit renders a 1x1 page that gets scaled
	 * up to fill the view, i.e. one stretched pixel (a blank-looking fill). */
	private int peerW = -1, peerH = -1;

	private long ensurePeer() {
		if (peer == 0 && !destroyed) {
			int w = getWidth() > 0 ? getWidth() : 1;
			int h = getHeight() > 0 ? getHeight() : 1;
			peer = native_create(w, h);
			peerW = w;
			peerH = h;
		}
		return peer;
	}

	/* keep the WPE backend sized to the view; safe to call every frame */
	private void syncPeerSize() {
		int w = getWidth(), h = getHeight();
		if (peer != 0 && w > 0 && h > 0 && (w != peerW || h != peerH)) {
			native_setSize(peer, w, h);
			peerW = w;
			peerH = h;
		}
	}

	@Override
	protected void onSizeChanged(int w, int h, int oldw, int oldh) {
		super.onSizeChanged(w, h, oldw, oldh);
		syncPeerSize();
	}

	@Override
	public void onDraw(Canvas canvas) {
		long p = ensurePeer();
		if (p != 0) {
			syncPeerSize();
			native_draw(p, canvas.getNativeCanvasWrapper(), getWidth(), getHeight());
		}
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

	// to be used by native code
	void internalJsResult(long callbackId, String resultJson) {
		ValueCallback<String> callback = jsCallbacks.remove(callbackId);
		if (callback != null) {
			callback.onReceiveValue(resultJson != null ? resultJson : "null");
		}
	}

	@Override
	public boolean onTouchEvent(MotionEvent event) {
		long p = ensurePeer();
		if (p != 0 && native_motionEvent(p, event.getAction(), event.getX(), event.getY(), event.getEventTime())) {
			return true;
		}
		return super.onTouchEvent(event);
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

	public void destroy() {
		destroyed = true;
		if (peer != 0) {
			native_destroy(peer);
			peer = 0;
		}
		jsCallbacks.clear();
	}

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
			evaluateJavascript(url.substring("javascript:".length()), null);
			return;
		}
		long p = ensurePeer();
		if (p != 0)
			native_loadUrl(p, url);
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

	public void evaluateJavascript(String script, ValueCallback<String> resultCallback) {
		long p = ensurePeer();
		long callbackId = 0;
		if (resultCallback != null) {
			callbackId = nextJsCallbackId++;
			jsCallbacks.put(callbackId, resultCallback);
		}
		if (p == 0 || !native_runJs(p, script, callbackId)) {
			internalJsResult(callbackId, null);
		}
	}

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
	private native void native_destroy(long peer);
	private native void native_setSize(long peer, int width, int height);
	private native void native_loadUrl(long peer, String url);
	private native void native_loadHtml(long peer, String html, String baseUri);
	private native boolean native_motionEvent(long peer, int action, float x, float y, long timeMs);
	private native boolean native_runJs(long peer, String script, long callbackId);
	private native void native_draw(long peer, long canvasPtr, int width, int height);
}
