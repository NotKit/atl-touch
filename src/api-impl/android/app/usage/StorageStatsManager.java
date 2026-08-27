package android.app.usage;

import android.os.UserHandle;
import java.io.IOException;
import java.util.UUID;

/**
 * There is no storage-volume service here, so every figure is the free space
 * the filesystem reports. Fenix asks for these on a coroutine and its uncaught
 * handler exits the process when the class is missing.
 */
public class StorageStatsManager {

	public long getTotalBytes(UUID storageUuid) throws IOException {
		return new java.io.File("/").getTotalSpace();
	}

	public long getFreeBytes(UUID storageUuid) throws IOException {
		return new java.io.File("/").getUsableSpace();
	}

	public StorageStats queryStatsForPackage(UUID storageUuid, String packageName, UserHandle user)
	    throws IOException {
		return new StorageStats();
	}

	public StorageStats queryStatsForUid(UUID storageUuid, int uid) throws IOException {
		return new StorageStats();
	}

	public StorageStats queryStatsForUser(UUID storageUuid, UserHandle user) throws IOException {
		return new StorageStats();
	}
}
