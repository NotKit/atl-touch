package android.content.pm;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Icon;
import android.os.Parcelable;
import android.os.PersistableBundle;

public class ShortcutInfo implements Parcelable {
	public static final Creator<ShortcutInfo> CREATOR = null;

	public static class Builder {
		public Builder(Context context, String id) {}

		public ShortcutInfo build() {
			return new ShortcutInfo();
		}

		public Builder setIcon(Icon icon) {
			return this;
		}

		public Builder setIntent(Intent intent) {
			return this;
		}

		public Builder setIntents(Intent[] intents) {
			return this;
		}

		public Builder setRank(int rank) {
			return this;
		}

		public Builder setDisabledMessage(CharSequence disabledMessage) {
			return this;
		}

		public Builder setLongLived(boolean longLived) {
			return this;
		}

		public Builder setCategories(java.util.Set<String> categories) {
			return this;
		}

		public Builder setActivity(ComponentName activity) {
			return this;
		}

		public Builder setExtras(PersistableBundle extras) {
			return this;
		}

		public Builder setExcludedFromSurfaces(int surfaces) {
			return this;
		}

		public Builder setLongLabel(CharSequence longLabel) {
			return this;
		}

		public Builder setShortLabel(CharSequence shortLabel) {
			return this;
		}
	}
}
