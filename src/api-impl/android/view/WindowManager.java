package android.view;

import android.os.IBinder;

public interface WindowManager {
	public android.view.Display getDefaultDisplay();

	public WindowMetrics getCurrentWindowMetrics();

	public WindowMetrics getMaximumWindowMetrics();

	public void addView(View view, ViewGroup.LayoutParams params);

	public void updateViewLayout(View view, ViewGroup.LayoutParams params);

	public void removeView(View view);

	public void removeViewImmediate(View view);

	public class LayoutParams extends ViewGroup.LayoutParams {
		/* flag values match AOSP */
		public static final int FLAG_DIM_BEHIND = 0x00000002;
		public static final int FLAG_NOT_FOCUSABLE = 0x00000008;
		public static final int FLAG_NOT_TOUCHABLE = 0x00000010;
		public static final int FLAG_NOT_TOUCH_MODAL = 0x00000020;
		public static final int FLAG_KEEP_SCREEN_ON = 0x00000080;
		public static final int FLAG_LAYOUT_IN_SCREEN = 0x00000100;
		public static final int FLAG_FULLSCREEN = 0x00000400;
		public static final int FLAG_SECURE = 0x00002000;
		public static final int FLAG_ALT_FOCUSABLE_IM = 0x00020000;
		public static final int FLAG_WATCH_OUTSIDE_TOUCH = 0x00040000;
		public static final int FLAG_HARDWARE_ACCELERATED = 0x01000000;
		public static final int FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS = 0x80000000;

		/* window types (AOSP values) */
		public static final int FIRST_APPLICATION_WINDOW = 1;
		public static final int TYPE_BASE_APPLICATION = 1;
		public static final int TYPE_APPLICATION = 2;
		public static final int TYPE_APPLICATION_STARTING = 3;
		public static final int LAST_APPLICATION_WINDOW = 99;
		public static final int FIRST_SUB_WINDOW = 1000;
		public static final int TYPE_APPLICATION_PANEL = 1000;
		public static final int TYPE_APPLICATION_MEDIA = 1001;
		public static final int TYPE_APPLICATION_SUB_PANEL = 1002;
		public static final int TYPE_APPLICATION_ATTACHED_DIALOG = 1003;
		public static final int LAST_SUB_WINDOW = 1999;
		public static final int TYPE_TOAST = 2005;
		public static final int TYPE_APPLICATION_OVERLAY = 2038;

		public static final int SOFT_INPUT_ADJUST_RESIZE = 0x10;
		public static final int SOFT_INPUT_ADJUST_PAN = 0x20;
		public static final int SOFT_INPUT_ADJUST_NOTHING = 0x30;

		public float screenBrightness = -1;
		public int softInputMode;
		public int x;
		public int y;
		public int gravity;
		public float dimAmount = 1.0f;
		public int windowAnimations;
		public int flags;
		public float alpha = 1.0f;
		public int type = TYPE_APPLICATION;
		public IBinder token;
		public int format;
		public int layoutInDisplayCutoutMode;
		public String packageName;
		public CharSequence title;

		/* Floating-dialog width, resolved lazily at layout time (see
		 * ViewRootImpl.layoutPanel). When > 0 and width is WRAP_CONTENT the panel
		 * takes this fraction of the live window width (major = landscape window,
		 * minor = portrait), capped at Material's 560dp. Resolving at layout time
		 * instead of at show() time avoids baking a 0 width when a dialog is shown
		 * before the window has been measured. */
		public float floatingWidthMajor;
		public float floatingWidthMinor;

		public LayoutParams(int w, int h, int type, int flags, int format) {
			super(w, h);
			this.type = type;
			this.flags = flags;
			this.format = format;
		}

		public LayoutParams(int w, int h, int xpos, int ypos, int type, int flags, int format) {
			super(w, h);
			this.x = xpos;
			this.y = ypos;
			this.type = type;
			this.flags = flags;
			this.format = format;
		}

		public LayoutParams(int type, int flags, int format) {
			super(MATCH_PARENT, MATCH_PARENT);
			this.type = type;
			this.flags = flags;
			this.format = format;
		}

		public LayoutParams(int type) {
			this(type, 0, 0);
		}

		public LayoutParams() {
			super(MATCH_PARENT, MATCH_PARENT);
		}

		public void setTitle(CharSequence title) {
			this.title = title;
		}

		public CharSequence getTitle() {
			return title != null ? title : "";
		}

		public int copyFrom(LayoutParams o) {
			this.width = o.width;
			this.height = o.height;
			this.x = o.x;
			this.y = o.y;
			this.gravity = o.gravity;
			this.dimAmount = o.dimAmount;
			this.flags = o.flags;
			this.alpha = o.alpha;
			this.type = o.type;
			this.token = o.token;
			this.format = o.format;
			this.softInputMode = o.softInputMode;
			this.screenBrightness = o.screenBrightness;
			this.windowAnimations = o.windowAnimations;
			this.packageName = o.packageName;
			this.title = o.title;
			this.floatingWidthMajor = o.floatingWidthMajor;
			this.floatingWidthMinor = o.floatingWidthMinor;
			return 0;
		}
	}
}
