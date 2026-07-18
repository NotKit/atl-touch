package android.webkit;

import android.graphics.Bitmap;

public class WebViewClient {
	public void onPageStarted(WebView view, String url, Bitmap favicon) {}
	public void onPageFinished(WebView view, String url) {}

	public WebResourceResponse shouldInterceptRequest(WebView view, String url) {
		return null;
	}

	public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
		return shouldInterceptRequest(view, request.getUrl().toString());
	}
}
