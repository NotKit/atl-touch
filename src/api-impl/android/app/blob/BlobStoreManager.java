package android.app.blob;

import android.content.Context;
import android.os.ParcelFileDescriptor;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.function.Consumer;

/**
 * The shared blob store, as a directory in the app's own data dir.
 *
 * AOSP's blob store is shared between apps and leased against a system-wide
 * quota; under ATL there is one app, so a blob is a file named after its
 * SHA-256 digest and a lease is bookkeeping. That is enough for what apps use
 * it for — caching a large read-only asset (Google Camera keeps its ML models
 * here) and finding it again by digest on the next launch.
 */
public class BlobStoreManager {

	/** an open session: bytes are written to a temp file and committed by rename */
	public static class Session implements AutoCloseable {
		private final BlobStoreManager manager;
		private final long sessionId;
		private final BlobHandle handle;
		private final File staged;
		private boolean closed;

		Session(BlobStoreManager manager, long sessionId, BlobHandle handle, File staged) {
			this.manager = manager;
			this.sessionId = sessionId;
			this.handle = handle;
			this.staged = staged;
		}

		public ParcelFileDescriptor openWrite(long offsetBytes, long lengthBytes) throws IOException {
			int mode = ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_CREATE;

			if (offsetBytes == 0)
				mode |= ParcelFileDescriptor.MODE_TRUNCATE;
			return ParcelFileDescriptor.open(staged, mode);
		}

		public ParcelFileDescriptor openRead() throws IOException {
			return ParcelFileDescriptor.open(staged, ParcelFileDescriptor.MODE_READ_ONLY);
		}

		public long getSize() throws IOException {
			return staged.length();
		}

		/** a no-op: there is nobody else on this device to share the blob with */
		public void allowPublicAccess() {
		}

		public void allowSameSignatureAccess() {
		}

		public void allowPackageAccess(String packageName, byte[] certificate) {
		}

		public void commit(Executor executor, Consumer<Integer> resultCallback) {
			final int result = manager.commitSession(sessionId, handle, staged) ? 0 : 1;

			closed = true;
			if (executor != null && resultCallback != null)
				executor.execute(new Runnable() {
					@Override
					public void run() {
						resultCallback.accept(result);
					}
				});
		}

		public void abandon() {
			close();
			staged.delete();
		}

		@Override
		public void close() {
			if (!closed)
				manager.forgetSession(sessionId);
			closed = true;
		}
	}

	private final Context context;
	private final Map<Long, Session> sessions = new HashMap<>();
	private final Map<BlobHandle, String> leases = new HashMap<>();
	private long nextSessionId = 1;

	public BlobStoreManager(Context context) {
		this.context = context;
	}

	private File storeDir() {
		File dir = new File(context.getDataDir(), "blobstore");

		dir.mkdirs();
		return dir;
	}

	public long createSession(BlobHandle handle) throws IOException {
		File staged = new File(storeDir(), handle.fileName() + ".staged");
		long id;

		synchronized (this) {
			id = nextSessionId++;
			sessions.put(id, new Session(this, id, handle, staged));
		}
		return id;
	}

	public Session openSession(long sessionId) throws IOException {
		Session session;

		synchronized (this) {
			session = sessions.get(sessionId);
		}
		if (session == null)
			throw new IOException("no such blob session: " + sessionId);
		return session;
	}

	public void abandonSession(long sessionId) throws IOException {
		openSession(sessionId).abandon();
	}

	public ParcelFileDescriptor openBlob(BlobHandle handle) throws IOException {
		File blob = new File(storeDir(), handle.fileName());

		if (!blob.isFile())
			throw new IOException("no blob for " + handle);
		return ParcelFileDescriptor.open(blob, ParcelFileDescriptor.MODE_READ_ONLY);
	}

	public void acquireLease(BlobHandle handle, CharSequence description, long leaseExpiryTimeMillis) {
		synchronized (this) {
			leases.put(handle, String.valueOf(description));
		}
	}

	public void acquireLease(BlobHandle handle, CharSequence description) {
		acquireLease(handle, description, 0);
	}

	public void acquireLease(BlobHandle handle, int descriptionResId) {
		acquireLease(handle, context.getString(descriptionResId), 0);
	}

	public void acquireLease(BlobHandle handle, int descriptionResId, long leaseExpiryTimeMillis) {
		acquireLease(handle, context.getString(descriptionResId), leaseExpiryTimeMillis);
	}

	public void releaseLease(BlobHandle handle) {
		synchronized (this) {
			leases.remove(handle);
		}
	}

	public List<BlobHandle> getLeasedBlobs() {
		synchronized (this) {
			return new ArrayList<>(leases.keySet());
		}
	}

	/** no quota is enforced, so what is left is what the filesystem has */
	public long getRemainingLeaseQuotaBytes() {
		return storeDir().getUsableSpace();
	}

	private boolean commitSession(long sessionId, BlobHandle handle, File staged) {
		File blob = new File(storeDir(), handle.fileName());

		forgetSession(sessionId);
		if (!staged.isFile())
			return false;
		blob.delete();
		return staged.renameTo(blob);
	}

	private void forgetSession(long sessionId) {
		synchronized (this) {
			sessions.remove(sessionId);
		}
	}
}
