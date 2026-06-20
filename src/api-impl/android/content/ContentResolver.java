package android.content;

import android.accounts.Account;
import android.content.res.AssetFileDescriptor;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class ContentResolver {
	public static final String SCHEME_CONTENT = "content";

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
		if ("file".equals(uri.getScheme())) {
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

	public List getPersistedUriPermissions() {
		return Collections.emptyList();
	}

	public static void requestSync(Account account, String authority, Bundle extras) {
	}

	public static void cancelSync(Account account, String authority) {
	}

	public static void setMasterSyncAutomatically(boolean sync) {
	}

	public static boolean isSyncActive(Account account, String authority) {
		return false;
	}

	private static native void native_query_file_info(String path, String[] attributes, Object[] result);
}
