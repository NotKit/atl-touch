package android.content;

import android.accounts.Account;
import android.atl.ATLLoadedApp;
import android.content.res.AssetFileDescriptor;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class ContentResolver {
	public static final String SCHEME_CONTENT = "content";
	public static final String SCHEME_ANDROID_RESOURCE = "android.resource";
	public static final String SCHEME_FILE = "file";

	public static final String SYNC_EXTRAS_IGNORE_SETTINGS = "ignore_settings";

	/* getContentResolver() hands out a fresh instance every call, so the
	 * registrations have to live in the class, not the object. */
	private static final ArrayList<Registration> observers = new ArrayList<Registration>();

	private static final class Registration {
		final String uri;
		final boolean notifyForDescendants;
		final ContentObserver observer;

		Registration(Uri uri, boolean notifyForDescendants, ContentObserver observer) {
			this.uri = uri.toString();
			this.notifyForDescendants = notifyForDescendants;
			this.observer = observer;
		}
	}

	public final void registerContentObserver(Uri uri, boolean notifyForDescendants, ContentObserver observer) {
		if (uri == null || observer == null)
			return;
		synchronized (observers) {
			observers.add(new Registration(uri, notifyForDescendants, observer));
		}
	}
	public final void unregisterContentObserver(ContentObserver observer) {
		if (observer == null)
			return;
		synchronized (observers) {
			for (int i = observers.size() - 1; i >= 0; i--) {
				if (observers.get(i).observer == observer)
					observers.remove(i);
			}
		}
	}
	public void notifyChange(Uri uri, ContentObserver observer) {
		if (uri == null)
			return;
		String changed = uri.toString();
		ArrayList<Registration> matched = new ArrayList<Registration>();
		synchronized (observers) {
			for (Registration r : observers) {
				if (matches(r, changed))
					matched.add(r);
			}
		}
		for (Registration r : matched)
			r.observer.dispatchChange(r.observer == observer, uri);
	}

	/* AOSP walks a tree of uri path segments; the shape of it is: an exact hit
	 * always notifies, a change below a registration only with
	 * notifyForDescendants, and a change above one always notifies it. */
	private static boolean matches(Registration r, String changed) {
		if (r.uri.equals(changed))
			return true;
		if (r.notifyForDescendants && changed.startsWith(r.uri + "/"))
			return true;
		return r.uri.startsWith(changed + "/");
	}

	public void notifyChange(Uri uri, ContentObserver observer, boolean syncToNetwork) {
		notifyChange(uri, observer);
	}
	public int getUserId() {
		return 0;
	}
	public final void registerContentObserver(Uri uri, boolean notifyForDescendants, ContentObserver observer, int userHandle) {
		registerContentObserver(uri, notifyForDescendants, observer);
	}

	public ParcelFileDescriptor openFileDescriptor(Uri uri, String mode) throws FileNotFoundException {
		if ("file".equals(uri.getScheme())) {
			return ParcelFileDescriptor.open(new File(uri.getPath()), ParcelFileDescriptor.parseMode(mode));
		} else {
			ContentProvider provider = ContentProvider.atl_get_content_provider(uri.getAuthority());
			if (provider != null)
				return provider.openFile(uri, mode);
			else
				return null;
		}
	}

	public ParcelFileDescriptor openFileDescriptor(Uri uri, String mode, CancellationSignal signal) throws FileNotFoundException {
		return openFileDescriptor(uri, mode);
	}

	public AssetFileDescriptor openAssetFileDescriptor(Uri uri, String mode) throws FileNotFoundException {
		if ("file".equals(uri.getScheme())) {
			return new AssetFileDescriptor(ParcelFileDescriptor.open(new File(uri.getPath()), ParcelFileDescriptor.parseMode(mode)), 0, AssetFileDescriptor.UNKNOWN_LENGTH);
		} else {
			ContentProvider provider = ContentProvider.atl_get_content_provider(uri.getAuthority());
			if (provider != null)
				return provider.openAssetFile(uri, mode);
			else
				return null;
		}
	}

	public AssetFileDescriptor openAssetFileDescriptor(Uri uri, String mode, CancellationSignal signal) throws FileNotFoundException {
		return openAssetFileDescriptor(uri, mode);
	}

	public final AssetFileDescriptor openTypedAssetFileDescriptor(Uri uri, String mimeType, Bundle opts) throws FileNotFoundException {
		/* FIXME */
		return openAssetFileDescriptor(uri, "r");
	}

	public final AssetFileDescriptor openTypedAssetFileDescriptor(Uri uri, String mimeType, Bundle opts, CancellationSignal cancellationSignal) throws FileNotFoundException {
		return openTypedAssetFileDescriptor(uri, mimeType, opts);
	}

	public InputStream openInputStream(Uri uri) throws FileNotFoundException {
		ParcelFileDescriptor fd = openFileDescriptor(uri, "r");
		return fd != null ? new ParcelFileDescriptor.AutoCloseInputStream(fd) : null;
	}

	public OutputStream openOutputStream(Uri uri) throws FileNotFoundException {
		ParcelFileDescriptor fd = openFileDescriptor(uri, "w");
		return fd != null ? new ParcelFileDescriptor.AutoCloseOutputStream(fd) : null;
	}

	public Cursor query(Uri uri, String[] projection, Bundle queryArgs, CancellationSignal cancellationSignal) {
		ContentProvider provider = ContentProvider.atl_get_content_provider(uri.getAuthority());
		if (provider != null) {
			return provider.query(uri, projection, queryArgs, cancellationSignal);
		} else if ("file".equals(uri.getScheme())) {
			MatrixCursor cursor = new MatrixCursor(projection);
			Object[] row = new Object[projection.length];
			native_query_file_info(uri.getPath(), projection, row);
			cursor.addRow(row);
			return cursor;
		} else {
			return null;
		}
	}

	public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
		ContentProvider provider = ContentProvider.atl_get_content_provider(uri.getAuthority());
		if (provider != null) {
			return provider.query(uri, projection, selection, selectionArgs, sortOrder);
		} else if ("file".equals(uri.getScheme())) {
			MatrixCursor cursor = new MatrixCursor(projection);
			Object[] row = new Object[projection.length];
			native_query_file_info(uri.getPath(), projection, row);
			cursor.addRow(row);
			return cursor;
		} else {
			return null;
		}
	}

	public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder, CancellationSignal cancellationSignal) {
		return query(uri, projection, selection, selectionArgs, sortOrder);
	}

	public int delete(Uri uri, String selection, String[] selectionArgs) {
		ContentProvider provider = ContentProvider.atl_get_content_provider(uri.getAuthority());
		if (provider != null)
			return provider.delete(uri, selection, selectionArgs);
		else
			return 0;
	}

	public Uri insert(Uri uri, ContentValues values) {
		ContentProvider provider = ContentProvider.atl_get_content_provider(uri.getAuthority());
		if (provider != null)
			return provider.insert(uri, values);
		else
			return null;
	}

	/** The data types a provider can convert the uri to; ATL's providers offer none. */
	public String[] getStreamTypes(Uri uri, String mimeTypeFilter) {
		return null;
	}

	public String getType(Uri uri) {
		ContentProvider provider = ContentProvider.atl_get_content_provider(uri.getAuthority());
		if (provider != null)
			return provider.getType(uri);
		else
			return null;
	}

	public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
		ContentProvider provider = ContentProvider.atl_get_content_provider(uri.getAuthority());
		if (provider != null)
			return provider.update(uri, values, selection, selectionArgs);
		else
			return 0;
	}

	public static final String QUERY_ARG_SQL_SELECTION = "android:query-arg-sql-selection";
	public static final String QUERY_ARG_SQL_SELECTION_ARGS = "android:query-arg-sql-selection-args";
	public static final String QUERY_ARG_SQL_SORT_ORDER = "android:query-arg-sql-sort-order";
	public static final String QUERY_ARG_SORT_COLUMNS = "android:query-arg-sort-columns";
	public static final String QUERY_ARG_SORT_DIRECTION = "android:query-arg-sort-direction";
	public static final String QUERY_ARG_SORT_COLLATION = "android:query-arg-sort-collation";
	public static final String QUERY_ARG_LIMIT = "android:query-arg-limit";
	public static final String QUERY_ARG_OFFSET = "android:query-arg-offset";
	public static final String QUERY_ARG_SQL_LIMIT = "android:query-arg-sql-limit";
	public static final int QUERY_SORT_DIRECTION_ASCENDING = 0;
	public static final int QUERY_SORT_DIRECTION_DESCENDING = 1;

	/* API 30's bundle form; ATL's providers take the selection args directly */
	public int update(Uri uri, ContentValues values, Bundle extras) {
		String selection = extras == null ? null : extras.getString(QUERY_ARG_SQL_SELECTION);
		String[] args = extras == null ? null : extras.getStringArray(QUERY_ARG_SQL_SELECTION_ARGS);

		return update(uri, values, selection, args);
	}

	/**
	 * Apply a batch of operations to one authority's provider. They are applied
	 * in order and each one's result is reported; ATL has no transaction to roll
	 * back, so a failure leaves the earlier ones applied.
	 */
	public ContentProviderResult[] applyBatch(String authority,
	    ArrayList<ContentProviderOperation> operations)
	    throws OperationApplicationException {
		ContentProvider provider = ContentProvider.atl_get_content_provider(authority);

		if (provider == null)
			throw new OperationApplicationException("no provider for " + authority);

		return provider.applyBatch(authority, operations);
	}

	public final Bundle call(Uri uri, String method, String arg, Bundle extras) {
		return call(uri.getAuthority(), method, arg, extras);
	}

	public final Bundle call(String authority, String method, String arg, Bundle extras) {
		ContentProvider provider = ContentProvider.atl_get_content_provider(authority);
		if (provider == null)
			throw new IllegalArgumentException("Unknown authority " + authority);
		return provider.call(authority, method, arg, extras);
	}

	public List getPersistedUriPermissions() {
		return Collections.emptyList();
	}

	public final ContentProviderClient acquireContentProviderClient(Uri uri) {
		return acquireContentProviderClient(uri.getAuthority());
	}

	public final ContentProviderClient acquireContentProviderClient(String name) {
		ContentProvider provider = ContentProvider.atl_get_content_provider(name);
		return provider != null ? new ContentProviderClient(provider) : null;
	}

	public final ContentProviderClient acquireUnstableContentProviderClient(Uri uri) {
		return acquireContentProviderClient(uri);
	}

	public final ContentProviderClient acquireUnstableContentProviderClient(String name) {
		return acquireContentProviderClient(name);
	}

	public static void requestSync(final Account account, final String authority, final Bundle extras) {
		// bind the app's sync adapter service and run the sync on a separate thread, like the SyncManager would
		ATLLoadedApp app = ATLLoadedApp.getPrimaryApplication();
		Intent intent = new Intent("android.content.SyncAdapter");
		intent.setPackage(app.pkg.packageName);
		ComponentName component = app.startOrBindService(intent, new ServiceConnection() {
			@Override
			public void onServiceConnected(ComponentName name, IBinder service) {
				if (!(service instanceof AbstractThreadedSyncAdapter.SyncAdapterBinder)) {
					Log.w("ContentResolver", "requestSync: sync adapter service returned " + service);
					return;
				}
				final AbstractThreadedSyncAdapter adapter = ((AbstractThreadedSyncAdapter.SyncAdapterBinder)service).getSyncAdapter();
				new Thread(new Runnable() {
					@Override
					public void run() {
						ContentProvider provider = ContentProvider.atl_get_content_provider(authority);
						SyncResult syncResult = new SyncResult();
						try {
							adapter.onPerformSync(account, extras != null ? extras : new Bundle(), authority,
							                      provider != null ? new ContentProviderClient(provider) : null, syncResult);
							Log.i("ContentResolver", "requestSync: sync for " + authority + " finished: " + syncResult);
						} catch (Throwable t) {
							Log.e("ContentResolver", "requestSync: sync for " + authority + " failed", t);
						}
					}
				}, "SyncAdapterThread").start();
			}
		});
		if (component == null)
			Log.w("ContentResolver", "requestSync: no sync adapter service found in " + app.pkg.packageName);
	}

	public static void cancelSync(Account account, String authority) {
	}

	public static void setMasterSyncAutomatically(boolean sync) {
	}

	public static boolean getMasterSyncAutomatically() {
		return true;
	}

	public static int getIsSyncable(Account account, String authority) {
		return 1;
	}

	public static boolean getSyncAutomatically(Account account, String authority) {
		return false;
	}

	public static void setSyncAutomatically(Account account, String authority, boolean sync) {
	}

	public static void addPeriodicSync(Account account, String authority, Bundle extras, long pollFrequency) {
	}

	public static void setIsSyncable(Account account, String authority, int syncable) {
	}

	public static boolean isSyncActive(Account account, String authority) {
		return false;
	}

	private static native void native_query_file_info(String path, String[] attributes, Object[] result);
}
