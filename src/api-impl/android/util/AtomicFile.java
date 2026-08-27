package android.util;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * AOSP's write-to-a-backup-then-rename file. android-components'
 * OnDiskMessageMetadataStorage uses it, and a missing class here is fatal:
 * Fenix's uncaught handler exits the process.
 */
public class AtomicFile {

	private final File baseName;
	private final File backupName;

	public AtomicFile(File baseName) {
		this.baseName = baseName;
		this.backupName = new File(baseName.getPath() + ".bak");
	}

	public File getBaseFile() {
		return baseName;
	}

	public void delete() {
		baseName.delete();
		backupName.delete();
	}

	public FileOutputStream startWrite() throws IOException {
		if (baseName.exists()) {
			if (!backupName.exists()) {
				if (!baseName.renameTo(backupName))
					throw new IOException("cannot rename " + baseName + " to " + backupName);
			} else {
				baseName.delete();
			}
		}
		try {
			return new FileOutputStream(baseName);
		} catch (FileNotFoundException e) {
			File parent = baseName.getParentFile();
			if (parent == null || !parent.mkdirs())
				throw new IOException("cannot create " + baseName, e);
			return new FileOutputStream(baseName);
		}
	}

	public void finishWrite(FileOutputStream str) {
		if (str == null)
			return;
		try {
			str.getFD().sync();
			str.close();
			backupName.delete();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	public void failWrite(FileOutputStream str) {
		if (str == null)
			return;
		try {
			str.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
		baseName.delete();
		backupName.renameTo(baseName);
	}

	public FileInputStream openRead() throws FileNotFoundException {
		if (backupName.exists()) {
			baseName.delete();
			backupName.renameTo(baseName);
		}
		return new FileInputStream(baseName);
	}

	public byte[] readFully() throws IOException {
		FileInputStream in = openRead();
		try {
			byte[] out = new byte[in.available()];
			int off = 0;
			while (off < out.length) {
				int n = in.read(out, off, out.length - off);
				if (n < 0)
					break;
				off += n;
			}
			return out;
		} finally {
			in.close();
		}
	}
}
