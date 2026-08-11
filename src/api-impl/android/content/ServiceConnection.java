package android.content;

import android.os.IBinder;

public interface ServiceConnection {
	public void onServiceConnected(ComponentName name, IBinder service);

	public default void onServiceDisconnected(android.content.ComponentName a0) { }
}
