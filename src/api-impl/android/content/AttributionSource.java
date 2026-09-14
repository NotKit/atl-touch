package android.content;

import android.os.Process;

/**
 * Who is asking, for the permission checks a real Android framework does on the
 * far side of a binder call.
 *
 * Nothing under ATL checks permissions across processes, so this only has to
 * describe this app truthfully: its uid, its package, and no next link in the
 * chain.
 */
public final class AttributionSource {

	private final int uid;
	private final int pid;
	private final String packageName;
	private final String attributionTag;

	AttributionSource(int uid, int pid, String packageName, String attributionTag) {
		this.uid = uid;
		this.pid = pid;
		this.packageName = packageName;
		this.attributionTag = attributionTag;
	}

	public int getUid() {
		return uid;
	}

	public int getPid() {
		return pid;
	}

	public String getPackageName() {
		return packageName;
	}

	public String getAttributionTag() {
		return attributionTag;
	}

	/** the chain stops here: an ATL app never delegates to another one */
	public AttributionSource getNext() {
		return null;
	}

	@Override
	public String toString() {
		return "AttributionSource(uid " + uid + ", " + packageName + ")";
	}

	public static final class Builder {
		private final int uid;
		private String packageName;
		private String attributionTag;

		public Builder(int uid) {
			this.uid = uid;
		}

		public Builder setPackageName(String packageName) {
			this.packageName = packageName;
			return this;
		}

		public Builder setAttributionTag(String attributionTag) {
			this.attributionTag = attributionTag;
			return this;
		}

		public AttributionSource build() {
			return new AttributionSource(uid, Process.myPid(), packageName, attributionTag);
		}
	}
}
