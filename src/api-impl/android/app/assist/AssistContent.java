package android.app.assist;

import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

/**
 * The values an activity offers to an assistant, in
 * Activity.onProvideAssistContent(). Nothing here has an assistant, so nothing
 * ever reads them back; this collects what the app sets and hands it back.
 *
 * It exists because the type has to resolve: Fenix's HomeActivity and
 * ExternalAppBrowserActivity both override
 * onProvideAssistContent(AssistContent) and call super, so without the class -
 * and without Activity's method below - that call is a NoSuchMethodError.
 *
 * Not Parcelable, unlike the SDK's: nothing marshals one here.
 */
public class AssistContent {

	private Intent intent;
	private boolean appProvidedIntent;
	private ClipData clipData;
	private String structuredData;
	private Uri webUri;
	private boolean appProvidedWebUri;
	private final Bundle extras = new Bundle();

	public void setIntent(Intent intent) {
		this.appProvidedIntent = true;
		this.intent = intent;
	}

	public Intent getIntent() { return intent; }

	public boolean isAppProvidedIntent() { return appProvidedIntent; }

	public void setClipData(ClipData clip) { this.clipData = clip; }

	public ClipData getClipData() { return clipData; }

	public void setStructuredData(String structuredData) { this.structuredData = structuredData; }

	public String getStructuredData() { return structuredData; }

	public void setWebUri(Uri uri) {
		this.appProvidedWebUri = true;
		this.webUri = uri;
	}

	public Uri getWebUri() { return webUri; }

	public boolean isAppProvidedWebUri() { return appProvidedWebUri; }

	public Bundle getExtras() { return extras; }
}
