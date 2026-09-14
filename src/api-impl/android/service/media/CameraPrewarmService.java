package android.service.media;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/*
 * The lock-screen camera "prewarm" service. Nothing on ATL ever starts one -
 * there is no lock screen - but Google Camera declares a subclass, and a class
 * its manifest names has to resolve or the whole component fails to load.
 */
public abstract class CameraPrewarmService extends Service {

	public static final String ACTION_PREWARM = "android.service.media.CameraPrewarmService.ACTION_PREWARM";

	@Override
	public IBinder onBind(Intent intent) {
		return null;
	}

	public void onPrewarm() {}

	public void onCooldown(boolean cameraIntentFired) {}
}
