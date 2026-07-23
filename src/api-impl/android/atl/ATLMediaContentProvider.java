package android.atl;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Bridges Telegram's in-app gallery (MediaStore queries on content://media/...)
 * onto a real host folder the user picks. Telegram enumerates the gallery by
 * querying the images and then the video content URI; we answer with one cursor
 * row per media file in the chosen folder, so its attach sheet shows a real
 * thumbnail grid instead of a single item.
 */
public class ATLMediaContentProvider extends ContentProvider {

	private static final Set<String> IMAGE_EXT = new HashSet<String>(Arrays.asList(
	    "jpg", "jpeg", "png", "gif", "webp", "bmp", "heic", "heif"));
	private static final Set<String> VIDEO_EXT = new HashSet<String>(Arrays.asList(
	    "mp4", "mkv", "webm", "mov", "avi", "3gp", "m4v"));

	boolean waitingForFileChooser = false;
	// the folder the user picked, whose media files back the gallery grid
	File selectedDir = null;

	// called from native
	void setSelectedFile(String selectedFile) {
		this.selectedDir = selectedFile == null ? null : new File(selectedFile);
		this.waitingForFileChooser = false;
		synchronized (this) {
			notifyAll();
		}
	}

	private void openFileChooser() {
		if (!waitingForFileChooser) {
			waitingForFileChooser = true;
			new Handler(Looper.getMainLooper()).post(new Runnable() {
				@Override
				public void run() {
					new ATLFilePicker(android.content.Context.this_application, ATLFilePicker.ACTION_SELECT_FOLDER,
					                  "Open Media Folder", null, new ATLFilePicker.ResultListener() {
						@Override
						public void onResult(File file) {
							setSelectedFile(file != null ? file.getAbsolutePath() : null);
						}
					}).show();
				}
			});
		}
		synchronized (this) {
			try {
				while (waitingForFileChooser) {
					wait();
				}
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
	}

	private static boolean hasExt(File f, Set<String> exts) {
		String name = f.getName();
		int dot = name.lastIndexOf('.');
		if (dot < 0)
			return false;
		return exts.contains(name.substring(dot + 1).toLowerCase(Locale.ROOT));
	}

	// media files of the requested kind in the chosen folder, newest first
	private List<File> listMedia(boolean video) {
		List<File> out = new ArrayList<File>();
		if (selectedDir == null || !selectedDir.isDirectory())
			return out;
		File[] files = selectedDir.listFiles();
		if (files == null)
			return out;
		for (File f : files) {
			if (f.isFile() && hasExt(f, video ? VIDEO_EXT : IMAGE_EXT))
				out.add(f);
		}
		Collections.sort(out, new java.util.Comparator<File>() {
			@Override
			public int compare(File a, File b) {
				return Long.compare(b.lastModified(), a.lastModified());
			}
		});
		return out;
	}

	private boolean isVideoUri(Uri uri) {
		return uri.toString().contains("/video/");
	}

	private void fillRow(Object[] row, String[] projection, File file, int id, boolean video) {
		int[] bounds = null;
		for (int i = 0; i < projection.length; i++) {
			switch (projection[i]) {
				case "_id":
					row[i] = id;
					break;
				case "_data":
				case "title":
					row[i] = file.getAbsolutePath();
					break;
				case "bucket_id":
					row[i] = selectedDir.getAbsolutePath().hashCode();
					break;
				case "bucket_display_name":
					row[i] = selectedDir.getName();
					break;
				case "mime_type":
					row[i] = mimeOf(file);
					break;
				case "media_type":
					row[i] = video ? 3 : 1;
					break;
				case "date_modified":
				case "datetaken":
					row[i] = file.lastModified();
					break;
				case "orientation":
					row[i] = 0;
					break;
				case "duration":
					row[i] = 0;
					break;
				case "width":
				case "height":
					if (!video) {
						if (bounds == null)
							bounds = decodeBounds(file);
						row[i] = "width".equals(projection[i]) ? bounds[0] : bounds[1];
					} else {
						row[i] = 0;
					}
					break;
				case "_size":
					row[i] = file.length();
					break;
			}
		}
	}

	private static int[] decodeBounds(File file) {
		BitmapFactory.Options opts = new BitmapFactory.Options();
		opts.inJustDecodeBounds = true;
		try {
			BitmapFactory.decodeFile(file.getAbsolutePath(), opts);
		} catch (Throwable t) {
			// a broken file shouldn't drop it from the grid
		}
		return new int[]{Math.max(opts.outWidth, 0), Math.max(opts.outHeight, 0)};
	}

	private static String mimeOf(File file) {
		try {
			String type = Files.probeContentType(file.toPath());
			if (type != null)
				return type;
		} catch (IOException e) {
			// fall through
		}
		return "application/octet-stream";
	}

	@Override
	public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
		boolean video = isVideoUri(uri);
		boolean count = projection != null && projection.length == 1 && projection[0] != null
		    && projection[0].toUpperCase(Locale.ROOT).startsWith("COUNT");
		boolean distinct = uri.getQueryParameter("distinct") != null;
		// a .../media/<id> lookup resolves one known file, it must not pop the picker
		boolean idLookup = "0".equals(uri.getLastPathSegment())
		    || (selectionArgs != null && selectionArgs.length > 0);

		// Pop the folder picker once, when Telegram starts enumerating the gallery
		// (its images query). The video query that follows reuses the same folder.
		if (!count && !distinct && !idLookup && !video && selectedDir == null) {
			openFileChooser();
		}

		if (count) {
			MatrixCursor cursor = new MatrixCursor(projection);
			cursor.addRow(new Object[]{listMedia(video).size()});
			return cursor;
		}

		if (distinct) {
			MatrixCursor cursor = new MatrixCursor(projection);
			if (selectedDir != null) {
				Object[] row = new Object[projection.length];
				for (int i = 0; i < projection.length; i++) {
					switch (projection[i]) {
						case "bucket_display_name":
							row[i] = selectedDir.getName();
							break;
						case "bucket_id":
							row[i] = selectedDir.getAbsolutePath().hashCode();
							break;
					}
				}
				cursor.addRow(row);
			}
			return cursor;
		}

		MatrixCursor cursor = new MatrixCursor(projection);
		List<File> files = listMedia(video);
		for (int i = 0; i < files.size(); i++) {
			Object[] row = new Object[projection.length];
			fillRow(row, projection, files.get(i), i, video);
			cursor.addRow(row);
		}
		return cursor;
	}

	// resolve a file for the single-file access paths (openFile/getType)
	private File fileFor(Uri uri) {
		String last = uri.getLastPathSegment();
		try {
			int id = Integer.parseInt(last);
			List<File> images = listMedia(false);
			if (id >= 0 && id < images.size())
				return images.get(id);
			List<File> videos = listMedia(true);
			if (id >= 0 && id < videos.size())
				return videos.get(id);
		} catch (NumberFormatException e) {
			// not an id-addressed uri
		}
		List<File> images = listMedia(false);
		if (!images.isEmpty())
			return images.get(0);
		return selectedDir;
	}

	@Override
	public String getType(Uri uri) {
		File file = fileFor(uri);
		return file == null ? "application/octet-stream" : mimeOf(file);
	}

	@Override
	public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
		File file = fileFor(uri);
		if (file == null)
			throw new FileNotFoundException(uri.toString());
		return ParcelFileDescriptor.open(file, ParcelFileDescriptor.parseMode(mode));
	}

	@Override
	public Uri insert(Uri uri, ContentValues values) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'insert'");
	}

	@Override
	public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'update'");
	}

	@Override
	public int delete(Uri uri, String selection, String[] selectionArgs) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'delete'");
	}

	@Override
	public AssetFileDescriptor openAssetFile(Uri uri, String mode) throws FileNotFoundException {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'openAssetFile'");
	}
}
