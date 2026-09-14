package android.atl;

import android.util.Slog;
import android.util.Xml;

import dalvik.system.DexFile;

import org.xmlpull.v1.XmlPullParser;

import java.io.File;
import java.io.FileReader;
import java.io.Reader;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * The shared Java libraries an app declares with &lt;uses-library&gt;.
 *
 * ATL ships none of its own, but on a Halium device the whole Android system
 * image is mounted, and its libraries are ordinary dex jars declared by
 * &lt;root&gt;/&lt;partition&gt;/etc/permissions/*.xml exactly as on Android.
 * Google Camera is why this exists: its HDR+ and experimental modes go through
 * the Pixel's own camera2 keys, which live in a vendor jar
 * (com.google.android.camera.experimental20NN) holding one class of Key
 * constants.
 */
public class ATLSharedLibraries {

	private static final String TAG = "ATLSharedLibraries";

	/** where a Halium container's Android system image is mounted */
	private static final String DEFAULT_ROOT = "/android";

	/** the partitions Android itself reads library declarations from */
	private static final String[] PARTITIONS = {"system", "system_ext", "product", "vendor", "odm"};

	private static Map<String, String> libraries;

	private ATLSharedLibraries() {}

	/** The jar a &lt;uses-library&gt; name resolves to on this device, or null. */
	public static synchronized String resolve(String name) {
		if (libraries == null)
			libraries = declaredLibraries();
		return libraries.get(name);
	}

	/**
	 * Puts an app's resolved libraries on the class loader its own classes come
	 * from, so a jar appended here is visible to the app the way a shared
	 * library is on Android: by name, through plain Class.forName.
	 */
	public static void install(ClassLoader loader, String[] jars) {
		if (jars == null)
			return;
		for (String jar : jars) {
			try {
				appendToClassPath(loader, new File(jar));
				Slog.i(TAG, "shared library on the class path: " + jar);
			} catch (Throwable t) {
				Slog.w(TAG, "shared library " + jar + " could not be loaded", t);
			}
		}
	}

	/**
	 * Appends a dex jar or APK to a class loader's search path. There is no API
	 * for that, so this reaches into DexPathList the way every plugin loader on
	 * Android does.
	 */
	public static void appendToClassPath(ClassLoader loader, File path) throws Exception {
		Object pathList = getField(loader, "pathList");
		Object[] elements = (Object[])getField(pathList, "dexElements");
		Class<?> elementClass = elements.getClass().getComponentType();

		Constructor<?> constructor = elementClass.getDeclaredConstructor(DexFile.class, File.class);
		constructor.setAccessible(true);
		Object element = constructor.newInstance(DexFile.loadDex(path.getCanonicalPath(), null, 0), path);

		Object[] grown = (Object[])Array.newInstance(elementClass, elements.length + 1);
		System.arraycopy(elements, 0, grown, 0, elements.length);
		grown[elements.length] = element;
		setField(pathList, "dexElements", grown);
	}

	private static Map<String, String> declaredLibraries() {
		String root = androidRoot();
		Map<String, String> map = new HashMap<String, String>();

		for (String partition : PARTITIONS) {
			File[] files = new File(root + "/" + partition + "/etc/permissions").listFiles();
			if (files == null)
				continue;
			for (File file : files) {
				if (file.getName().endsWith(".xml"))
					readLibraries(file, root, map);
			}
		}

		if (!map.isEmpty())
			Slog.i(TAG, map.size() + " shared libraries declared under " + root);
		return Collections.unmodifiableMap(map);
	}

	private static String androidRoot() {
		String root = System.getenv("ATL_ANDROID_ROOT");
		if (root == null)
			root = DEFAULT_ROOT;
		while (root.endsWith("/"))
			root = root.substring(0, root.length() - 1);
		return root;
	}

	/**
	 * One permissions file: &lt;library name="..." file="/vendor/framework/x.jar"/&gt;.
	 * The path in it is absolute in the Android image, so it is read under the
	 * root the image is mounted at.
	 */
	private static void readLibraries(File xml, String root, Map<String, String> map) {
		try (Reader reader = new FileReader(xml)) {
			XmlPullParser parser = Xml.newPullParser();
			parser.setInput(reader);
			for (int event = parser.getEventType(); event != XmlPullParser.END_DOCUMENT; event = parser.next()) {
				if (event != XmlPullParser.START_TAG || !"library".equals(parser.getName()))
					continue;
				String name = parser.getAttributeValue(null, "name");
				String file = parser.getAttributeValue(null, "file");
				if (name == null || file == null)
					continue;
				File jar = new File(file.startsWith("/") ? root + file : file);
				if (jar.isFile())
					map.put(name, jar.getPath());
				else
					Slog.w(TAG, "shared library " + name + " is declared by " + xml + " but " + jar + " is missing");
			}
		} catch (Exception e) {
			Slog.w(TAG, "could not read " + xml + ": " + e);
		}
	}

	private static Object getField(Object object, String name) throws Exception {
		Field field = declaredField(object.getClass(), name);
		field.setAccessible(true);
		return field.get(object);
	}

	private static void setField(Object object, String name, Object value) throws Exception {
		Field field = declaredField(object.getClass(), name);
		field.setAccessible(true);
		field.set(object, value);
	}

	/** pathList is declared by BaseDexClassLoader, some way up from the loader we get */
	private static Field declaredField(Class<?> cls, String name) throws NoSuchFieldException {
		for (Class<?> c = cls; c != null; c = c.getSuperclass()) {
			try {
				return c.getDeclaredField(name);
			} catch (NoSuchFieldException e) {
				/* try the superclass */
			}
		}
		throw new NoSuchFieldException(cls.getName() + "." + name);
	}
}
