/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.content;

import android.net.Uri;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.ArrayMap;
import android.util.SparseArray;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

public class ContentProviderOperation implements Parcelable {

	public final static int TYPE_INSERT = 1;
	public final static int TYPE_UPDATE = 2;
	public final static int TYPE_DELETE = 3;
	public final static int TYPE_ASSERT = 4;
	public final static int TYPE_CALL = 5;

	private final int mType;
	private final Uri mUri;
	private final String mMethod;
	private final String mArg;
	private final ArrayMap<String, Object> mValues;
	private final ArrayMap<String, Object> mExtras;
	private final String mSelection;
	private final SparseArray<Object> mSelectionArgs;
	private final Integer mExpectedCount;
	private final boolean mYieldAllowed;
	private final boolean mExceptionAllowed;

	private ContentProviderOperation(Builder builder) {
		mType = builder.mType;
		mUri = builder.mUri;
		mMethod = builder.mMethod;
		mArg = builder.mArg;
		mValues = builder.mValues;
		mExtras = builder.mExtras;
		mSelection = builder.mSelection;
		mSelectionArgs = builder.mSelectionArgs;
		mExpectedCount = builder.mExpectedCount;
		mYieldAllowed = builder.mYieldAllowed;
		mExceptionAllowed = builder.mExceptionAllowed;
	}

	private ContentProviderOperation(Parcel source) throws ReflectiveOperationException {
		mType = source.readInt();
		mUri = Uri.CREATOR.createFromParcel(source);
		mMethod = source.readInt() != 0 ? source.readString() : null;
		mArg = source.readInt() != 0 ? source.readString() : null;
		final int valuesSize = source.readInt();
		if (valuesSize != -1) {
			mValues = new ArrayMap<>(valuesSize);
			source.readMap(mValues, null);
		} else {
			mValues = null;
		}
		final int extrasSize = source.readInt();
		if (extrasSize != -1) {
			mExtras = new ArrayMap<>(extrasSize);
			source.readMap(mExtras, null);
		} else {
			mExtras = null;
		}
		mSelection = source.readInt() != 0 ? source.readString() : null;
		mSelectionArgs = source.readSparseArray(null);
		mExpectedCount = source.readInt() != 0 ? source.readInt() : null;
		mYieldAllowed = source.readInt() != 0;
		mExceptionAllowed = source.readInt() != 0;
	}

	@Override
	public void writeToParcel(Parcel dest, int flags) {
		dest.writeInt(mType);
		Uri.writeToParcel(dest, mUri);
		if (mMethod != null) {
			dest.writeInt(1);
			dest.writeString(mMethod);
		} else {
			dest.writeInt(0);
		}
		if (mArg != null) {
			dest.writeInt(1);
			dest.writeString(mArg);
		} else {
			dest.writeInt(0);
		}
		if (mValues != null) {
			dest.writeInt(mValues.size());
			dest.writeMap(mValues);
		} else {
			dest.writeInt(-1);
		}
		if (mExtras != null) {
			dest.writeInt(mExtras.size());
			dest.writeMap(mExtras);
		} else {
			dest.writeInt(-1);
		}
		if (mSelection != null) {
			dest.writeInt(1);
			dest.writeString(mSelection);
		} else {
			dest.writeInt(0);
		}
		dest.writeSparseArray(mSelectionArgs);
		if (mExpectedCount != null) {
			dest.writeInt(1);
			dest.writeInt(mExpectedCount);
		} else {
			dest.writeInt(0);
		}
		dest.writeInt(mYieldAllowed ? 1 : 0);
		dest.writeInt(mExceptionAllowed ? 1 : 0);
	}

	public static final @android.annotation.NonNull Creator<ContentProviderOperation> CREATOR = new Creator<ContentProviderOperation>() {
		@Override
		public ContentProviderOperation createFromParcel(Parcel source) {
			try {
				return new ContentProviderOperation(source);
			} catch (ReflectiveOperationException e) {
				throw new RuntimeException(e);
			}
		}

		@Override
		public ContentProviderOperation[] newArray(int size) {
			return new ContentProviderOperation[size];
		}
	};

	public Uri getUri() {
		return mUri;
	}

	public boolean isYieldAllowed() {
		return mYieldAllowed;
	}

	public ContentProviderResult apply(ContentProvider provider, ContentProviderResult[] backRefs, int numBackRefs)
	    throws OperationApplicationException {
		if (mExceptionAllowed) {
			try {
				return applyInternal(provider);
			} catch (Exception e) {
				return new ContentProviderResult(e);
			}
		}
		return applyInternal(provider);
	}

	// back references can't be created with our Builder, so the values are used as they are
	private ContentProviderResult applyInternal(ContentProvider provider) throws OperationApplicationException {
		ContentValues values = null;
		if (mValues != null) {
			values = new ContentValues();
			for (int i = 0; i < mValues.size(); i++) {
				values.putObject(mValues.keyAt(i), mValues.valueAt(i));
			}
		}
		final String[] selectionArgs = resolveSelectionArgs();
		if (mType == TYPE_INSERT) {
			final Uri newUri = provider.insert(mUri, values);
			if (newUri == null)
				throw new OperationApplicationException("Insert into " + mUri + " returned no result");
			return new ContentProviderResult(newUri);
		}
		final int numRows;
		if (mType == TYPE_UPDATE) {
			numRows = provider.update(mUri, values, mSelection, selectionArgs);
		} else if (mType == TYPE_DELETE) {
			numRows = provider.delete(mUri, mSelection, selectionArgs);
		} else {
			throw new UnsupportedOperationException("ContentProviderOperation type " + mType + " not implemented");
		}
		if (mExpectedCount != null && mExpectedCount != numRows)
			throw new OperationApplicationException("Expected " + mExpectedCount + " rows but actual " + numRows);
		return new ContentProviderResult(numRows);
	}

	private String[] resolveSelectionArgs() {
		if (mSelectionArgs != null) {
			int max = -1;
			for (int i = 0; i < mSelectionArgs.size(); i++) {
				max = Math.max(max, mSelectionArgs.keyAt(i));
			}

			final String[] selectionArgs = new String[max + 1];
			for (int i = 0; i < mSelectionArgs.size(); i++) {
				selectionArgs[mSelectionArgs.keyAt(i)] = String.valueOf(mSelectionArgs.valueAt(i));
			}
			return selectionArgs;
		} else {
			return null;
		}
	}

	public static Builder newUpdate(Uri uri) {
		return new Builder(TYPE_UPDATE, uri);
	}

	public static Builder newInsert(Uri uri) {
		return new Builder(TYPE_INSERT, uri);
	}

	public static Builder newDelete(Uri uri) {
		return new Builder(TYPE_DELETE, uri);
	}

	public static Builder newAssertQuery(Uri uri) { return new Builder(TYPE_ASSERT, uri); }
	public boolean isInsert() { return mType == TYPE_INSERT; }
	public boolean isUpdate() { return mType == TYPE_UPDATE; }
	public boolean isDelete() { return mType == TYPE_DELETE; }
	public boolean isAssertQuery() { return mType == TYPE_ASSERT; }

	public static class Builder {
		private final int mType;
		private final Uri mUri;
		private final String mMethod;
		private final String mArg;
		private ArrayMap<String, Object> mValues;
		private ArrayMap<String, Object> mExtras;
		private String mSelection;
		private SparseArray<Object> mSelectionArgs;
		private Integer mExpectedCount;
		private boolean mYieldAllowed;
		private boolean mExceptionAllowed;

		private Builder(int type, Uri uri) {
			this(type, uri, null, null);
		}

		private Builder(int type, Uri uri, String method, String arg) {
			mType = type;
			mUri = Objects.requireNonNull(uri);
			mMethod = method;
			mArg = arg;
		}

		private void setValue(String key, Object value) {
			if (mValues == null)
				mValues = new ArrayMap<>();
			mValues.put(key, value);
		}

		public Builder withValues(ContentValues values) {
			final Set<Map.Entry<String, Object>> rawValues = values.valueSet();
			for (Map.Entry<String, Object> entry : rawValues) {
				setValue(entry.getKey(), entry.getValue());
			}
			return this;
		}

		public Builder withSelection(String selection, String[] selectionArgs) {
			mSelection = selection;
			if (mSelectionArgs == null)
				mSelectionArgs = new SparseArray<>();
			if (selectionArgs != null) {
				for (int i = 0; i < selectionArgs.length; i++) {
					mSelectionArgs.put(i, selectionArgs[i]);
				}
			}
			return this;
		}

		public Builder withValue(String key, Object value) { setValue(key, value); return this; }
		public Builder withYieldAllowed(boolean allowed) { mYieldAllowed = allowed; return this; }
		public Builder withExpectedCount(int count) { mExpectedCount = count; return this; }
		public Builder withValueBackReference(String key, int previousResult) { return this; }
		public Builder withSelectionBackReference(int index, int previousResult) { return this; }

		public ContentProviderOperation build() {
			if (mType == TYPE_UPDATE) {
				if ((mValues == null || mValues.isEmpty())) {
					throw new IllegalArgumentException("Empty values");
				}
			}
			if (mType == TYPE_ASSERT) {
				if ((mValues == null || mValues.isEmpty())
				    && (mExpectedCount == null)) {
					throw new IllegalArgumentException("Empty values");
				}
			}
			return new ContentProviderOperation(this);
		}
	}
}
