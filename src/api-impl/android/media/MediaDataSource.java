package android.media;

import java.io.Closeable;
import java.io.IOException;

/**
 * Declaration only. Nothing here reads through a MediaDataSource: the class
 * exists because libraries name it in method signatures (androidx's
 * ExifInterface has a setDataSource overload taking one), and verifying a
 * caller resolves every type its callees name.
 */
public abstract class MediaDataSource implements Closeable {

	public abstract int readAt(long position, byte[] buffer, int offset, int size) throws IOException;

	public abstract long getSize() throws IOException;
}
