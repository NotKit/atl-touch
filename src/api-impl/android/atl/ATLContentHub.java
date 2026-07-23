package android.atl;

/**
 * Bridge to Ubuntu Touch's content-hub broker, the confined-app way to import
 * files/media the app can't reach directly (AppArmor blocks browsing $HOME).
 *
 * content-hub only enumerates the apps that can supply a content type and runs
 * the transfer; the source-app chooser is drawn by us (ATLContentHubPicker).
 *
 * The natives are only linked on device (content-hub-glib present). On the
 * desktop they stay unresolved, so {@link #listSources} throws
 * UnsatisfiedLinkError and callers fall back to ATLFilePicker.
 */
public class ATLContentHub {

	public static final int SELECT_SINGLE = 0;
	public static final int SELECT_MULTIPLE = 1;

	public static final class Peer {
		public final String id;
		public final String name;
		public final String iconPath;
		public final byte[] icon;

		Peer(String id, String name, String iconPath, byte[] icon) {
			this.id = id;
			this.name = name;
			this.iconPath = iconPath;
			this.icon = icon;
		}
	}

	/**
	 * Apps that can supply {@code contentType} ("pictures", "videos", "all", ...).
	 * Returns null if content-hub is unreachable; an empty array if reachable but
	 * no app offers the type. Throws UnsatisfiedLinkError off-device.
	 */
	public static Peer[] listSources(String contentType) {
		Object[] rows = nativeListSources(contentType);
		if (rows == null)
			return null;
		Peer[] peers = new Peer[rows.length];
		for (int i = 0; i < rows.length; i++) {
			Object[] r = (Object[]) rows[i];
			peers[i] = new Peer((String) r[0], (String) r[1], (String) r[2], (byte[]) r[3]);
		}
		return peers;
	}

	private static native Object[] nativeListSources(String contentType);

	/**
	 * Run an import from one peer, blocking until the user finishes picking in the
	 * source app (or it aborts). Returns the imported files' local paths, or null
	 * on cancel/error. Must not run on the UI thread.
	 */
	public static native String[] nativeImport(String peerId, String contentType, int selectionType);

	/** Cancel the in-flight import from another thread (unblocks nativeImport). */
	public static native void nativeImportCancel();

	/** Purge the cache dir imported files are moved into. */
	public static native void nativeClearTemp();
}
