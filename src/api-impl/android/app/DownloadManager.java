package android.app;

import android.database.Cursor;
import android.net.Uri;
import android.util.Slog;

/**
 * There is no download service under ATL, so every download fails - but the
 * object has to exist: an app that asks for one and gets null usually
 * dereferences it straight away (Google Camera does).
 */
public class DownloadManager {

	private static final String TAG = "DownloadManager";

	public static final String COLUMN_ID = "_id";
	public static final String COLUMN_STATUS = "status";
	public static final String COLUMN_REASON = "reason";
	public static final String COLUMN_LOCAL_URI = "local_uri";
	public static final String COLUMN_TOTAL_SIZE_BYTES = "total_size";
	public static final String COLUMN_BYTES_DOWNLOADED_SO_FAR = "bytes_so_far";

	public static final int STATUS_PENDING = 1 << 0;
	public static final int STATUS_RUNNING = 1 << 1;
	public static final int STATUS_PAUSED = 1 << 2;
	public static final int STATUS_SUCCESSFUL = 1 << 3;
	public static final int STATUS_FAILED = 1 << 4;

	public static final int ERROR_UNKNOWN = 1000;

	public static class Request {
		public Request(Uri uri) {}

		public Request setDestinationUri(Uri uri) { return this; }

		public Request setNotificationVisibility(int visibility) { return this; }

		public Request setTitle(CharSequence title) { return this; }

		public Request setDescription(CharSequence description) { return this; }

		public Request setMimeType(String mimeType) { return this; }

		public Request addRequestHeader(String header, String value) { return this; }

		public Request setAllowedNetworkTypes(int flags) { return this; }
	}

	public static class Query {
		public Query setFilterById(long... ids) { return this; }

		public Query setFilterByStatus(int flags) { return this; }
	}

	public long enqueue(Request request) {
		Slog.w(TAG, "enqueue: there is no download service, the download is dropped");
		return -1;
	}

	public int remove(long... ids) {
		return 0;
	}

	/** null is a documented answer for a query that matches nothing */
	public Cursor query(Query query) {
		return null;
	}

	public Uri getUriForDownloadedFile(long id) {
		return null;
	}
}
