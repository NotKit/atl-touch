package android.provider;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import java.io.FileNotFoundException;

/**
 * Settings-search indexing provider. Nothing under ATL indexes anything, so this
 * only has to exist and be constructible: apps declare a subclass in their
 * manifest and ATL instantiates every declared provider at startup.
 *
 * The query methods are not abstract on purpose - a subclass that overrides only
 * some of them must stay instantiable.
 */
public class SearchIndexablesProvider extends ContentProvider {

	@Override
	public boolean onCreate() {
		return true;
	}

	public Cursor queryXmlResources(String[] projection) {
		return null;
	}

	public Cursor queryRawData(String[] projection) {
		return null;
	}

	public Cursor queryNonIndexableKeys(String[] projection) {
		return null;
	}

	public Cursor querySiteMapPairs() {
		return null;
	}

	public Cursor querySliceUriPairs() {
		return null;
	}

	public Cursor queryDynamicRawData(String[] projection) {
		return null;
	}

	@Override
	public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
		return null;
	}

	@Override
	public Uri insert(Uri uri, ContentValues values) {
		throw new UnsupportedOperationException("insert on a search-indexables provider");
	}

	@Override
	public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
		throw new UnsupportedOperationException("update on a search-indexables provider");
	}

	@Override
	public int delete(Uri uri, String selection, String[] selectionArgs) {
		throw new UnsupportedOperationException("delete on a search-indexables provider");
	}

	@Override
	public String getType(Uri uri) {
		return null;
	}

	@Override
	public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
		throw new FileNotFoundException("no files in a search-indexables provider: " + uri);
	}
}
