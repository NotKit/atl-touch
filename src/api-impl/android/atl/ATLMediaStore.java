package android.atl;

import android.content.ContentValues;
import android.net.Uri;
import android.os.Environment;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * The write half of content://media: the rows an app inserts, each backed by a
 * real file on the host.
 *
 * An app that saves a photo on Android Q or later never writes a path of its
 * own. It inserts a row naming the file it wants, gets a content uri back and
 * writes through that; a null from the insert is fatal (Google Camera throws
 * on it, and the shot is lost). So an insert here creates the file and hands
 * back a uri that openFile can serve.
 *
 * The files land under the host's picture folder rather than the app's private
 * external storage, because a photo the user cannot find in their gallery has
 * not really been saved. ATL_MEDIA_STORE_DIR overrides the root.
 */
class ATLMediaStore {

	/** one inserted row: the file, and whether the app is still writing it */
	static final class Row {
		final long id;
		final File file;
		final String mime;
		final boolean video;
		boolean pending;

		Row(long id, File file, String mime, boolean video, boolean pending) {
			this.id = id;
			this.file = file;
			this.mime = mime;
			this.video = video;
			this.pending = pending;
		}
	}

	/* well clear of the row ids the picked-gallery enumeration hands out, which
	 * are indexes into its own list */
	private static final long FIRST_ID = 1000000;

	/* the folder names by hand rather than through Environment, whose static
	 * init needs the app data dir out of JNI */
	private static final String PICTURES = "Pictures";
	private static final String MOVIES = "Movies";

	private final Map<Long, Row> rows = new HashMap<Long, Row>();
	private long nextId = FIRST_ID;
	private File root = null;

	/**
	 * The volume root: $HOME/Pictures, so that DCIM/Camera/foo.jpg becomes
	 * ~/Pictures/DCIM/Camera/foo.jpg and the host's gallery indexes it. An app
	 * data dir is the fallback when the home folder cannot be written, which is
	 * what a confined click app sees.
	 */
	private synchronized File root() {
		if (root != null)
			return root;
		String override = System.getenv("ATL_MEDIA_STORE_DIR");
		File candidate = null;

		if (override != null) {
			candidate = new File(override);
		} else {
			String home = System.getenv("HOME");
			if (home == null)
				home = System.getProperty("user.home");
			if (home != null)
				candidate = new File(home, PICTURES);
		}
		if (candidate != null) {
			candidate.mkdirs();
			if (candidate.isDirectory() && candidate.canWrite()) {
				root = candidate;
				return root;
			}
		}
		root = Environment.getExternalStorageDirectory();
		return root;
	}

	private static boolean isVideo(Uri uri, String mime) {
		if (mime != null)
			return mime.startsWith("video/");
		return uri.toString().contains("/video/");
	}

	/** the extension for a mime type, for a display name given without one */
	private static String extensionOf(String mime) {
		if (mime == null)
			return "bin";
		if (mime.equals("image/jpeg"))
			return "jpg";
		if (mime.equals("image/heif") || mime.equals("image/heic"))
			return "heic";
		if (mime.equals("image/x-adobe-dng"))
			return "dng";
		if (mime.equals("audio/mpeg"))
			return "mp3";
		int slash = mime.indexOf('/');
		return slash < 0 ? "bin" : mime.substring(slash + 1);
	}

	/** foo.jpg, foo (1).jpg, foo (2).jpg ... as MediaStore itself does */
	private static File uniqueFile(File dir, String name) {
		File file = new File(dir, name);
		int dot = name.lastIndexOf('.');
		String stem = dot < 0 ? name : name.substring(0, dot);
		String ext = dot < 0 ? "" : name.substring(dot);

		for (int i = 1; file.exists() && i < 1000; i++)
			file = new File(dir, stem + " (" + i + ")" + ext);
		return file;
	}

	/**
	 * Create the file the values describe and remember it. Null when the values
	 * name nothing usable or the file cannot be created; the caller reports that
	 * to the app as a failed insert.
	 */
	synchronized Row insert(Uri uri, ContentValues values) {
		String name = values == null ? null : values.getAsString("_display_name");
		String data = values == null ? null : values.getAsString("_data");
		String relative = values == null ? null : values.getAsString("relative_path");
		String mime = values == null ? null : values.getAsString("mime_type");
		Integer pending = values == null ? null : values.getAsInteger("is_pending");
		boolean video = isVideo(uri, mime);
		File file;

		if (name == null && data == null)
			name = "ATL_" + System.currentTimeMillis();
		if (data != null) {
			file = new File(data);
		} else {
			if (name.indexOf('.') < 0)
				name = name + "." + extensionOf(mime);
			if (relative == null)
				relative = video ? MOVIES : PICTURES;
			relative = relative.replaceAll("^/+", "").replaceAll("/+$", "");
			file = uniqueFile(new File(root(), relative), name);
		}
		File dir = file.getParentFile();
		if (dir != null)
			dir.mkdirs();
		try {
			if (!file.exists() && !file.createNewFile())
				return null;
		} catch (IOException e) {
			System.out.println("ATLMediaStore: cannot create " + file + ": " + e);
			return null;
		}

		Row row = new Row(nextId++, file, mime, video, pending != null && pending.intValue() != 0);
		rows.put(Long.valueOf(row.id), row);
		System.out.println("ATLMediaStore: " + uri + " -> " + file);
		return row;
	}

	/** the row a .../media/<id> uri addresses, or null */
	/* the row for a file already on disk (a shot from an earlier run, or one
	 * another app saved): found by path, or registered now with a fresh id */
	synchronized Row rowFor(File file, boolean video) {
		for (Row row : rows.values()) {
			if (row.file.equals(file))
				return row;
		}
		Row row = new Row(nextId++, file, null, video, false);
		rows.put(Long.valueOf(row.id), row);
		return row;
	}

	File storeRoot() {
		return root();
	}

	synchronized Row byUri(Uri uri) {
		String last = uri.getLastPathSegment();

		if (last == null)
			return null;
		try {
			return rows.get(Long.valueOf(Long.parseLong(last)));
		} catch (NumberFormatException e) {
			return null;
		}
	}

	/**
	 * The only update that matters is publishing a pending row; a rename comes
	 * with it when the app finalises the file under its real name.
	 */
	synchronized int update(Uri uri, ContentValues values) {
		Row row = byUri(uri);
		Integer pending = values == null ? null : values.getAsInteger("is_pending");
		String name = values == null ? null : values.getAsString("_display_name");

		if (row == null)
			return 0;
		if (pending != null)
			row.pending = pending.intValue() != 0;
		if (name != null && !name.equals(row.file.getName())) {
			File dir = row.file.getParentFile();
			File renamed = uniqueFile(dir, name);
			if (row.file.renameTo(renamed))
				rows.put(Long.valueOf(row.id),
				    new Row(row.id, renamed, row.mime, row.video, row.pending));
		}
		return 1;
	}

	synchronized int delete(Uri uri) {
		Row row = byUri(uri);

		if (row == null)
			return 0;
		row.file.delete();
		rows.remove(Long.valueOf(row.id));
		return 1;
	}

	/** every row still on disk, newest first */
	synchronized List<Row> published() {
		List<Row> out = new ArrayList<Row>();

		for (Row row : rows.values()) {
			if (!row.pending && row.file.isFile())
				out.add(row);
		}
		return out;
	}

	static String mimeOf(Row row) {
		if (row.mime != null)
			return row.mime;
		String name = row.file.getName().toLowerCase(Locale.ROOT);
		if (name.endsWith(".jpg") || name.endsWith(".jpeg"))
			return "image/jpeg";
		if (name.endsWith(".png"))
			return "image/png";
		if (name.endsWith(".mp4"))
			return "video/mp4";
		return row.video ? "video/*" : "image/*";
	}
}
