package android.content;

import android.net.Uri;
import android.os.Handler;

public class AsyncQueryHandler extends Handler {

	public AsyncQueryHandler(ContentResolver cr) {}

	public void startQuery(int token, Object cookie, Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {}
}
