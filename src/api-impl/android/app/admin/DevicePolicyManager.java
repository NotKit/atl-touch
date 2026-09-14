package android.app.admin;

import android.content.ComponentName;

import java.util.Collections;
import java.util.List;

/**
 * Device policy. Nothing administers an ATL host, so there are no admins and
 * nothing is disabled - apps ask this before opening the camera.
 */
public class DevicePolicyManager {

	public boolean getCameraDisabled(ComponentName admin) {
		return false;
	}

	public List<ComponentName> getActiveAdmins() {
		return Collections.emptyList();
	}

	public boolean isProfileOwnerApp(String packageName) {
		return false;
	}

	public boolean isDeviceOwnerApp(String packageName) {
		return false;
	}

	public boolean isAdminActive(ComponentName admin) {
		return false;
	}
}
