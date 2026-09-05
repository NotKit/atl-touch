package android.app;

import android.media.AudioAttributes;
import android.net.Uri;

public class NotificationChannel {

	private final String id;
	private final CharSequence name;
	private final int importance;

	public NotificationChannel(String id, CharSequence name, int importance) {
		this.id = id;
		this.name = name;
		this.importance = importance;
	}

	public String getId() { return id; }
	public CharSequence getName() { return name; }
	public int getImportance() { return importance; }

	public void setLockscreenVisibility(int a) {}
	public void setShowBadge(boolean a) {}
	public void setGroup(String grp) {}
	public void enableLights(boolean en) {}
	public void setLightColor(int color) {}
	public void setVibrationPattern(long[] pattern) {}
	public void enableVibration(boolean en) {}
	public void setSound(Uri uri, AudioAttributes attrs) {}
	public boolean shouldShowLights() { return false; }
	public int getLightColor() { return 0; }
	public boolean shouldVibrate() { return false; }
	public Uri getSound() { return null; }
	public void setDescription(String description) {}
	public void setBypassDnd(boolean bypassDnd) {}
}
