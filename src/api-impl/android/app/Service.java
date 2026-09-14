package android.app;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.os.IBinder;

public abstract class Service extends ContextWrapper {

	private int notification_id;

	public Service() {
		super(null);
	}

	public void onCreate() {
		System.out.println("Service.onCreate() called");
	}

	public void onDestroy() {
		System.out.println("Service.onDestroy() called");
	}

	public abstract IBinder onBind(Intent intent);

	public int onStartCommand(Intent intent, int flags, int startId) {
		System.out.println("Service.onStartCommand(" + intent + ", " + flags + ", " + startId + ") called");
		return 0;
	}

	public void startForeground(int id, Notification notification) {
		System.out.println("startForeground(" + id + ", " + notification + ") called");
		this.notification_id = id;
	}

	/* API 29's typed overload; nothing here enforces a foreground service type */
	public void startForeground(int id, Notification notification, int foregroundServiceType) {
		startForeground(id, notification);
	}

	public void stopForeground(boolean remove) {
		System.out.println("stopForeground(" + remove + ") called");
		if (remove)
			new NotificationManager().cancel(notification_id);
	}

	public void stopForeground(int remove) {
		stopForeground(remove == 1);
	}

	public Application getApplication() {
		return this.get_atl_loaded_app().getApplication();
	}

	/*
	 * Stopping is asynchronous, as on Android: the caller runs on, and
	 * onDestroy comes back on the main looper. Doing it inline would run
	 * onDestroy underneath whatever called stopSelf.
	 *
	 * Leaving these as no-ops kept a service alive forever. Telegram's call
	 * service was the case that showed it: nothing cleared its static
	 * instance, so the "return to call" bar stayed after the call ended and
	 * its failure handler ran once a second for as long as the app lived.
	 *
	 * Stopping only ends a *start*, though. A service that something is still
	 * bound to stays alive, as on Android -- see ATLLoadedApp.ServiceRecord.
	 */
	public void stopSelf(int startId) {
		stopSelfInternal(startId);
	}

	public void stopSelf() {
		stopSelfInternal(-1);
	}

	public boolean stopSelfResult(int startId) {
		stopSelfInternal(startId);
		return true;
	}

	private void stopSelfInternal(final int startId) {
		final Service self = this;
		new android.os.Handler(android.os.Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				Context.stopRunningService(self, startId);
			}
		});
	}

	public void attachBaseContext(Context newBase) {
		super.attachBaseContext(newBase);
		System.out.println("Service.attachBaseContext(" + newBase + ") called");
	}

	public boolean onUnbind(android.content.Intent a0) { return false; }
}
