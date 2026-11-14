package android.os;

public class PersistableBundle extends BaseBundle {

	public PersistableBundle() {
	}

	public PersistableBundle(PersistableBundle b) {
		mMap.putAll(b.mMap);
	}

	@Override
	public synchronized String toString() {
		return "PersistableBundle[" + mMap.toString() + "]";
	}
}
