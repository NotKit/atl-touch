package android.media;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * The EXIF of a saved JPEG: enough of it for what a camera app writes and
 * reads back.
 *
 * The attributes are kept as strings in AOSP's own textual form (a rational
 * is "numerator/denominator", a list is comma separated), parsed out of the
 * APP1 segment and written back by rebuilding that segment from scratch —
 * everything else in the file is copied through untouched.
 */
public class ExifInterface {

	public static final String TAG_IMAGE_WIDTH = "ImageWidth";
	public static final String TAG_IMAGE_LENGTH = "ImageLength";
	public static final String TAG_MAKE = "Make";
	public static final String TAG_MODEL = "Model";
	public static final String TAG_ORIENTATION = "Orientation";
	public static final String TAG_SOFTWARE = "Software";
	public static final String TAG_DATETIME = "DateTime";
	public static final String TAG_DATETIME_ORIGINAL = "DateTimeOriginal";
	public static final String TAG_DATETIME_DIGITIZED = "DateTimeDigitized";
	public static final String TAG_EXPOSURE_TIME = "ExposureTime";
	public static final String TAG_F_NUMBER = "FNumber";
	public static final String TAG_APERTURE_VALUE = "ApertureValue";
	public static final String TAG_ISO_SPEED_RATINGS = "ISOSpeedRatings";
	public static final String TAG_PHOTOGRAPHIC_SENSITIVITY = "PhotographicSensitivity";
	public static final String TAG_FLASH = "Flash";
	public static final String TAG_FOCAL_LENGTH = "FocalLength";
	public static final String TAG_WHITE_BALANCE = "WhiteBalance";
	public static final String TAG_PIXEL_X_DIMENSION = "PixelXDimension";
	public static final String TAG_PIXEL_Y_DIMENSION = "PixelYDimension";
	public static final String TAG_GPS_LATITUDE = "GPSLatitude";
	public static final String TAG_GPS_LATITUDE_REF = "GPSLatitudeRef";
	public static final String TAG_GPS_LONGITUDE = "GPSLongitude";
	public static final String TAG_GPS_LONGITUDE_REF = "GPSLongitudeRef";
	public static final String TAG_GPS_ALTITUDE = "GPSAltitude";
	public static final String TAG_GPS_ALTITUDE_REF = "GPSAltitudeRef";
	public static final String TAG_GPS_TIMESTAMP = "GPSTimeStamp";
	public static final String TAG_GPS_DATESTAMP = "GPSDateStamp";
	public static final String TAG_GPS_PROCESSING_METHOD = "GPSProcessingMethod";

	public static final int ORIENTATION_UNDEFINED = 0;
	public static final int ORIENTATION_NORMAL = 1;
	public static final int ORIENTATION_FLIP_HORIZONTAL = 2;
	public static final int ORIENTATION_ROTATE_180 = 3;
	public static final int ORIENTATION_FLIP_VERTICAL = 4;
	public static final int ORIENTATION_TRANSPOSE = 5;
	public static final int ORIENTATION_ROTATE_90 = 6;
	public static final int ORIENTATION_TRANSVERSE = 7;
	public static final int ORIENTATION_ROTATE_270 = 8;

	public static final int WHITEBALANCE_AUTO = 0;
	public static final int WHITEBALANCE_MANUAL = 1;

	private static final int TYPE_BYTE = 1;
	private static final int TYPE_ASCII = 2;
	private static final int TYPE_SHORT = 3;
	private static final int TYPE_LONG = 4;
	private static final int TYPE_RATIONAL = 5;
	private static final int TYPE_UNDEFINED = 7;
	private static final int TYPE_SLONG = 9;
	private static final int TYPE_SRATIONAL = 10;

	private static final int IFD_0 = 0;
	private static final int IFD_EXIF = 1;
	private static final int IFD_GPS = 2;

	private static final int TAG_EXIF_POINTER = 0x8769;
	private static final int TAG_GPS_POINTER = 0x8825;

	/* one known attribute: where it lives, its tag number and its type */
	private static final class Definition {
		final String name;
		final int ifd;
		final int number;
		final int type;

		Definition(String name, int ifd, int number, int type) {
			this.name = name;
			this.ifd = ifd;
			this.number = number;
			this.type = type;
		}
	}

	private static final List<Definition> DEFINITIONS = new ArrayList<Definition>();
	private static final Map<String, Definition> BY_NAME = new HashMap<String, Definition>();
	private static final Map<Integer, Definition> BY_TAG = new HashMap<Integer, Definition>();

	private static void define(String name, int ifd, int number, int type) {
		Definition definition = new Definition(name, ifd, number, type);

		DEFINITIONS.add(definition);
		BY_NAME.put(name, definition);
		BY_TAG.put(Integer.valueOf(ifd << 16 | number), definition);
	}

	static {
		define(TAG_IMAGE_WIDTH, IFD_0, 0x0100, TYPE_LONG);
		define(TAG_IMAGE_LENGTH, IFD_0, 0x0101, TYPE_LONG);
		define(TAG_MAKE, IFD_0, 0x010f, TYPE_ASCII);
		define(TAG_MODEL, IFD_0, 0x0110, TYPE_ASCII);
		define(TAG_ORIENTATION, IFD_0, 0x0112, TYPE_SHORT);
		define(TAG_SOFTWARE, IFD_0, 0x0131, TYPE_ASCII);
		define(TAG_DATETIME, IFD_0, 0x0132, TYPE_ASCII);

		define(TAG_EXPOSURE_TIME, IFD_EXIF, 0x829a, TYPE_RATIONAL);
		define(TAG_F_NUMBER, IFD_EXIF, 0x829d, TYPE_RATIONAL);
		define(TAG_ISO_SPEED_RATINGS, IFD_EXIF, 0x8827, TYPE_SHORT);
		define(TAG_DATETIME_ORIGINAL, IFD_EXIF, 0x9003, TYPE_ASCII);
		define(TAG_DATETIME_DIGITIZED, IFD_EXIF, 0x9004, TYPE_ASCII);
		define(TAG_APERTURE_VALUE, IFD_EXIF, 0x9202, TYPE_RATIONAL);
		define(TAG_FLASH, IFD_EXIF, 0x9209, TYPE_SHORT);
		define(TAG_FOCAL_LENGTH, IFD_EXIF, 0x920a, TYPE_RATIONAL);
		define(TAG_PIXEL_X_DIMENSION, IFD_EXIF, 0xa002, TYPE_LONG);
		define(TAG_PIXEL_Y_DIMENSION, IFD_EXIF, 0xa003, TYPE_LONG);
		define(TAG_WHITE_BALANCE, IFD_EXIF, 0xa403, TYPE_SHORT);

		define(TAG_GPS_LATITUDE_REF, IFD_GPS, 0x0001, TYPE_ASCII);
		define(TAG_GPS_LATITUDE, IFD_GPS, 0x0002, TYPE_RATIONAL);
		define(TAG_GPS_LONGITUDE_REF, IFD_GPS, 0x0003, TYPE_ASCII);
		define(TAG_GPS_LONGITUDE, IFD_GPS, 0x0004, TYPE_RATIONAL);
		define(TAG_GPS_ALTITUDE_REF, IFD_GPS, 0x0005, TYPE_BYTE);
		define(TAG_GPS_ALTITUDE, IFD_GPS, 0x0006, TYPE_RATIONAL);
		define(TAG_GPS_TIMESTAMP, IFD_GPS, 0x0007, TYPE_RATIONAL);
		define(TAG_GPS_PROCESSING_METHOD, IFD_GPS, 0x001b, TYPE_UNDEFINED);
		define(TAG_GPS_DATESTAMP, IFD_GPS, 0x001d, TYPE_ASCII);
	}

	/* PhotographicSensitivity is the modern name of ISOSpeedRatings, and apps
	 * use both; they share the one tag */
	private static String canonicalName(String name) {
		return TAG_PHOTOGRAPHIC_SENSITIVITY.equals(name) ? TAG_ISO_SPEED_RATINGS : name;
	}

	private final String filename;
	private final Map<String, String> attributes = new LinkedHashMap<String, String>();

	public ExifInterface(String filename) throws IOException {
		if (filename == null)
			throw new NullPointerException("filename must not be null");
		this.filename = filename;

		InputStream in = new FileInputStream(filename);
		try {
			readExif(readAll(in));
		} finally {
			in.close();
		}
	}

	public ExifInterface(File file) throws IOException {
		this(file.getAbsolutePath());
	}

	/** Read-only: an ExifInterface over a stream has no file to save to. */
	public ExifInterface(InputStream in) throws IOException {
		this.filename = null;
		readExif(readAll(in));
	}

	public String getAttribute(String tag) {
		return attributes.get(canonicalName(tag));
	}

	public int getAttributeInt(String tag, int defaultValue) {
		String value = getAttribute(tag);

		if (value == null)
			return defaultValue;
		try {
			int comma = value.indexOf(',');

			if (comma >= 0)
				value = value.substring(0, comma);
			int slash = value.indexOf('/');

			if (slash >= 0)
				return (int)rational(value);
			return Integer.parseInt(value.trim());
		} catch (NumberFormatException e) {
			return defaultValue;
		}
	}

	public double getAttributeDouble(String tag, double defaultValue) {
		String value = getAttribute(tag);

		if (value == null)
			return defaultValue;
		try {
			int comma = value.indexOf(',');

			if (comma >= 0)
				value = value.substring(0, comma);
			return value.indexOf('/') >= 0 ? rational(value) : Double.parseDouble(value.trim());
		} catch (NumberFormatException e) {
			return defaultValue;
		}
	}

	public byte[] getThumbnail() {
		return null;
	}

	public boolean hasThumbnail() {
		return false;
	}

	public void setAttribute(String tag, String value) {
		String name = canonicalName(tag);

		if (!BY_NAME.containsKey(name)) {
			System.out.println("ExifInterface: ignoring unsupported tag " + tag);
			return;
		}
		if (value == null)
			attributes.remove(name);
		else
			attributes.put(name, value);
	}

	/** Degrees north and east, or false when the file carries no position. */
	public boolean getLatLong(float[] output) {
		double[] latLong = getLatLong();

		if (latLong == null)
			return false;
		output[0] = (float)latLong[0];
		output[1] = (float)latLong[1];
		return true;
	}

	public double[] getLatLong() {
		String latitude = getAttribute(TAG_GPS_LATITUDE);
		String latitudeRef = getAttribute(TAG_GPS_LATITUDE_REF);
		String longitude = getAttribute(TAG_GPS_LONGITUDE);
		String longitudeRef = getAttribute(TAG_GPS_LONGITUDE_REF);

		if (latitude == null || longitude == null || latitudeRef == null || longitudeRef == null)
			return null;
		try {
			double lat = degrees(latitude, latitudeRef, "S");
			double lon = degrees(longitude, longitudeRef, "W");

			return new double[] {lat, lon};
		} catch (NumberFormatException e) {
			return null;
		}
	}

	public double getAltitude(double defaultValue) {
		double altitude = getAttributeDouble(TAG_GPS_ALTITUDE, Double.NaN);
		int ref = getAttributeInt(TAG_GPS_ALTITUDE_REF, 0);

		if (Double.isNaN(altitude))
			return defaultValue;
		return ref == 1 ? -altitude : altitude;
	}

	public void setLatLong(double latitude, double longitude) {
		if (latitude < -90 || latitude > 90 || Double.isNaN(latitude))
			throw new IllegalArgumentException("latitude " + latitude + " is out of range");
		if (longitude < -180 || longitude > 180 || Double.isNaN(longitude))
			throw new IllegalArgumentException("longitude " + longitude + " is out of range");

		setAttribute(TAG_GPS_LATITUDE_REF, latitude >= 0 ? "N" : "S");
		setAttribute(TAG_GPS_LATITUDE, dms(Math.abs(latitude)));
		setAttribute(TAG_GPS_LONGITUDE_REF, longitude >= 0 ? "E" : "W");
		setAttribute(TAG_GPS_LONGITUDE, dms(Math.abs(longitude)));
	}

	/** Rewrites the file with a freshly built APP1 in front of everything else. */
	public void saveAttributes() throws IOException {
		if (filename == null)
			throw new IOException("this ExifInterface was not opened from a file");

		byte[] jpeg;
		InputStream in = new FileInputStream(filename);
		try {
			jpeg = readAll(in);
		} finally {
			in.close();
		}
		if (jpeg.length < 2 || (jpeg[0] & 0xff) != 0xff || (jpeg[1] & 0xff) != 0xd8)
			throw new IOException(filename + " is not a JPEG");

		byte[] app1 = buildApp1();
		ByteArrayOutputStream out = new ByteArrayOutputStream(jpeg.length + app1.length);

		out.write(jpeg, 0, 2); /* SOI */
		out.write(app1, 0, app1.length);
		copyWithoutExif(jpeg, out);

		OutputStream file = new FileOutputStream(filename);
		try {
			out.writeTo(file);
		} finally {
			file.close();
		}
	}

	/* ---- reading ---- */

	private static byte[] readAll(InputStream in) throws IOException {
		ByteArrayOutputStream out = new ByteArrayOutputStream();
		byte[] chunk = new byte[16384];
		int read;

		while ((read = in.read(chunk)) > 0)
			out.write(chunk, 0, read);
		return out.toByteArray();
	}

	private void readExif(byte[] jpeg) {
		int offset = exifSegmentOffset(jpeg);

		if (offset < 0)
			return;
		int length = ((jpeg[offset - 2] & 0xff) << 8 | (jpeg[offset - 1] & 0xff)) - 2;
		int tiff = offset + 6; /* past "Exif\0\0" */

		if (length < 6 || tiff + 8 > jpeg.length)
			return;
		try {
			readTiff(jpeg, tiff, offset + length);
		} catch (RuntimeException e) {
			System.out.println("ExifInterface: malformed EXIF ignored (" + e + ")");
		}
	}

	/* the start of the "Exif\0\0" APP1 payload, or -1 */
	private static int exifSegmentOffset(byte[] jpeg) {
		int i = 2;

		if (jpeg.length < 4 || (jpeg[0] & 0xff) != 0xff || (jpeg[1] & 0xff) != 0xd8)
			return -1;
		while (i + 4 <= jpeg.length && (jpeg[i] & 0xff) == 0xff) {
			int marker = jpeg[i + 1] & 0xff;

			if (marker == 0xd8 || marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) {
				i += 2;
				continue;
			}
			if (marker == 0xda || marker == 0xd9) /* image data starts here */
				return -1;
			int length = (jpeg[i + 2] & 0xff) << 8 | (jpeg[i + 3] & 0xff);

			if (marker == 0xe1 && i + 4 + 6 <= jpeg.length && jpeg[i + 4] == 'E' &&
			    jpeg[i + 5] == 'x' && jpeg[i + 6] == 'i' && jpeg[i + 7] == 'f')
				return i + 4;
			i += 2 + length;
		}
		return -1;
	}

	private void readTiff(byte[] data, int tiff, int end) {
		boolean bigEndian;
		int byteOrder = (data[tiff] & 0xff) << 8 | (data[tiff + 1] & 0xff);

		if (byteOrder == 0x4d4d)
			bigEndian = true;
		else if (byteOrder == 0x4949)
			bigEndian = false;
		else
			return;

		if (readShort(data, tiff + 2, bigEndian) != 42)
			return;
		int ifd0 = (int)readLong(data, tiff + 4, bigEndian);

		readIfd(data, tiff, ifd0, end, bigEndian, IFD_0);
	}

	private void readIfd(byte[] data, int tiff, int offset, int end, boolean bigEndian, int ifd) {
		int base = tiff + offset;

		if (base + 2 > end || base < tiff)
			return;
		int count = readShort(data, base, bigEndian);

		for (int i = 0; i < count; i++) {
			int entry = base + 2 + i * 12;

			if (entry + 12 > end)
				return;
			int number = readShort(data, entry, bigEndian);
			int type = readShort(data, entry + 2, bigEndian);
			int components = (int)readLong(data, entry + 4, bigEndian);
			int size = componentSize(type) * components;
			int value = entry + 8;

			if (size > 4) {
				value = tiff + (int)readLong(data, entry + 8, bigEndian);
				if (value < tiff || value + size > end)
					continue;
			}

			if (number == TAG_EXIF_POINTER || number == TAG_GPS_POINTER) {
				int pointer = (int)readLong(data, value, bigEndian);

				readIfd(data, tiff, pointer, end, bigEndian,
				    number == TAG_EXIF_POINTER ? IFD_EXIF : IFD_GPS);
				continue;
			}

			Definition definition = BY_TAG.get(Integer.valueOf(ifd << 16 | number));
			if (definition == null)
				continue;
			String text = readValue(data, value, type, components, bigEndian);
			if (text != null)
				attributes.put(definition.name, text);
		}
	}

	private static String readValue(byte[] data, int offset, int type, int components, boolean bigEndian) {
		StringBuilder text = new StringBuilder();

		switch (type) {
		case TYPE_ASCII:
		case TYPE_UNDEFINED: {
			int length = components;

			while (length > 0 && data[offset + length - 1] == 0)
				length--;
			return new String(data, offset, length);
		}
		case TYPE_BYTE:
			for (int i = 0; i < components; i++)
				text.append(i > 0 ? "," : "").append(data[offset + i] & 0xff);
			return text.toString();
		case TYPE_SHORT:
			for (int i = 0; i < components; i++)
				text.append(i > 0 ? "," : "").append(readShort(data, offset + i * 2, bigEndian));
			return text.toString();
		case TYPE_LONG:
		case TYPE_SLONG:
			for (int i = 0; i < components; i++)
				text.append(i > 0 ? "," : "").append(readLong(data, offset + i * 4, bigEndian));
			return text.toString();
		case TYPE_RATIONAL:
		case TYPE_SRATIONAL:
			for (int i = 0; i < components; i++) {
				long numerator = readLong(data, offset + i * 8, bigEndian);
				long denominator = readLong(data, offset + i * 8 + 4, bigEndian);

				text.append(i > 0 ? "," : "").append(numerator).append('/').append(denominator);
			}
			return text.toString();
		default:
			return null;
		}
	}

	private static int componentSize(int type) {
		switch (type) {
		case TYPE_SHORT:
			return 2;
		case TYPE_LONG:
		case TYPE_SLONG:
			return 4;
		case TYPE_RATIONAL:
		case TYPE_SRATIONAL:
			return 8;
		default:
			return 1;
		}
	}

	private static int readShort(byte[] data, int offset, boolean bigEndian) {
		int a = data[offset] & 0xff;
		int b = data[offset + 1] & 0xff;

		return bigEndian ? a << 8 | b : b << 8 | a;
	}

	private static long readLong(byte[] data, int offset, boolean bigEndian) {
		long a = data[offset] & 0xffL;
		long b = data[offset + 1] & 0xffL;
		long c = data[offset + 2] & 0xffL;
		long d = data[offset + 3] & 0xffL;

		return bigEndian ? a << 24 | b << 16 | c << 8 | d : d << 24 | c << 16 | b << 8 | a;
	}

	/* ---- writing ---- */

	/* everything after the SOI except the EXIF APP1 the new one replaces */
	private static void copyWithoutExif(byte[] jpeg, ByteArrayOutputStream out) {
		int offset = exifSegmentOffset(jpeg);

		if (offset < 0) {
			out.write(jpeg, 2, jpeg.length - 2);
			return;
		}
		int start = offset - 4; /* the marker and its length */
		int length = (jpeg[offset - 2] & 0xff) << 8 | (jpeg[offset - 1] & 0xff);

		out.write(jpeg, 2, start - 2);
		out.write(jpeg, start + 2 + length, jpeg.length - (start + 2 + length));
	}

	/*
	 * A whole APP1 segment: "Exif\0\0", a big-endian TIFF header, IFD0 with
	 * pointers to the Exif and GPS IFDs, and every value that does not fit in
	 * four bytes appended after the directories.
	 */
	private byte[] buildApp1() {
		List<Definition> ifd0 = definitionsFor(IFD_0);
		List<Definition> exif = definitionsFor(IFD_EXIF);
		List<Definition> gps = definitionsFor(IFD_GPS);

		int entries0 = ifd0.size() + (exif.isEmpty() ? 0 : 1) + (gps.isEmpty() ? 0 : 1);
		int ifd0Size = 2 + entries0 * 12 + 4;
		int exifSize = exif.isEmpty() ? 0 : 2 + exif.size() * 12 + 4;
		int gpsSize = gps.isEmpty() ? 0 : 2 + gps.size() * 12 + 4;

		int ifd0Offset = 8;
		int exifOffset = ifd0Offset + ifd0Size;
		int gpsOffset = exifOffset + exifSize;
		int dataOffset = gpsOffset + gpsSize;

		ByteArrayOutputStream directories = new ByteArrayOutputStream();
		ByteArrayOutputStream values = new ByteArrayOutputStream();

		writeShort(directories, 0x4d4d); /* MM: big endian */
		writeShort(directories, 42);
		writeInt(directories, ifd0Offset);

		writeIfd(directories, values, ifd0, dataOffset,
		    exif.isEmpty() ? -1 : exifOffset, gps.isEmpty() ? -1 : gpsOffset);
		if (!exif.isEmpty())
			writeIfd(directories, values, exif, dataOffset, -1, -1);
		if (!gps.isEmpty())
			writeIfd(directories, values, gps, dataOffset, -1, -1);

		byte[] tiff = new byte[directories.size() + values.size()];
		System.arraycopy(directories.toByteArray(), 0, tiff, 0, directories.size());
		System.arraycopy(values.toByteArray(), 0, tiff, directories.size(), values.size());

		int payload = 6 + tiff.length;
		ByteArrayOutputStream app1 = new ByteArrayOutputStream(payload + 4);

		app1.write(0xff);
		app1.write(0xe1);
		writeShort(app1, payload + 2);
		app1.write('E');
		app1.write('x');
		app1.write('i');
		app1.write('f');
		app1.write(0);
		app1.write(0);
		app1.write(tiff, 0, tiff.length);
		return app1.toByteArray();
	}

	private List<Definition> definitionsFor(int ifd) {
		List<Definition> list = new ArrayList<Definition>();

		for (Definition definition : DEFINITIONS)
			if (definition.ifd == ifd && attributes.containsKey(definition.name))
				list.add(definition);
		return list;
	}

	private void writeIfd(ByteArrayOutputStream directories, ByteArrayOutputStream values,
	    List<Definition> definitions, int dataOffset, int exifOffset, int gpsOffset) {
		int count = definitions.size() + (exifOffset >= 0 ? 1 : 0) + (gpsOffset >= 0 ? 1 : 0);

		writeShort(directories, count);
		for (Definition definition : definitions) {
			byte[] encoded = encode(definition, attributes.get(definition.name));
			int components = componentCount(definition, encoded);

			writeShort(directories, definition.number);
			writeShort(directories, definition.type);
			writeInt(directories, components);
			if (encoded.length <= 4) {
				directories.write(encoded, 0, encoded.length);
				for (int i = encoded.length; i < 4; i++)
					directories.write(0);
			} else {
				writeInt(directories, dataOffset + values.size());
				values.write(encoded, 0, encoded.length);
				if ((encoded.length & 1) != 0)
					values.write(0); /* IFD values start on even offsets */
			}
		}
		if (exifOffset >= 0) {
			writeShort(directories, TAG_EXIF_POINTER);
			writeShort(directories, TYPE_LONG);
			writeInt(directories, 1);
			writeInt(directories, exifOffset);
		}
		if (gpsOffset >= 0) {
			writeShort(directories, TAG_GPS_POINTER);
			writeShort(directories, TYPE_LONG);
			writeInt(directories, 1);
			writeInt(directories, gpsOffset);
		}
		writeInt(directories, 0); /* no next IFD */
	}

	private static int componentCount(Definition definition, byte[] encoded) {
		return encoded.length / componentSize(definition.type);
	}

	private static byte[] encode(Definition definition, String value) {
		ByteArrayOutputStream out = new ByteArrayOutputStream();

		switch (definition.type) {
		case TYPE_ASCII:
		case TYPE_UNDEFINED: {
			byte[] bytes = value.getBytes();

			out.write(bytes, 0, bytes.length);
			out.write(0);
			break;
		}
		case TYPE_BYTE:
			for (String part : value.split(","))
				out.write(Integer.parseInt(part.trim()) & 0xff);
			break;
		case TYPE_SHORT:
			for (String part : value.split(","))
				writeShort(out, Integer.parseInt(part.trim()));
			break;
		case TYPE_LONG:
		case TYPE_SLONG:
			for (String part : value.split(","))
				writeInt(out, (int)Long.parseLong(part.trim()));
			break;
		default:
			for (String part : value.split(",")) {
				int slash = part.indexOf('/');

				if (slash < 0) {
					writeInt(out, (int)Math.round(Double.parseDouble(part.trim()) * 1000));
					writeInt(out, 1000);
				} else {
					writeInt(out, (int)Long.parseLong(part.substring(0, slash).trim()));
					writeInt(out, (int)Long.parseLong(part.substring(slash + 1).trim()));
				}
			}
			break;
		}
		return out.toByteArray();
	}

	private static void writeShort(ByteArrayOutputStream out, int value) {
		out.write(value >> 8 & 0xff);
		out.write(value & 0xff);
	}

	private static void writeInt(ByteArrayOutputStream out, int value) {
		out.write(value >> 24 & 0xff);
		out.write(value >> 16 & 0xff);
		out.write(value >> 8 & 0xff);
		out.write(value & 0xff);
	}

	/* ---- values ---- */

	private static double rational(String value) {
		int slash = value.indexOf('/');

		if (slash < 0)
			return Double.parseDouble(value.trim());
		double denominator = Double.parseDouble(value.substring(slash + 1).trim());

		if (denominator == 0)
			return 0;
		return Double.parseDouble(value.substring(0, slash).trim()) / denominator;
	}

	private static double degrees(String value, String ref, String negative) {
		String[] parts = value.split(",");

		if (parts.length != 3)
			throw new NumberFormatException("not a degrees/minutes/seconds triple: " + value);
		double degrees = rational(parts[0]) + rational(parts[1]) / 60 + rational(parts[2]) / 3600;

		return ref.trim().equalsIgnoreCase(negative) ? -degrees : degrees;
	}

	private static String dms(double value) {
		int degrees = (int)value;
		double remainder = (value - degrees) * 60;
		int minutes = (int)remainder;
		long seconds = Math.round((remainder - minutes) * 60 * 1000);

		return degrees + "/1," + minutes + "/1," + seconds + "/1000";
	}
}
