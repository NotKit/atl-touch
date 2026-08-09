package android.view;

import android.graphics.Insets;

import java.util.Arrays;

public class WindowInsets {

	public static final WindowInsets CONSUMED = new WindowInsets();

	/* One entry per Type.indexOf(); null means "no inset of that type". We have no
	 * system bars or cutouts, so the soft keyboard is the only inset there normally
	 * is — and only when the layout isn't already resized around it (adjustPan /
	 * adjustNothing). Everything else exists so that androidx/Compose, which build
	 * and query WindowInsets by type from API 29 on, get real objects back. */
	private final Insets[] typeInsets;
	private final boolean[] typeVisible;
	private final DisplayCutout displayCutout;
	private final boolean round;

	public WindowInsets() {
		this(0);
	}

	WindowInsets(int imeBottom) {
		this(imeInsets(imeBottom), defaultVisibility(imeBottom > 0), null, false);
	}

	private WindowInsets(Insets[] typeInsets, boolean[] typeVisible, DisplayCutout cutout, boolean round) {
		this.typeInsets = typeInsets;
		this.typeVisible = typeVisible;
		this.displayCutout = cutout;
		this.round = round;
	}

	public WindowInsets(WindowInsets src) {
		this(src.typeInsets.clone(), src.typeVisible.clone(), src.displayCutout, src.round);
	}

	private static Insets[] imeInsets(int imeBottom) {
		Insets[] insets = new Insets[Type.SIZE];
		if (imeBottom > 0)
			insets[Type.indexOf(Type.IME)] = Insets.of(0, 0, 0, imeBottom);
		return insets;
	}

	private static boolean[] defaultVisibility(boolean imeVisible) {
		boolean[] visible = new boolean[Type.SIZE];
		for (int i = 0; i < Type.SIZE; i++)
			visible[i] = true;
		visible[Type.indexOf(Type.IME)] = imeVisible;
		return visible;
	}

	private static Insets insetOrNone(Insets insets) {
		return insets != null ? insets : Insets.NONE;
	}

	/* union of every requested type, as AOSP does */
	private Insets getInsets(Insets[] from, int typeMask, boolean ignoreVisibility) {
		Insets result = Insets.NONE;
		for (int type = Type.FIRST; type <= Type.LAST; type <<= 1) {
			if ((typeMask & type) == 0)
				continue;
			int i = Type.indexOf(type);
			if (!ignoreVisibility && !typeVisible[i])
				continue;
			result = Insets.max(result, insetOrNone(from[i]));
		}
		return result;
	}

	public Insets getInsets(int typeMask) {
		return getInsets(typeInsets, typeMask, false);
	}

	public Insets getInsetsIgnoringVisibility(int typeMask) {
		if ((typeMask & Type.IME) != 0)
			throw new IllegalArgumentException("Unable to query the maximum insets for IME");
		return getInsets(typeInsets, typeMask, true);
	}

	public boolean isVisible(int typeMask) {
		for (int type = Type.FIRST; type <= Type.LAST; type <<= 1) {
			if ((typeMask & type) != 0 && !typeVisible[Type.indexOf(type)])
				return false;
		}
		return true;
	}

	public WindowInsets consumeStableInsets() {
		return this;
	}

	public WindowInsets consumeSystemWindowInsets() {
		return this;
	}

	public WindowInsets replaceSystemWindowInsets(int left, int top, int right, int bottom) {
		Builder builder = new Builder(this);
		builder.setSystemWindowInsets(Insets.of(left, top, right, bottom));
		return builder.build();
	}

	public Insets getSystemWindowInsets() {
		return getInsets(Type.systemBars() | Type.IME);
	}

	public int getSystemWindowInsetLeft() {
		return getSystemWindowInsets().left;
	}

	public int getSystemWindowInsetTop() {
		return getSystemWindowInsets().top;
	}

	public int getSystemWindowInsetRight() {
		return getSystemWindowInsets().right;
	}

	public int getSystemWindowInsetBottom() {
		return getSystemWindowInsets().bottom;
	}

	public Insets getStableInsets() {
		return getInsets(typeInsets, Type.systemBars(), true);
	}

	public int getStableInsetLeft() {
		return getStableInsets().left;
	}

	public int getStableInsetTop() {
		return getStableInsets().top;
	}

	public int getStableInsetRight() {
		return getStableInsets().right;
	}

	public int getStableInsetBottom() {
		return getStableInsets().bottom;
	}

	public Insets getSystemGestureInsets() {
		return getInsets(Type.SYSTEM_GESTURES);
	}

	public Insets getMandatorySystemGestureInsets() {
		return getInsets(Type.MANDATORY_SYSTEM_GESTURES);
	}

	public Insets getTappableElementInsets() {
		return getInsets(Type.TAPPABLE_ELEMENT);
	}

	public DisplayCutout getDisplayCutout() {
		return displayCutout;
	}

	public boolean isRound() {
		return round;
	}

	public boolean isConsumed() {
		return false;
	}

	public WindowInsets consumeDisplayCutout() {
		return new WindowInsets(typeInsets, typeVisible, null, round);
	}

	/* shrink every inset by the given frame, clamping at zero */
	public WindowInsets inset(int left, int top, int right, int bottom) {
		Insets[] insets = new Insets[Type.SIZE];
		for (int i = 0; i < Type.SIZE; i++) {
			Insets in = typeInsets[i];
			if (in == null)
				continue;
			insets[i] = Insets.of(Math.max(0, in.left - left), Math.max(0, in.top - top),
			                      Math.max(0, in.right - right), Math.max(0, in.bottom - bottom));
		}
		return new WindowInsets(insets, typeVisible, displayCutout, round);
	}

	public WindowInsets inset(Insets insets) {
		return inset(insets.left, insets.top, insets.right, insets.bottom);
	}

	@Override
	public boolean equals(Object o) {
		if (this == o)
			return true;
		if (!(o instanceof WindowInsets))
			return false;
		WindowInsets other = (WindowInsets)o;
		return round == other.round && Arrays.equals(typeInsets, other.typeInsets)
		    && Arrays.equals(typeVisible, other.typeVisible)
		    && (displayCutout == null ? other.displayCutout == null
		                              : displayCutout.equals(other.displayCutout));
	}

	@Override
	public int hashCode() {
		return Arrays.hashCode(new Object[] {Arrays.hashCode(typeInsets),
		                                     Arrays.hashCode(typeVisible), displayCutout, round});
	}

	public static final class Builder {

		private final Insets[] typeInsets;
		private final boolean[] typeVisible;
		private DisplayCutout displayCutout;
		private boolean round;

		public Builder() {
			typeInsets = new Insets[Type.SIZE];
			typeVisible = defaultVisibility(false);
		}

		public Builder(WindowInsets insets) {
			typeInsets = insets.typeInsets.clone();
			typeVisible = insets.typeVisible.clone();
			displayCutout = insets.displayCutout;
			round = insets.round;
		}

		private Builder setInsets(int typeMask, Insets insets, boolean visible) {
			for (int type = Type.FIRST; type <= Type.LAST; type <<= 1) {
				if ((typeMask & type) == 0)
					continue;
				int i = Type.indexOf(type);
				typeInsets[i] = insets;
				typeVisible[i] = visible;
			}
			return this;
		}

		public Builder setInsets(int typeMask, Insets insets) {
			return setInsets(typeMask, insets, true);
		}

		public Builder setInsetsIgnoringVisibility(int typeMask, Insets insets) {
			if ((typeMask & Type.IME) != 0)
				throw new IllegalArgumentException("Unable to modify maximum insets for IME");
			for (int type = Type.FIRST; type <= Type.LAST; type <<= 1) {
				if ((typeMask & type) != 0)
					typeInsets[Type.indexOf(type)] = insets;
			}
			return this;
		}

		public Builder setVisible(int typeMask, boolean visible) {
			for (int type = Type.FIRST; type <= Type.LAST; type <<= 1) {
				if ((typeMask & type) != 0)
					typeVisible[Type.indexOf(type)] = visible;
			}
			return this;
		}

		public Builder setSystemWindowInsets(Insets insets) {
			return setInsets(Type.systemBars(), insets);
		}

		public Builder setStableInsets(Insets insets) {
			return setInsets(Type.systemBars(), insets);
		}

		public Builder setSystemGestureInsets(Insets insets) {
			return setInsets(Type.SYSTEM_GESTURES, insets);
		}

		public Builder setMandatorySystemGestureInsets(Insets insets) {
			return setInsets(Type.MANDATORY_SYSTEM_GESTURES, insets);
		}

		public Builder setTappableElementInsets(Insets insets) {
			return setInsets(Type.TAPPABLE_ELEMENT, insets);
		}

		public Builder setDisplayCutout(DisplayCutout cutout) {
			displayCutout = cutout;
			return this;
		}

		public Builder setRound(boolean round) {
			this.round = round;
			return this;
		}

		public WindowInsets build() {
			return new WindowInsets(typeInsets.clone(), typeVisible.clone(), displayCutout, round);
		}
	}

	/* Copyright (C) 2014 The Android Open Source Project */
	public static final class Type {

		static final int FIRST = 1 << 0;
		static final int STATUS_BARS = FIRST;
		static final int NAVIGATION_BARS = 1 << 1;
		static final int CAPTION_BAR = 1 << 2;

		static final int IME = 1 << 3;

		static final int SYSTEM_GESTURES = 1 << 4;
		static final int MANDATORY_SYSTEM_GESTURES = 1 << 5;
		static final int TAPPABLE_ELEMENT = 1 << 6;

		static final int DISPLAY_CUTOUT = 1 << 7;

		static final int WINDOW_DECOR = 1 << 8;

		static final int SYSTEM_OVERLAYS = 1 << 9;
		static final int LAST = SYSTEM_OVERLAYS;
		static final int SIZE = 10;

		static final int DEFAULT_VISIBLE = ~IME;

		static int indexOf(@InsetsType int type) {
			switch (type) {
				case STATUS_BARS:
					return 0;
				case NAVIGATION_BARS:
					return 1;
				case CAPTION_BAR:
					return 2;
				case IME:
					return 3;
				case SYSTEM_GESTURES:
					return 4;
				case MANDATORY_SYSTEM_GESTURES:
					return 5;
				case TAPPABLE_ELEMENT:
					return 6;
				case DISPLAY_CUTOUT:
					return 7;
				case WINDOW_DECOR:
					return 8;
				case SYSTEM_OVERLAYS:
					return 9;
				default:
					throw new IllegalArgumentException("type needs to be >= FIRST and <= LAST,"
					                                   + " type=" + type);
			}
		}

		/**
		 * @hide
		 */
		public static String toString(@InsetsType int types) {
			StringBuilder result = new StringBuilder();
			if ((types & STATUS_BARS) != 0) {
				result.append("statusBars ");
			}
			if ((types & NAVIGATION_BARS) != 0) {
				result.append("navigationBars ");
			}
			if ((types & CAPTION_BAR) != 0) {
				result.append("captionBar ");
			}
			if ((types & IME) != 0) {
				result.append("ime ");
			}
			if ((types & SYSTEM_GESTURES) != 0) {
				result.append("systemGestures ");
			}
			if ((types & MANDATORY_SYSTEM_GESTURES) != 0) {
				result.append("mandatorySystemGestures ");
			}
			if ((types & TAPPABLE_ELEMENT) != 0) {
				result.append("tappableElement ");
			}
			if ((types & DISPLAY_CUTOUT) != 0) {
				result.append("displayCutout ");
			}
			if ((types & WINDOW_DECOR) != 0) {
				result.append("windowDecor ");
			}
			if ((types & SYSTEM_OVERLAYS) != 0) {
				result.append("systemOverlays ");
			}
			if (result.length() > 0) {
				result.delete(result.length() - 1, result.length());
			}
			return result.toString();
		}

		private Type() {
		}

		/**
		 * @hide
		 */
		public @interface InsetsType {
		}

		/**
		 * @return An insets type representing any system bars for displaying status.
		 */
		public static @InsetsType int statusBars() {
			return STATUS_BARS;
		}

		/**
		 * @return An insets type representing any system bars for navigation.
		 */
		public static @InsetsType int navigationBars() {
			return NAVIGATION_BARS;
		}

		/**
		 * @return An insets type representing the window of a caption bar.
		 */
		public static @InsetsType int captionBar() {
			return CAPTION_BAR;
		}

		/**
		 * @return An insets type representing the window of an {@link InputMethod}.
		 */
		public static @InsetsType int ime() {
			return IME;
		}

		/**
		 * Returns an insets type representing the system gesture insets.
		 *
		 * <p>The system gesture insets represent the area of a window where system gestures have
		 * priority and may consume some or all touch input, e.g. due to the a system bar
		 * occupying it, or it being reserved for touch-only gestures.
		 *
		 * <p>Simple taps are guaranteed to reach the window even within the system gesture insets,
		 * as long as they are outside the {@link #getSystemWindowInsets() system window insets}.
		 *
		 * <p>When {@link View#SYSTEM_UI_FLAG_LAYOUT_STABLE} is requested, an inset will be returned
		 * even when the system gestures are inactive due to
		 * {@link View#SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN} or
		 * {@link View#SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION}.
		 *
		 * @see #getSystemGestureInsets()
		 */
		public static @InsetsType int systemGestures() {
			return SYSTEM_GESTURES;
		}

		/**
		 * @see #getMandatorySystemGestureInsets
		 */
		public static @InsetsType int mandatorySystemGestures() {
			return MANDATORY_SYSTEM_GESTURES;
		}

		/**
		 * @see #getTappableElementInsets
		 */
		public static @InsetsType int tappableElement() {
			return TAPPABLE_ELEMENT;
		}

		/**
		 * Returns an insets type representing the area that used by {@link DisplayCutout}.
		 *
		 * <p>This is equivalent to the safe insets on {@link #getDisplayCutout()}.
		 *
		 * <p>Note: During dispatch to {@link View#onApplyWindowInsets}, if the window is using
		 * the {@link WindowManager.LayoutParams#LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT default}
		 * {@link WindowManager.LayoutParams#layoutInDisplayCutoutMode}, {@link #getDisplayCutout()}
		 * will return {@code null} even if the window overlaps a display cutout area, in which case
		 * the {@link #displayCutout() displayCutout() inset} will still report the accurate value.
		 *
		 * @see DisplayCutout#getSafeInsetLeft()
		 * @see DisplayCutout#getSafeInsetTop()
		 * @see DisplayCutout#getSafeInsetRight()
		 * @see DisplayCutout#getSafeInsetBottom()
		 */
		public static @InsetsType int displayCutout() {
			return DISPLAY_CUTOUT;
		}

		/**
		 * System overlays represent the insets caused by the system visible elements. Unlike
		 * {@link #navigationBars()} or {@link #statusBars()}, system overlays might not be
		 * hidden by the client.
		 *
		 * For compatibility reasons, this type is included in {@link #systemBars()}. In this
		 * way, views which fit {@link #systemBars()} fit {@link #systemOverlays()}.
		 *
		 * Examples include climate controls, multi-tasking affordances, etc.
		 *
		 * @return An insets type representing the system overlays.
		 */
		public static @InsetsType int systemOverlays() {
			return SYSTEM_OVERLAYS;
		}

		/**
		 * @return All system bars. Includes {@link #statusBars()}, {@link #captionBar()} as well as
		 *         {@link #navigationBars()}, {@link #systemOverlays()}, but not {@link #ime()}.
		 */
		public static @InsetsType int systemBars() {
			return STATUS_BARS | NAVIGATION_BARS | CAPTION_BAR | SYSTEM_OVERLAYS;
		}

		/**
		 * @return Default visible types.
		 *
		 * @hide
		 */
		public static @InsetsType int defaultVisible() {
			return DEFAULT_VISIBLE;
		}

		/**
		 * @return All inset types combined.
		 *
		 * @hide
		 */
		public static @InsetsType int all() {
			return 0xFFFFFFFF;
		}

		/**
		 * @return System bars which can be controlled by {@link View.SystemUiVisibility}.
		 *
		 * @hide
		 */
		public static boolean hasCompatSystemBars(@InsetsType int types) {
			return (types & (STATUS_BARS | NAVIGATION_BARS)) != 0;
		}
	}
}
