package android.webkit;

import java.io.InputStream;
import java.util.Map;

public class WebResourceResponse {
	private String mimeType;
	private String encoding;
	private int statusCode = 200;
	private String reasonPhrase = "OK";
	private Map<String, String> responseHeaders;
	private InputStream data;

	public WebResourceResponse(String mimeType, String encoding, InputStream data) {
		this.mimeType = mimeType;
		this.encoding = encoding;
		this.data = data;
	}

	public WebResourceResponse(String mimeType, String encoding, int statusCode,
			String reasonPhrase, Map<String, String> responseHeaders, InputStream data) {
		this(mimeType, encoding, data);
		this.statusCode = statusCode;
		this.reasonPhrase = reasonPhrase;
		this.responseHeaders = responseHeaders;
	}

	public void setMimeType(String mimeType) {
		this.mimeType = mimeType;
	}

	public String getMimeType() {
		return mimeType;
	}

	public void setEncoding(String encoding) {
		this.encoding = encoding;
	}

	public String getEncoding() {
		return encoding;
	}

	public void setStatusCodeAndReasonPhrase(int statusCode, String reasonPhrase) {
		this.statusCode = statusCode;
		this.reasonPhrase = reasonPhrase;
	}

	public int getStatusCode() {
		return statusCode;
	}

	public String getReasonPhrase() {
		return reasonPhrase;
	}

	public void setResponseHeaders(Map<String, String> headers) {
		this.responseHeaders = headers;
	}

	public Map<String, String> getResponseHeaders() {
		return responseHeaders;
	}

	public void setData(InputStream data) {
		this.data = data;
	}

	public InputStream getData() {
		return data;
	}
}
