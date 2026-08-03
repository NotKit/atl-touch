package android.atl;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.provider.MediaStore;
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
import java.util.concurrent.CountDownLatch;

/**
 * Bridges Telegram's in-app gallery (MediaStore queries on content://media/...)
 * onto real host files the user picks. Telegram enumerates the gallery by
 * querying the images and then the video content URI; we answer with one cursor
 * row per media file, so its attach sheet shows a real thumbnail grid.
 *
 * The files come from one of two sources, chosen at pick time:
 *   - Ubuntu Touch: content-hub, the confined-app way to import media. The user
 *     picks in the system gallery and content-hub hands back the imported files.
 *   - Desktop: ATLFilePicker, browsing $HOME for a media folder (content-hub is
 *     unreachable there). The chosen folder is enumerated into the file list.
 */
public class ATLMediaContentProvider extends ContentProvider {

	private static final Set<String> IMAGE_EXT = new HashSet<String>(Arrays.asList(
	    "jpg", "jpeg", "png", "gif", "webp", "bmp", "heic", "heif"));
	private static final Set<String> VIDEO_EXT = new HashSet<String>(Arrays.asList(
	    "mp4", "mkv", "webm", "mov", "avi", "3gp", "m4v"));

	// the picked media files backing the gallery grid; null until a pick resolves
	// (an empty list means the user canceled — don't re-pop the picker)
	private List<File> selectedFiles = null;
	private String bucketName = "Gallery";
	// only one query drives the pick; the rest wait for its result
	private boolean picking = false;
	// until when enumerations answer empty instead of picking, because they are
	// the re-read our own expiry asked for
	private long refreshUntil = 0;

	/* A pick answers one enumeration, but apps cache the album they build from it
	 * (Telegram only re-reads the media store when its cached album is null), so
	 * without help the picker would appear once per app launch. Some time after
	 * the app stops touching the gallery we drop the selection and report a media
	 * store change: the app re-enumerates, gets nothing, forgets its album, and
	 * the next attach-menu open pops the picker again.
	 *
	 * Long enough that it can't clear a grid the user is still looking at. */
	private static final long IDLE_EXPIRY_MS = 20000;
	/* How long we wait for that re-read. An app that doesn't watch the media store
	 * never sends one; past this the next enumeration picks again, so nothing
	 * stays stuck. */
	private static final long EXPIRY_REFRESH_MS = 6000;
	/* Once it does arrive, keep answering empty for a beat: an app that watches
	 * several media uris reloads once per observer, and a second reload that
	 * picked would pop the picker with nobody asking for it. */
	private static final long REFRESH_SETTLE_MS = 1000;

	private final Handler expiryHandler = new Handler(Looper.getMainLooper());
	private final Runnable expiry = new Runnable() {
		@Override
		public void run() {
			expireSelection();
		}
	};

	private void touchSelection() {
		expiryHandler.removeCallbacks(expiry);
		synchronized (this) {
			if (selectedFiles == null || picking)
				return;
		}
		expiryHandler.postDelayed(expiry, IDLE_EXPIRY_MS);
	}

	private void expireSelection() {
		synchronized (this) {
			if (selectedFiles == null || picking)
				return;
			selectedFiles = null;
			refreshUntil = SystemClock.uptimeMillis() + EXPIRY_REFRESH_MS;
		}
		ContentResolver resolver = getContext().getContentResolver();
		resolver.notifyChange(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, null);
		resolver.notifyChange(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, null);
	}

	private void openFileChooser() {
		boolean drive = false;
		synchronized (this) {
			if (selectedFiles != null)
				return;
			if (!picking) {
				picking = true;
				drive = true;
			}
		}
		if (drive) {
			List<File> result;
			String name;
			// A preset folder skips the pick entirely. Apps that read the media
			// store without the user asking for it (a camera app looking for its
			// last shot) would otherwise pop the picker out of nowhere, and
			// headless runs have nobody to answer it.
			String preset = System.getenv("ATL_MEDIA_FOLDER");
			ATLContentHub.Peer[] peers = null;
			boolean contentHub = false;
			if (preset == null) {
				// content-hub first; UnsatisfiedLinkError (desktop) or null (no
				// broker) falls through to the folder picker.
				contentHub = true;
				try {
					peers = ATLContentHub.listSources("pictures");
				} catch (Throwable t) {
					contentHub = false;
				}
			}
			if (preset != null) {
				File folder = new File(preset);
				result = listFolder(folder);
				name = folder.getName();
			} else if (contentHub && peers != null) {
				result = importViaContentHub(peers);
				name = "Gallery";
			} else {
				File folder = pickFolderBlocking();
				result = listFolder(folder);
				name = folder != null ? folder.getName() : "Gallery";
			}
			synchronized (this) {
				bucketName = name;
				selectedFiles = result;
				picking = false;
				notifyAll();
			}
		} else {
			synchronized (this) {
				while (selectedFiles == null) {
					try {
						wait();
					} catch (InterruptedException e) {
						return;
					}
				}
			}
		}
	}

	// pick a source app, import from it; empty list on cancel/no source
	private List<File> importViaContentHub(ATLContentHub.Peer[] peers) {
		if (peers.length == 0)
			return new ArrayList<File>();
		String peerId = pickPeerBlocking(peers);
		if (peerId == null)
			return new ArrayList<File>();
		String[] paths;
		try {
			paths = ATLContentHub.nativeImport(peerId, "pictures", ATLContentHub.SELECT_MULTIPLE);
		} catch (Throwable t) {
			return new ArrayList<File>();
		}
		List<File> out = new ArrayList<File>();
		if (paths != null) {
			for (String p : paths)
				out.add(new File(p));
		}
		return out;
	}

	// show ATLContentHubPicker on the UI thread, block until a peer is chosen
	private String pickPeerBlocking(final ATLContentHub.Peer[] peers) {
		final CountDownLatch latch = new CountDownLatch(1);
		final String[] out = new String[1];
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				new ATLContentHubPicker(android.content.Context.this_application, null,
				                        peers, new ATLContentHubPicker.ResultListener() {
					@Override
					public void onResult(String peerId) {
						out[0] = peerId;
						latch.countDown();
					}
				}).show();
			}
		});
		try {
			latch.await();
		} catch (InterruptedException e) {
			// treated as cancel
		}
		return out[0];
	}

	// show ATLFilePicker on the UI thread, block until a folder is chosen
	private File pickFolderBlocking() {
		final CountDownLatch latch = new CountDownLatch(1);
		final File[] out = new File[1];
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				new ATLFilePicker(android.content.Context.this_application, ATLFilePicker.ACTION_SELECT_FOLDER,
				                  "Open Media Folder", null, new ATLFilePicker.ResultListener() {
					@Override
					public void onResult(File file) {
						out[0] = file;
						latch.countDown();
					}
				}).show();
			}
		});
		try {
			latch.await();
		} catch (InterruptedException e) {
			// treated as cancel
		}
		return out[0];
	}

	// all media files in a folder (both kinds), so listMedia can split by type
	private static List<File> listFolder(File dir) {
		List<File> out = new ArrayList<File>();
		if (dir == null || !dir.isDirectory())
			return out;
		File[] files = dir.listFiles();
		if (files == null)
			return out;
		for (File f : files) {
			if (f.isFile() && (hasExt(f, IMAGE_EXT) || hasExt(f, VIDEO_EXT)))
				out.add(f);
		}
		return out;
	}

	private static boolean hasExt(File f, Set<String> exts) {
		String name = f.getName();
		int dot = name.lastIndexOf('.');
		if (dot < 0)
			return false;
		return exts.contains(name.substring(dot + 1).toLowerCase(Locale.ROOT));
	}

	// picked media files of the requested kind, newest first
	private List<File> listMedia(boolean video) {
		List<File> out = new ArrayList<File>();
		if (selectedFiles == null)
			return out;
		Set<String> exts = video ? VIDEO_EXT : IMAGE_EXT;
		for (File f : selectedFiles) {
			if (f.isFile() && hasExt(f, exts))
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
					row[i] = bucketName.hashCode();
					break;
				case "bucket_display_name":
					row[i] = bucketName;
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

		// Pop the picker when Telegram starts enumerating the gallery (its images
		// query). The video query that follows reuses the result.
		if (!count && !distinct && !idLookup && !video && selectedFiles == null) {
			boolean refresh;
			synchronized (this) {
				long now = SystemClock.uptimeMillis();
				refresh = now < refreshUntil;
				if (refresh)
					refreshUntil = Math.min(refreshUntil, now + REFRESH_SETTLE_MS);
			}
			// the enumeration our own expiry provoked: answer empty, don't pick
			if (!refresh)
				openFileChooser();
		}
		touchSelection();

		if (count) {
			MatrixCursor cursor = new MatrixCursor(projection);
			cursor.addRow(new Object[]{listMedia(video).size()});
			return cursor;
		}

		if (distinct) {
			MatrixCursor cursor = new MatrixCursor(projection);
			if (selectedFiles != null && !selectedFiles.isEmpty()) {
				Object[] row = new Object[projection.length];
				for (int i = 0; i < projection.length; i++) {
					switch (projection[i]) {
						case "bucket_display_name":
							row[i] = bucketName;
							break;
						case "bucket_id":
							row[i] = bucketName.hashCode();
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
		touchSelection();
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
		return null;
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
