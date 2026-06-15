package android.content.pm;

import java.util.Collections;
import java.util.List;

public class ShortcutManager {
	public void removeAllDynamicShortcuts() {
	}

	public List getShortcuts(int matchFlags) {
		return Collections.emptyList();
	}
	public void removeLongLivedShortcuts(List<String> shortcutIds) {
	}

	public boolean setDynamicShortcuts(List<ShortcutInfo> shortcutInfoList) {
		return true;
	}

	public boolean addDynamicShortcuts(List<ShortcutInfo> shortcutInfoList) {
		return true;
	}

	public List<ShortcutInfo> getDynamicShortcuts() {
		return Collections.emptyList();
	}

	public List<ShortcutInfo> getManifestShortcuts() {
		return Collections.emptyList();
	}

	public void removeDynamicShortcuts(List<String> shortcutIds) {
	}

	public boolean updateShortcuts(List<ShortcutInfo> shortcutInfoList) {
		return true;
	}

	public void disableShortcuts(List<String> shortcutIds) {
	}

	public void enableShortcuts(List<String> shortcutIds) {
	}

	public void reportShortcutUsed(String shortcutId) {
	}

	public boolean isRateLimitingActive() {
		return false;
	}

	public int getMaxShortcutCountPerActivity() {
		return 5;
	}

	public boolean isRequestPinShortcutSupported() {
		return false;
	}

	public List<ShortcutInfo> getPinnedShortcuts() {
		return Collections.emptyList();
	}
}
