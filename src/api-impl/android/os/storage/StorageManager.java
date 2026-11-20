package android.os.storage;
import android.os.Environment;
import android.os.StatFs;
import java.io.IOException;
import java.util.UUID;


import java.io.File;

public class StorageManager {
	public StorageVolume[] getVolumeList() {
		StorageVolume[] sVolumes = {new StorageVolume()};
		return sVolumes;
	}

	public StorageVolume getStorageVolume(File file) { return null; }

	public static final UUID UUID_DEFAULT = UUID
	                                            .fromString("41217664-9172-527a-b3d5-edabb50a7d69");

	public UUID getUuidForPath(File path) throws IOException {
		return UUID_DEFAULT;
	}

	public long getCacheQuotaBytes(UUID storageUuid) throws IOException {
		return new StatFs(Environment.getDataDirectory().getPath()).getAvailableBytes();
	}
}
