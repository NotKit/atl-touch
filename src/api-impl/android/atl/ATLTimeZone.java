package android.atl;

import android.util.Slog;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.TimeZone;

/**
 * Gives the runtime the host's timezone data.
 *
 * Finding the host zone is the same everywhere; loading it is not. A runtime that
 * carries its own zone database (any JDK) only needs the id, while libcore wants an
 * Android tzdata blob it can be handed — see {@link ATLZoneInfoDb}, which is only
 * touched when the runtime turns out not to know the zone.
 */
public class ATLTimeZone {
	private static final String TAG = "ATLTimeZone";

	private static final String DEFAULT_ZONEINFO_DIR = "/usr/share/zoneinfo";

	// the tzdata index reserves 40 bytes per id, so nothing longer can be a zone
	static final int SIZEOF_TZNAME = 40;

	public static void init() {
		String id = hostZoneId();
		if (id == null || !isSaneZoneId(id)) {
			Slog.w(TAG, "could not determine the host timezone, staying on " + TimeZone.getDefault().getID());
			return;
		}

		TimeZone zone = TimeZone.getTimeZone(id);
		if (!id.equals(zone.getID())) {
			// The runtime does not know this zone, so it has no database of its own:
			// that is ART, and only there may we name libcore.
			try {
				zone = ATLZoneInfoDb.loadZone(id);
			} catch (LinkageError e) {
				Slog.w(TAG, "no timezone database to fall back on: " + e);
				zone = null;
			}
		}

		if (zone == null) {
			Slog.w(TAG, "no data for host timezone " + id + ", staying on " + TimeZone.getDefault().getID());
			return;
		}

		TimeZone.setDefault(zone);
		System.setProperty("user.timezone", id);
		Slog.i(TAG, "default timezone is " + id);
	}

	static String zoneInfoDir() {
		String dir = System.getenv("TZDIR");
		return dir != null && !dir.isEmpty() ? dir : DEFAULT_ZONEINFO_DIR;
	}

	private static boolean isSaneZoneId(String id) {
		return !id.isEmpty() && !id.startsWith("/") && !id.contains("..") && id.length() < SIZEOF_TZNAME;
	}

	private static String hostZoneId() {
		String tz = System.getenv("TZ");
		if (tz != null && tz.startsWith(":"))
			tz = tz.substring(1);
		// anything without a '/' is a POSIX rule like "EET-2EEST", not a zone we can load
		if (tz != null && tz.indexOf('/') > 0)
			return tz;

		String etc = readFile(new File("/etc/timezone"));
		if (etc != null)
			return etc.trim();

		// no /etc/timezone (Debian 13, Arch, Fedora): follow the /etc/localtime symlink
		try {
			String link = Files.readSymbolicLink(Paths.get("/etc/localtime")).toString();
			int zoneinfo = link.indexOf("zoneinfo/");
			if (zoneinfo >= 0)
				return link.substring(zoneinfo + "zoneinfo/".length());
		} catch (IOException | UnsupportedOperationException e) {
		}

		return null;
	}

	static String readFile(File file) {
		try {
			String contents = new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
			int newline = contents.indexOf('\n');
			contents = (newline >= 0 ? contents.substring(0, newline) : contents).trim();
			return contents.isEmpty() ? null : contents;
		} catch (IOException e) {
			return null;
		}
	}
}
