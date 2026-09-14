package android.content;

import android.net.Uri;

/** What one {@link ContentProviderOperation} of a batch did: a uri or a count. */
public class ContentProviderResult {

	public final Uri uri;
	public final Integer count;

	public ContentProviderResult(Uri uri) {
		this.uri = uri;
		this.count = null;
	}

	public ContentProviderResult(int count) {
		this.uri = null;
		this.count = Integer.valueOf(count);
	}

	@Override
	public String toString() {
		return uri != null ? "ContentProviderResult(uri=" + uri + ")"
		                   : "ContentProviderResult(count=" + count + ")";
	}
}
