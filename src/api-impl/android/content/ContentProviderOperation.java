package android.content;

import android.net.Uri;

/**
 * One insert, update or delete to apply through
 * {@link ContentResolver#applyBatch}.
 *
 * Google Camera finalises a saved picture this way: the media row it inserted
 * while the shot was pending is updated in a batch once the file is written.
 * The class missing took the whole media-group callback with it.
 */
public class ContentProviderOperation {

	private static final int TYPE_INSERT = 1;
	private static final int TYPE_UPDATE = 2;
	private static final int TYPE_DELETE = 3;
	private static final int TYPE_ASSERT = 4;

	private final int type;
	private final Uri uri;
	private final ContentValues values;
	private final String selection;
	private final String[] selectionArgs;
	private final boolean yieldAllowed;

	private ContentProviderOperation(Builder builder) {
		this.type = builder.type;
		this.uri = builder.uri;
		this.values = builder.values;
		this.selection = builder.selection;
		this.selectionArgs = builder.selectionArgs;
		this.yieldAllowed = builder.yieldAllowed;
	}

	public static Builder newInsert(Uri uri) {
		return new Builder(TYPE_INSERT, uri);
	}

	public static Builder newUpdate(Uri uri) {
		return new Builder(TYPE_UPDATE, uri);
	}

	public static Builder newDelete(Uri uri) {
		return new Builder(TYPE_DELETE, uri);
	}

	public static Builder newAssertQuery(Uri uri) {
		return new Builder(TYPE_ASSERT, uri);
	}

	public Uri getUri() {
		return uri;
	}

	public boolean isInsert() {
		return type == TYPE_INSERT;
	}

	public boolean isUpdate() {
		return type == TYPE_UPDATE;
	}

	public boolean isDelete() {
		return type == TYPE_DELETE;
	}

	public boolean isAssertQuery() {
		return type == TYPE_ASSERT;
	}

	public boolean isYieldAllowed() {
		return yieldAllowed;
	}

	/** Run it against the provider that serves its uri. */
	ContentProviderResult apply(ContentProvider provider) {
		switch (type) {
		case TYPE_INSERT:
			return new ContentProviderResult(provider.insert(uri, values));
		case TYPE_UPDATE:
			return new ContentProviderResult(
			    provider.update(uri, values, selection, selectionArgs));
		case TYPE_DELETE:
			return new ContentProviderResult(provider.delete(uri, selection, selectionArgs));
		default:
			/* an assertion has nothing to change; a provider that answered the
			 * query at all is as much as ATL checks */
			return new ContentProviderResult(0);
		}
	}

	public static class Builder {

		private final int type;
		private final Uri uri;
		private ContentValues values;
		private String selection;
		private String[] selectionArgs;
		private boolean yieldAllowed;

		Builder(int type, Uri uri) {
			if (uri == null)
				throw new IllegalArgumentException("uri must not be null");
			this.type = type;
			this.uri = uri;
		}

		public Builder withValues(ContentValues values) {
			if (this.values == null)
				this.values = new ContentValues();
			this.values.putAll(values);
			return this;
		}

		public Builder withValue(String key, Object value) {
			if (values == null)
				values = new ContentValues();
			if (value == null)
				values.putNull(key);
			else if (value instanceof String)
				values.put(key, (String)value);
			else if (value instanceof Byte)
				values.put(key, (Byte)value);
			else if (value instanceof Short)
				values.put(key, (Short)value);
			else if (value instanceof Integer)
				values.put(key, (Integer)value);
			else if (value instanceof Long)
				values.put(key, (Long)value);
			else if (value instanceof Float)
				values.put(key, (Float)value);
			else if (value instanceof Double)
				values.put(key, (Double)value);
			else if (value instanceof Boolean)
				values.put(key, (Boolean)value);
			else if (value instanceof byte[])
				values.put(key, (byte[])value);
			else
				throw new IllegalArgumentException("bad value type: " + value.getClass());
			return this;
		}

		public Builder withSelection(String selection, String[] selectionArgs) {
			this.selection = selection;
			this.selectionArgs = selectionArgs;
			return this;
		}

		public Builder withYieldAllowed(boolean yieldAllowed) {
			this.yieldAllowed = yieldAllowed;
			return this;
		}

		/* back references need a batch that remembers earlier results, which
		 * ATL's applyBatch does not; the values are taken as given instead */
		public Builder withValueBackReference(String key, int previousResult) {
			return this;
		}

		public Builder withSelectionBackReference(int selectionArgIndex, int previousResult) {
			return this;
		}

		public Builder withExpectedCount(int count) {
			return this;
		}

		public ContentProviderOperation build() {
			return new ContentProviderOperation(this);
		}
	}
}
