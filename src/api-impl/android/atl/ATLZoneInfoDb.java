package android.atl;

import android.util.Slog;

import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Map;
import java.util.TimeZone;
import java.util.TreeMap;

import libcore.io.MemoryMappedFile;
import libcore.util.ZoneInfo;
import libcore.util.ZoneInfoDB;

/**
 * The libcore half of {@link ATLTimeZone}: gives a runtime with no zone database of
 * its own the host's.
 *
 * libcore wants an Android tzdata blob (every zone concatenated into one indexed file)
 * under $ANDROID_ROOT, which we neither ship nor can point it at: setting ANDROID_ROOT
 * would also move ART's dex2oat and boot image. Without it every lookup returns GMT and
 * apps show UTC timestamps, since libcore's host fallback only reads /etc/timezone, which
 * several distros no longer have.
 *
 * So build that blob out of the host's /usr/share/zoneinfo once per tzdata release and
 * hand it to ZoneInfoDB directly.
 *
 * This lives apart from ATLTimeZone because a runtime without libcore (a stock JVM)
 * cannot even verify a class that names ZoneInfo — and does not need one.
 */
class ATLZoneInfoDb {
	private static final String TAG = "ATLTimeZone";

	private static final File CACHE_DIR = new File("/tmp/atl_cache");

	// the database reserves 40 bytes per id, and each index entry adds offset/length/unused ints
	private static final int SIZEOF_INDEX_ENTRY = ATLTimeZone.SIZEOF_TZNAME + 3 * 4;
	private static final int SIZEOF_HEADER = 12 + 3 * 4;

	/** The zone as libcore sees it, or null if the host has no data for it. */
	static TimeZone loadZone(String id) {
		TimeZone zone = installZoneDatabase() ? TimeZone.getTimeZone(id) : null;
		// the database is missing or doesn't know the host zone: at least get the default right
		if (zone == null || !id.equals(zone.getID()))
			zone = readZoneFile(id);
		return zone;
	}

	/** Builds the tzdata blob if needed and points ZoneInfoDB at it. */
	private static boolean installZoneDatabase() {
		try {
			File blob = new File(CACHE_DIR, "tzdata-" + cacheKey());
			if (!blob.exists())
				writeZoneDatabase(blob);

			ZoneInfoDB.TzData data = ZoneInfoDB.TzData.loadTzData(blob.getPath());
			if (data == null) {
				Slog.w(TAG, "generated " + blob + " is not loadable");
				return false;
			}

			Field field = ZoneInfoDB.class.getDeclaredField("DATA");
			field.setAccessible(true);
			field.set(null, data);
			return true;
		} catch (Exception | LinkageError e) {
			Slog.w(TAG, "could not install the host timezone database: " + e);
			return false;
		}
	}

	/** Reads a single zone straight from the host's tzfile, for when the database isn't there. */
	private static TimeZone readZoneFile(String id) {
		File tzfile = new File(ATLTimeZone.zoneInfoDir(), id);
		try (MemoryMappedFile mapped = MemoryMappedFile.mmapRO(tzfile.getPath())) {
			return ZoneInfo.readTimeZone(id, mapped.bigEndianIterator(), System.currentTimeMillis());
		} catch (Exception | LinkageError e) {
			Slog.w(TAG, "failed to load " + tzfile + ": " + e);
			return null;
		}
	}

	private static void writeZoneDatabase(File blob) throws IOException {
		long started = System.currentTimeMillis();

		File dir = new File(ATLTimeZone.zoneInfoDir());
		Map<String, File> zones = new TreeMap<>(); // the index has to come out sorted by id
		collectZones(dir, "", zones);
		if (zones.isEmpty())
			throw new IOException("no zones under " + dir);

		byte[] zoneTab = zoneTab(dir);
		int dataOffset = SIZEOF_HEADER + SIZEOF_INDEX_ENTRY * zones.size();
		long dataLength = 0;
		for (File file : zones.values())
			dataLength += file.length();

		CACHE_DIR.mkdirs();
		// write out of the way and rename, so a second app launching at the same time
		// either sees no blob or a complete one
		File tmp = new File(blob.getPath() + "." + android.os.Process.myPid());
		try {
			try (DataOutputStream out = new DataOutputStream(new java.io.BufferedOutputStream(new FileOutputStream(tmp)))) {
				out.write(("tzdata" + version(dir)).getBytes(StandardCharsets.US_ASCII));
				out.write(0);
				out.writeInt(SIZEOF_HEADER);
				out.writeInt(dataOffset);
				out.writeInt((int)(dataOffset + dataLength));

				int offset = 0;
				for (Map.Entry<String, File> zone : zones.entrySet()) {
					byte[] id = zone.getKey().getBytes(StandardCharsets.US_ASCII);
					int length = (int)zone.getValue().length();
					out.write(id);
					out.write(new byte[ATLTimeZone.SIZEOF_TZNAME - id.length]);
					out.writeInt(offset);
					out.writeInt(length);
					out.writeInt(0); // used to be the raw offset, ignored since
					offset += length;
				}

				for (File file : zones.values())
					copy(file, out);

				out.write(zoneTab);
			}
			if (!tmp.renameTo(blob))
				throw new IOException("could not rename " + tmp + " to " + blob);
		} finally {
			tmp.delete();
		}

		Slog.i(TAG, "built " + blob + " from " + dir + ": " + zones.size() + " zones in "
		            + (System.currentTimeMillis() - started) + "ms");
	}

	private static void collectZones(File dir, String prefix, Map<String, File> zones) {
		File[] entries = dir.listFiles();
		if (entries == null)
			return;

		for (File entry : entries) {
			String id = prefix + entry.getName();
			if (entry.isDirectory()) {
				// posix/ and right/ are alternative copies of every zone
				if (!id.equals("posix") && !id.equals("right"))
					collectZones(entry, id + "/", zones);
			} else if (id.length() < ATLTimeZone.SIZEOF_TZNAME && !id.equals("localtime") && !id.equals("posixrules")
			           && isZoneFile(entry)) {
				zones.put(id, entry);
			}
		}
	}

	private static boolean isZoneFile(File file) {
		// the index rejects anything shorter than a tzfile header
		if (file.length() < 44)
			return false;
		byte[] magic = new byte[4];
		try (InputStream in = new java.io.FileInputStream(file)) {
			return in.read(magic) == magic.length && new String(magic, StandardCharsets.US_ASCII).equals("TZif");
		} catch (IOException e) {
			return false;
		}
	}

	private static void copy(File file, OutputStream out) throws IOException {
		byte[] buffer = new byte[8192];
		try (InputStream in = new java.io.FileInputStream(file)) {
			for (int read; (read = in.read(buffer)) > 0; )
				out.write(buffer, 0, read);
		}
	}

	private static byte[] zoneTab(File dir) {
		for (String name : new String[] {"zone1970.tab", "zone.tab"}) {
			try {
				return Files.readAllBytes(new File(dir, name).toPath());
			} catch (IOException e) {
			}
		}
		return "# no zone.tab\n".getBytes(StandardCharsets.US_ASCII); // the header requires a non-empty section
	}

	/** The tzdata release the host is on, as the 5 ASCII chars the header has room for. */
	private static String version(File dir) {
		String version = ATLTimeZone.readFile(new File(dir, "+VERSION"));
		if (version == null) {
			String line = ATLTimeZone.readFile(new File(dir, "tzdata.zi")); // "# version 2026b\n..."
			if (line != null) {
				String[] words = line.split("\\s+");
				if (words.length >= 3 && words[1].equals("version"))
					version = words[2];
			}
		}
		if (version == null || version.length() != 5 || !version.matches("[0-9a-z]+"))
			version = "atl00";
		return version;
	}

	private static String cacheKey() {
		File dir = new File(ATLTimeZone.zoneInfoDir());
		return version(dir) + "-" + dir.lastModified();
	}
}
