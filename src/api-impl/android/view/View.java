package android.view;

import android.R;
import android.animation.AnimatorInflater;
import android.animation.StateListAnimator;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Outline;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Parcelable;
import android.os.SystemClock;
import android.os.Vibrator;
import android.util.AttributeSet;
import android.util.FloatProperty;
import android.util.LayoutDirection;
import android.util.Property;
import android.util.Slog;
import android.util.SparseArray;
import android.util.TypedValue;
import android.view.animation.Animation;
import android.view.autofill.AutofillId;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.translation.ViewTranslationCallback;
import java.lang.CharSequence;
import java.lang.ref.WeakReference;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicInteger;

class WindowId {}

public class View implements Drawable.Callback {

	// --- constants from android source

	/**
	 * The logging tag used by this class with android.util.Log.
	 */
	protected static final String TAG = "View";

	/**
	 * When set to true, apps will draw debugging information about their layouts.
	 *
	 * @hide
	 */
	public static final String DEBUG_LAYOUT_PROPERTY = "debug.layout";

	/**
	 * Used to mark a View that has no ID.
	 */
	public static final int NO_ID = -1;

	/**
	 * Signals that compatibility booleans have been initialized according to
	 * target SDK versions.
	 */
	private static boolean sCompatibilityDone = false;

	/**
	 * Use the old (broken) way of building MeasureSpecs.
	 */
	private static boolean sUseBrokenMakeMeasureSpec = false;

	/**
	 * Ignore any optimizations using the measure cache.
	 */
	private static boolean sIgnoreMeasureCache = false;

	/**
	 * This view does not want keystrokes. Use with TAKES_FOCUS_MASK when
	 * calling setFlags.
	 */
	private static final int NOT_FOCUSABLE = 0x00000000;

	/**
	 * This view wants keystrokes. Use with TAKES_FOCUS_MASK when calling
	 * setFlags.
	 */
	private static final int FOCUSABLE = 0x00000001;

	/**
	 * Mask for use with setFlags indicating bits used for focus.
	 */
	private static final int FOCUSABLE_MASK = 0x00000001;

	/**
	 * This view will adjust its padding to fit sytem windows (e.g. status bar)
	 */
	private static final int FITS_SYSTEM_WINDOWS = 0x00000002;

	/**
	 * This view is visible.
	 * Use with {@link #setVisibility} and <a href="#attr_android:visibility">{@code
	 * android:visibility}.
	 */
	public static final int VISIBLE = 0x00000000;

	/**
	 * This view is invisible, but it still takes up space for layout purposes.
	 * Use with {@link #setVisibility} and <a href="#attr_android:visibility">{@code
	 * android:visibility}.
	 */
	public static final int INVISIBLE = 0x00000004;

	/**
	 * This view is invisible, and it doesn't take any space for layout
	 * purposes. Use with {@link #setVisibility} and <a href="#attr_android:visibility">{@code
	 * android:visibility}.
	 */
	public static final int GONE = 0x00000008;

	public static final int LAYER_TYPE_NONE = 0;
	public static final int LAYER_TYPE_SOFTWARE = 1;
	public static final int LAYER_TYPE_HARDWARE = 2;

	/**
	 * Mask for use with setFlags indicating bits used for visibility.
	 * {@hide}
	 */
	static final int VISIBILITY_MASK = 0x0000000C;

	private static final int[] VISIBILITY_FLAGS = {VISIBLE, INVISIBLE, GONE};

	/**
	 * This view is enabled. Interpretation varies by subclass.
	 * Use with ENABLED_MASK when calling setFlags.
	 * {@hide}
	 */
	static final int ENABLED = 0x00000000;

	/**
	 * This view is disabled. Interpretation varies by subclass.
	 * Use with ENABLED_MASK when calling setFlags.
	 * {@hide}
	 */
	static final int DISABLED = 0x00000020;

	/**
	 * Mask for use with setFlags indicating bits used for indicating whether
	 * this view is enabled
	 * {@hide}
	 */
	static final int ENABLED_MASK = 0x00000020;

	/**
	 * This view won't draw. {@link #onDraw(android.graphics.Canvas)} won't be
	 * called and further optimizations will be performed. It is okay to have
	 * this flag set and a background. Use with DRAW_MASK when calling setFlags.
	 * {@hide}
	 */
	static final int WILL_NOT_DRAW = 0x00000080;

	/**
	 * Mask for use with setFlags indicating bits used for indicating whether
	 * this view is will draw
	 * {@hide}
	 */
	static final int DRAW_MASK = 0x00000080;

	/**
	 * <p>This view doesn't show scrollbars.</p>
	 * {@hide}
	 */
	static final int SCROLLBARS_NONE = 0x00000000;

	/**
	 * <p>This view shows horizontal scrollbars.</p>
	 * {@hide}
	 */
	static final int SCROLLBARS_HORIZONTAL = 0x00000100;

	/**
	 * <p>This view shows vertical scrollbars.</p>
	 * {@hide}
	 */
	static final int SCROLLBARS_VERTICAL = 0x00000200;

	/**
	 * <p>Mask for use with setFlags indicating bits used for indicating which
	 * scrollbars are enabled.</p>
	 * {@hide}
	 */
	static final int SCROLLBARS_MASK = 0x00000300;

	/**
	 * Indicates that the view should filter touches when its window is obscured.
	 * Refer to the class comments for more information about this security feature.
	 * {@hide}
	 */
	static final int FILTER_TOUCHES_WHEN_OBSCURED = 0x00000400;

	/**
	 * Set for framework elements that use FITS_SYSTEM_WINDOWS, to indicate
	 * that they are optional and should be skipped if the window has
	 * requested system UI flags that ignore those insets for layout.
	 */
	static final int OPTIONAL_FITS_SYSTEM_WINDOWS = 0x00000800;

	/**
	 * <p>This view doesn't show fading edges.</p>
	 * {@hide}
	 */
	static final int FADING_EDGE_NONE = 0x00000000;

	/**
	 * <p>This view shows horizontal fading edges.</p>
	 * {@hide}
	 */
	static final int FADING_EDGE_HORIZONTAL = 0x00001000;

	/**
	 * <p>This view shows vertical fading edges.</p>
	 * {@hide}
	 */
	static final int FADING_EDGE_VERTICAL = 0x00002000;

	/**
	 * <p>Mask for use with setFlags indicating bits used for indicating which
	 * fading edges are enabled.</p>
	 * {@hide}
	 */
	static final int FADING_EDGE_MASK = 0x00003000;

	/**
	 * <p>Indicates this view can be clicked. When clickable, a View reacts
	 * to clicks by notifying the OnClickListener.<p>
	 * {@hide}
	 */
	static final int CLICKABLE = 0x00004000;

	/**
	 * <p>Indicates this view is caching its drawing into a bitmap.</p>
	 * {@hide}
	 */
	static final int DRAWING_CACHE_ENABLED = 0x00008000;

	/**
	 * <p>Indicates that no icicle should be saved for this view.<p>
	 * {@hide}
	 */
	static final int SAVE_DISABLED = 0x000010000;

	/**
	 * <p>Mask for use with setFlags indicating bits used for the saveEnabled
	 * property.</p>
	 * {@hide}
	 */
	static final int SAVE_DISABLED_MASK = 0x000010000;

	/**
	 * <p>Indicates that no drawing cache should ever be created for this view.<p>
	 * {@hide}
	 */
	static final int WILL_NOT_CACHE_DRAWING = 0x000020000;

	/**
	 * <p>Indicates this view can take / keep focus when int touch mode.</p>
	 * {@hide}
	 */
	static final int FOCUSABLE_IN_TOUCH_MODE = 0x00040000;

	/**
	 * <p>Enables low quality mode for the drawing cache.</p>
	 */
	public static final int DRAWING_CACHE_QUALITY_LOW = 0x00080000;

	/**
	 * <p>Enables high quality mode for the drawing cache.</p>
	 */
	public static final int DRAWING_CACHE_QUALITY_HIGH = 0x00100000;

	/**
	 * <p>Enables automatic quality mode for the drawing cache.</p>
	 */
	public static final int DRAWING_CACHE_QUALITY_AUTO = 0x00000000;

	private static final int[] DRAWING_CACHE_QUALITY_FLAGS = {
		DRAWING_CACHE_QUALITY_AUTO, DRAWING_CACHE_QUALITY_LOW, DRAWING_CACHE_QUALITY_HIGH
	};

	/**
	 * <p>Mask for use with setFlags indicating bits used for the cache
	 * quality property.</p>
	 * {@hide}
	 */
	static final int DRAWING_CACHE_QUALITY_MASK = 0x00180000;

	/**
	 * <p>
	 * Indicates this view can be long clicked. When long clickable, a View
	 * reacts to long clicks by notifying the OnLongClickListener or showing a
	 * context menu.
	 * </p>
	 * {@hide}
	 */
	static final int LONG_CLICKABLE = 0x00200000;

	/**
	 * <p>Indicates that this view gets its drawable states from its direct parent
	 * and ignores its original internal states.</p>
	 *
	 * @hide
	 */
	static final int DUPLICATE_PARENT_STATE = 0x00400000;

	/**
	 * The scrollbar style to display the scrollbars inside the content area,
	 * without increasing the padding. The scrollbars will be overlaid with
	 * translucency on the view's content.
	 */
	public static final int SCROLLBARS_INSIDE_OVERLAY = 0;

	/**
	 * The scrollbar style to display the scrollbars inside the padded area,
	 * increasing the padding of the view. The scrollbars will not overlap the
	 * content area of the view.
	 */
	public static final int SCROLLBARS_INSIDE_INSET = 0x01000000;

	/**
	 * The scrollbar style to display the scrollbars at the edge of the view,
	 * without increasing the padding. The scrollbars will be overlaid with
	 * translucency.
	 */
	public static final int SCROLLBARS_OUTSIDE_OVERLAY = 0x02000000;

	/**
	 * The scrollbar style to display the scrollbars at the edge of the view,
	 * increasing the padding of the view. The scrollbars will only overlap the
	 * background, if any.
	 */
	public static final int SCROLLBARS_OUTSIDE_INSET = 0x03000000;

	/**
	 * Mask to check if the scrollbar style is overlay or inset.
	 * {@hide}
	 */
	static final int SCROLLBARS_INSET_MASK = 0x01000000;

	/**
	 * Mask to check if the scrollbar style is inside or outside.
	 * {@hide}
	 */
	static final int SCROLLBARS_OUTSIDE_MASK = 0x02000000;

	/**
	 * Mask for scrollbar style.
	 * {@hide}
	 */
	static final int SCROLLBARS_STYLE_MASK = 0x03000000;

	/**
	 * View flag indicating that the screen should remain on while the
	 * window containing this view is visible to the user.  This effectively
	 * takes care of automatically setting the WindowManager's
	 * {@link WindowManager.LayoutParams#FLAG_KEEP_SCREEN_ON}.
	 */
	public static final int KEEP_SCREEN_ON = 0x04000000;

	/**
	 * View flag indicating whether this view should have sound effects enabled
	 * for events such as clicking and touching.
	 */
	public static final int SOUND_EFFECTS_ENABLED = 0x08000000;

	/**
	 * View flag indicating whether this view should have haptic feedback
	 * enabled for events such as long presses.
	 */
	public static final int HAPTIC_FEEDBACK_ENABLED = 0x10000000;

	/**
	 * <p>Indicates that the view hierarchy should stop saving state when
	 * it reaches this view.  If state saving is initiated immediately at
	 * the view, it will be allowed.
	 * {@hide}
	 */
	static final int PARENT_SAVE_DISABLED = 0x20000000;

	/**
	 * <p>Mask for use with setFlags indicating bits used for PARENT_SAVE_DISABLED.</p>
	 * {@hide}
	 */
	static final int PARENT_SAVE_DISABLED_MASK = 0x20000000;

	/**
	 * View flag indicating whether {@link #addFocusables(ArrayList, int, int)}
	 * should add all focusable Views regardless if they are focusable in touch mode.
	 */
	public static final int FOCUSABLES_ALL = 0x00000000;

	/**
	 * View flag indicating whether {@link #addFocusables(ArrayList, int, int)}
	 * should add only Views focusable in touch mode.
	 */
	public static final int FOCUSABLES_TOUCH_MODE = 0x00000001;

	/**
	 * Use with {@link #focusSearch(int)}. Move focus to the previous selectable
	 * item.
	 */
	public static final int FOCUS_BACKWARD = 0x00000001;

	/**
	 * Use with {@link #focusSearch(int)}. Move focus to the next selectable
	 * item.
	 */
	public static final int FOCUS_FORWARD = 0x00000002;

	/**
	 * Use with {@link #focusSearch(int)}. Move focus to the left.
	 */
	public static final int FOCUS_LEFT = 0x00000011;

	/**
	 * Use with {@link #focusSearch(int)}. Move focus up.
	 */
	public static final int FOCUS_UP = 0x00000021;

	/**
	 * Use with {@link #focusSearch(int)}. Move focus to the right.
	 */
	public static final int FOCUS_RIGHT = 0x00000042;

	/**
	 * Use with {@link #focusSearch(int)}. Move focus down.
	 */
	public static final int FOCUS_DOWN = 0x00000082;

	/**
	 * Bits of {@link #getMeasuredWidthAndState()} and
	 * {@link #getMeasuredWidthAndState()} that provide the actual measured size.
	 */
	public static final int MEASURED_SIZE_MASK = 0x00ffffff;

	/**
	 * Bits of {@link #getMeasuredWidthAndState()} and
	 * {@link #getMeasuredWidthAndState()} that provide the additional state bits.
	 */
	public static final int MEASURED_STATE_MASK = 0xff000000;

	/**
	 * Bit shift of {@link #MEASURED_STATE_MASK} to get to the height bits
	 * for functions that combine both width and height into a single int,
	 * such as {@link #getMeasuredState()} and the childState argument of
	 * {@link #resolveSizeAndState(int, int, int)}.
	 */
	public static final int MEASURED_HEIGHT_STATE_SHIFT = 16;

	/**
	 * Bit of {@link #getMeasuredWidthAndState()} and
	 * {@link #getMeasuredWidthAndState()} that indicates the measured size
	 * is smaller that the space the view would like to have.
	 */
	public static final int MEASURED_STATE_TOO_SMALL = 0x01000000;

	// --- and some flags

	/**
	 * Indicates that this view has reported that it can accept the current drag's content.
	 * Cleared when the drag operation concludes.
	 * @hide
	 */
	static final int PFLAG2_DRAG_CAN_ACCEPT = 0x00000001;

	/**
	 * Indicates that this view is currently directly under the drag location in a
	 * drag-and-drop operation involving content that it can accept.  Cleared when
	 * the drag exits the view, or when the drag operation concludes.
	 * @hide
	 */
	static final int PFLAG2_DRAG_HOVERED = 0x00000002;

	/**
	 * Horizontal layout direction of this view is from Left to Right.
	 * Use with {@link #setLayoutDirection}.
	 */
	public static final int LAYOUT_DIRECTION_LTR = LayoutDirection.LTR;

	/**
	 * Horizontal layout direction of this view is from Right to Left.
	 * Use with {@link #setLayoutDirection}.
	 */
	public static final int LAYOUT_DIRECTION_RTL = LayoutDirection.RTL;

	/**
	 * Horizontal layout direction of this view is inherited from its parent.
	 * Use with {@link #setLayoutDirection}.
	 */
	public static final int LAYOUT_DIRECTION_INHERIT = LayoutDirection.INHERIT;

	/**
	 * Horizontal layout direction of this view is from deduced from the default language
	 * script for the locale. Use with {@link #setLayoutDirection}.
	 */
	public static final int LAYOUT_DIRECTION_LOCALE = LayoutDirection.LOCALE;

	/**
	 * Bit shift to get the horizontal layout direction. (bits after DRAG_HOVERED)
	 * @hide
	 */
	static final int PFLAG2_LAYOUT_DIRECTION_MASK_SHIFT = 2;

	/**
	 * Mask for use with private flags indicating bits used for horizontal layout direction.
	 * @hide
	 */
	static final int PFLAG2_LAYOUT_DIRECTION_MASK = 0x00000003 << PFLAG2_LAYOUT_DIRECTION_MASK_SHIFT;

	/**
	 * Indicates whether the view horizontal layout direction has been resolved and drawn to the
	 * right-to-left direction.
	 * @hide
	 */
	static final int PFLAG2_LAYOUT_DIRECTION_RESOLVED_RTL = 4 << PFLAG2_LAYOUT_DIRECTION_MASK_SHIFT;

	/**
	 * Indicates whether the view horizontal layout direction has been resolved.
	 * @hide
	 */
	static final int PFLAG2_LAYOUT_DIRECTION_RESOLVED = 8 << PFLAG2_LAYOUT_DIRECTION_MASK_SHIFT;

	/**
	 * Mask for use with private flags indicating bits used for resolved horizontal layout direction.
	 * @hide
	 */
	static final int PFLAG2_LAYOUT_DIRECTION_RESOLVED_MASK = 0x0000000C << PFLAG2_LAYOUT_DIRECTION_MASK_SHIFT;

	/**
	 * Indicates no axis of view scrolling.
	 */
	public static final int SCROLL_AXIS_NONE = 0;

	/**
	 * Indicates scrolling along the horizontal axis.
	 */
	public static final int SCROLL_AXIS_HORIZONTAL = 1 << 0;

	/**
	 * Indicates scrolling along the vertical axis.
	 */
	public static final int SCROLL_AXIS_VERTICAL = 1 << 1;

	/**
	 * Always allow a user to over-scroll this view, provided it is a
	 * view that can scroll.
	 *
	 * @see #getOverScrollMode()
	 * @see #setOverScrollMode(int)
	 */
	public static final int OVER_SCROLL_ALWAYS = 0;

	/**
	 * Allow a user to over-scroll this view only if the content is large
	 * enough to meaningfully scroll, provided it is a view that can scroll.
	 *
	 * @see #getOverScrollMode()
	 * @see #setOverScrollMode(int)
	 */
	public static final int OVER_SCROLL_IF_CONTENT_SCROLLS = 1;

	/**
	 * Never allow a user to over-scroll this view.
	 *
	 * @see #getOverScrollMode()
	 * @see #setOverScrollMode(int)
	 */
	public static final int OVER_SCROLL_NEVER = 2;

	// --- apparently there's more...

	@Deprecated
	public static final int STATUS_BAR_HIDDEN = 1; // 0x1

	@Deprecated
	public static final int STATUS_BAR_VISIBLE = 0; // 0x0

	public static final int SYSTEM_UI_FLAG_FULLSCREEN = 4; // 0x4

	public static final int SYSTEM_UI_FLAG_HIDE_NAVIGATION = 2; // 0x2

	public static final int SYSTEM_UI_FLAG_IMMERSIVE = 2048; // 0x800

	public static final int SYSTEM_UI_FLAG_IMMERSIVE_STICKY = 4096; // 0x1000

	public static final int SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = 1024; // 0x400

	public static final int SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 512; // 0x200

	public static final int SYSTEM_UI_FLAG_LAYOUT_STABLE = 256; // 0x100

	public static final int SYSTEM_UI_FLAG_LOW_PROFILE = 1; // 0x1

	public static final int SYSTEM_UI_FLAG_VISIBLE = 0; // 0x0

	public static final int SYSTEM_UI_LAYOUT_FLAGS = 1536; // 0x600

	public static final int TEXT_ALIGNMENT_CENTER = 4; // 0x4

	public static final int TEXT_ALIGNMENT_GRAVITY = 1; // 0x1

	public static final int TEXT_ALIGNMENT_INHERIT = 0; // 0x0

	public static final int TEXT_ALIGNMENT_TEXT_END = 3; // 0x3

	public static final int TEXT_ALIGNMENT_TEXT_START = 2; // 0x2

	public static final int TEXT_ALIGNMENT_VIEW_END = 6; // 0x6

	public static final int TEXT_ALIGNMENT_VIEW_START = 5; // 0x5

	public static final int TEXT_DIRECTION_ANY_RTL = 2; // 0x2

	public static final int TEXT_DIRECTION_FIRST_STRONG = 1; // 0x1

	public static final int TEXT_DIRECTION_INHERIT = 0; // 0x0

	public static final int TEXT_DIRECTION_LOCALE = 5; // 0x5

	public static final int TEXT_DIRECTION_LTR = 3; // 0x3

	public static final int TEXT_DIRECTION_RTL = 4; // 0x4

	public static final Property<View, Float> SCALE_X = new FloatProperty<View>("scaleX") {
		@Override
		public void setValue(View object, float value) {
			object.setScaleX(value);
		}
		@Override
		public Float get(View object) {
			return object.getScaleX();
		}
	};

	public static final Property<View, Float> SCALE_Y = new FloatProperty<View>("scaleY") {
		@Override
		public void setValue(View object, float value) {
			object.setScaleY(value);
		}
		@Override
		public Float get(View object) {
			return object.getScaleY();
		}
	};

	// --- end of constants from android source

	// --- interfaces from android source
	public interface OnTouchListener {
		/**
		 * Called when a touch event is dispatched to a view. This allows listeners to
		 * get a chance to respond before the target view.
		 *
		 * @param v The view the touch event has been dispatched to.
		 * @param event The MotionEvent object containing full information about
		 *        the event.
		 * @return True if the listener has consumed the event, false otherwise.
		 */
		boolean onTouch(View v, MotionEvent event);
	}

	public interface OnScrollChangeListener {
		void onScrollChange(View v, int scrollX, int scrollY, int oldScrollX, int oldScrollY);
	}

	public interface OnClickListener {
		void onClick(View v);
	}

	public interface OnAttachStateChangeListener {
		public void onViewAttachedToWindow(View v);
		public void onViewDetachedFromWindow(View v);
	}

	public interface OnGenericMotionListener {
		boolean onGenericMotion(View v, MotionEvent event);
	}

	public interface OnSystemUiVisibilityChangeListener {
		// TODO
	}

	public static interface OnKeyListener {
		boolean onKey(View v, int keyCode, KeyEvent event);
	}

	public interface OnLongClickListener {
		public boolean onLongClick(View v);
	}

	public interface OnContextClickListener {
		boolean onContextClick(View v);
	}

	public interface OnHoverListener {
		boolean onHover(View v, MotionEvent event);
	}

	public interface OnApplyWindowInsetsListener {
		WindowInsets onApplyWindowInsets(View v, WindowInsets insets);
	}

	public interface OnLayoutChangeListener {
		void onLayoutChange(View v, int left, int top, int right, int bottom,
				int oldLeft, int oldTop, int oldRight, int oldBottom);
	}

	public interface OnUnhandledKeyEventListener {
		// TODO
	}

	public interface OnFocusChangeListener {
		void onFocusChange(View v, boolean hasFocus);
	}

	public interface OnCreateContextMenuListener {
		/**
		 * Called when the context menu for this view is being built. It is not
		 * safe to hold onto the menu after this method returns.
		 *
		 * @param menu The context menu that is being built
		 * @param v The view for which the context menu is being built
		 * @param menuInfo Extra information about the item for which the
		 *            context menu should be shown. This information will vary
		 *            depending on the class of v.
		 */
		//void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo);
	}

	public interface OnDragListener {}
	// --- end of interfaces

	// --- subclasses

	/**
	 * A MeasureSpec encapsulates the layout requirements passed from parent to child.
	 * Each MeasureSpec represents a requirement for either the width or the height.
	 * A MeasureSpec is comprised of a size and a mode. There are three possible
	 * modes:
	 * <dl>
	 * <dt>UNSPECIFIED</dt>
	 * <dd>
	 * The parent has not imposed any constraint on the child. It can be whatever size
	 * it wants.
	 * </dd>
	 *
	 * <dt>EXACTLY</dt>
	 * <dd>
	 * The parent has determined an exact size for the child. The child is going to be
	 * given those bounds regardless of how big it wants to be.
	 * </dd>
	 *
	 * <dt>AT_MOST</dt>
	 * <dd>
	 * The child can be as large as it wants up to the specified size.
	 * </dd>
	 * </dl>
	 *
	 * MeasureSpecs are implemented as ints to reduce object allocation. This class
	 * is provided to pack and unpack the &lt;size, mode&gt; tuple into the int.
	 */
	public static class MeasureSpec {
		private static final int MODE_SHIFT = 30;
		private static final int MODE_MASK = 0x3 << MODE_SHIFT;

		/**
		 * Measure specification mode: The parent has not imposed any constraint
		 * on the child. It can be whatever size it wants.
		 */
		public static final int UNSPECIFIED = 0 << MODE_SHIFT;

		/**
		 * Measure specification mode: The parent has determined an exact size
		 * for the child. The child is going to be given those bounds regardless
		 * of how big it wants to be.
		 */
		public static final int EXACTLY = 1 << MODE_SHIFT;

		/**
		 * Measure specification mode: The child can be as large as it wants up
		 * to the specified size.
		 */
		public static final int AT_MOST = 2 << MODE_SHIFT;

		/**
		 * Creates a measure specification based on the supplied size and mode.
		 *
		 * The mode must always be one of the following:
		 * <ul>
		 *  <li>{@link android.view.View.MeasureSpec#UNSPECIFIED}</li>
		 *  <li>{@link android.view.View.MeasureSpec#EXACTLY}</li>
		 *  <li>{@link android.view.View.MeasureSpec#AT_MOST}</li>
		 * </ul>
		 *
		 * <p><strong>Note:</strong> On API level 17 and lower, makeMeasureSpec's
		 * implementation was such that the order of arguments did not matter
		 * and overflow in either value could impact the resulting MeasureSpec.
		 * {@link android.widget.RelativeLayout} was affected by this bug.
		 * Apps targeting API levels greater than 17 will get the fixed, more strict
		 * behavior.</p>
		 *
		 * @param size the size of the measure specification
		 * @param mode the mode of the measure specification
		 * @return the measure specification based on size and mode
		 */
		public static int makeMeasureSpec(int size, int mode) {
			if (sUseBrokenMakeMeasureSpec) {
				return size + mode;
			} else {
				return (size & ~MODE_MASK) | (mode & MODE_MASK);
			}
		}

		/**
		 * Extracts the mode from the supplied measure specification.
		 *
		 * @param measureSpec the measure specification to extract the mode from
		 * @return {@link android.view.View.MeasureSpec#UNSPECIFIED},
		 *		 {@link android.view.View.MeasureSpec#AT_MOST} or
		 *		 {@link android.view.View.MeasureSpec#EXACTLY}
		 */
		public static int getMode(int measureSpec) {
			return (measureSpec & MODE_MASK);
		}

		/**
		 * Extracts the size from the supplied measure specification.
		 *
		 * @param measureSpec the measure specification to extract the size from
		 * @return the size in pixels defined in the supplied measure specification
		 */
		public static int getSize(int measureSpec) {
			return (measureSpec & ~MODE_MASK);
		}

		static int adjust(int measureSpec, int delta) {
			return makeMeasureSpec(getSize(measureSpec + delta), getMode(measureSpec));
		}

		/**
		 * Returns a String representation of the specified measure
		 * specification.
		 *
		 * @param measureSpec the measure specification to convert to a String
		 * @return a String with the following format: "MeasureSpec: MODE SIZE"
		 */
		public static String toString(int measureSpec) {
			int mode = getMode(measureSpec);
			int size = getSize(measureSpec);

			StringBuilder sb = new StringBuilder("MeasureSpec: ");

			if (mode == UNSPECIFIED)
				sb.append("UNSPECIFIED ");
			else if (mode == EXACTLY)
				sb.append("EXACTLY ");
			else if (mode == AT_MOST)
				sb.append("AT_MOST ");
			else
				sb.append(mode).append(" ");

			sb.append(size);
			return sb.toString();
		}
	}

	/* Copyright (C) 2006 The Android Open Source Project */
	public static class DragShadowBuilder {
		private final WeakReference<View> mView;

		public DragShadowBuilder(View view) {
			mView = new WeakReference<View>(view);
		}

		public DragShadowBuilder() {
			mView = new WeakReference<View>(null);
		}

		final public View getView() {
			return mView.get();
		}

		public void onProvideShadowMetrics(Point outShadowSize, Point outShadowTouchPoint) {
			final View view = mView.get();
			if (view != null) {
				outShadowSize.set(view.getWidth(), view.getHeight());
				outShadowTouchPoint.set(outShadowSize.x / 2, outShadowSize.y / 2);
			} else {
				Slog.e("View", "Asked for drag thumb metrics but no view");
			}
		}

		public void onDrawShadow(Canvas canvas) {
			final View view = mView.get();
			if (view != null) {
				view.draw(canvas);
			} else {
				Slog.e("View", "Asked to draw drag shadow but no view");
			}
		}
	}

	// --- end of subclasses

	public int id = NO_ID;
	private int system_ui_visibility = 0;
	public ViewParent parent;
	public AttributeSet attrs;
	protected ViewGroup.LayoutParams layout_params;
	private Context context;
	private Map<Integer, Object> tags = new HashMap<>();
	private Object tag;
	int gravity = -1; // fallback gravity for layout children

	int measuredWidth = 0;
	int measuredHeight = 0;

	private int left = 0;
	private int top = 0;
	private int right = 0;
	private int bottom = 0;

	private float translationX = 0;
	private float translationY = 0;
	private float scaleX = 1;
	private float scaleY = 1;
	private float rotation = 0;
	private float pivotX = 0;
	private float pivotY = 0;
	private boolean pivotExplicitlySet = false;
	private float transitionAlpha = 1.0f;
	private Matrix matrix; // lazily built by getMatrix()
	private Matrix inverseMatrix;

	private int scrollX = 0;
	private int scrollY = 0;

	ViewTreeObserver floating_observer = null;

	public long widget; // pointer

	private int oldWidthMeasureSpec = -1;
	private int oldHeightMeasureSpec = -1;
	private boolean layoutRequested = true;
	private int oldWidth;
	private int oldHeight;
	protected boolean haveCustomMeasure = true;
	public boolean disallowIntercept = false;

	private int visibility = View.VISIBLE;
	private float alpha = 1.0f;
	private boolean pressed = false;
	private Drawable background;
	private int backgroundTint = 0;
	private boolean clipToOutline = false;
	private ViewOutlineProvider outlineProvider = ViewOutlineProvider.BACKGROUND;

	private int minWidth = 0;
	private int minHeight = 0;

	protected int paddingLeft = 0;
	protected int paddingTop = 0;
	protected int paddingRight = 0;
	protected int paddingBottom = 0;
	/** true once padding has been set explicitly (by the app or an XML padding attr),
	 *  so a background drawable's own padding must no longer override it. */
	private boolean userPaddingSet = false;

	public static final Property<View, Float> TRANSLATION_X = new Property<View, Float>(Float.class, "translationX") {
		@Override
		public Float get(View view) {
			return view.getTranslationX();
		}
		@Override
		public void set(View view, Float value) {
			view.setTranslationX(value);
		}
	};

	public static final Property<View, Float> TRANSLATION_Y = new Property<View, Float>(Float.class, "translationY") {
		@Override
		public Float get(View view) {
			return view.getTranslationY();
		}
		@Override
		public void set(View view, Float value) {
			view.setTranslationY(value);
		}
	};

	public static final Property<View, Float> TRANSLATION_Z = new Property<View, Float>(Float.class, "translationZ") {
		@Override
		public Float get(View view) {
			return view.getTranslationZ();
		}
		@Override
		public void set(View view, Float value) {
			view.setTranslationZ(value);
		}
	};

	public static final Property<View, Float> ALPHA = new Property<View, Float>(Float.class, "alpha") {
		@Override
		public Float get(View view) {
			return view.getAlpha();
		}
		@Override
		public void set(View view, Float value) {
			view.setAlpha(value);
		}
	};

	public static final Property<View, Float> ROTATION = new Property<View, Float>(Float.class, "rotation") {
		@Override
		public Float get(View view) {
			return view.getRotation();
		}
		@Override
		public void set(View view, Float value) {
			view.setRotation(value);
		}
	};

	public static final Property<View, Float> ROTATION_X = new Property<View, Float>(Float.class, "rotationX") {
		@Override
		public Float get(View view) {
			return view.getRotationX();
		}
		@Override
		public void set(View view, Float value) {
			view.setRotationX(value);
		}
	};

	public static final Property<View, Float> ROTATION_Y = new Property<View, Float>(Float.class, "rotationY") {
		@Override
		public Float get(View view) {
			return view.getRotationY();
		}
		@Override
		public void set(View view, Float value) {
			view.setRotationY(value);
		}
	};

	public static final Property<View, Float> X = new Property<View, Float>(Float.class, "x") {
		@Override
		public Float get(View view) {
			return view.getX();
		}
		@Override
		public void set(View view, Float value) {
			view.setX(value);
		}
	};

	public static final Property<View, Float> Y = new Property<View, Float>(Float.class, "y") {
		@Override
		public Float get(View view) {
			return view.getY();
		}
		@Override
		public void set(View view, Float value) {
			view.setY(value);
		}
	};

	public static final Property<View, Float> Z = new Property<View, Float>(Float.class, "z") {
		@Override
		public Float get(View view) {
			return view.getZ();
		}
		@Override
		public void set(View view, Float value) {
			view.setZ(value);
		}
	};

	public View(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	public View(Context context, AttributeSet attrs, int defStyle) {
		this(context, attrs, defStyle, 0);
	}

	public View(Context context, AttributeSet attrs, int defStyle, int defStyleRes) {
		this.context = context;

		TypedArray a = context.obtainStyledAttributes(attrs, com.android.internal.R.styleable.View, defStyle, defStyleRes);
		this.id = a.getResourceId(com.android.internal.R.styleable.View_id, View.NO_ID);
		if (a.hasValue(com.android.internal.R.styleable.View_backgroundTint))
			backgroundTint = a.getColor(com.android.internal.R.styleable.View_backgroundTint, 0);
		clipToOutline = a.getBoolean(com.android.internal.R.styleable.View_clipToOutline, false);
		if (a.hasValue(com.android.internal.R.styleable.View_background)) {
			try {
				Drawable background = a.getDrawable(com.android.internal.R.styleable.View_background);

				if (background != null) {
					if (background instanceof ColorDrawable) {
						setBackgroundColor(((ColorDrawable)background).getColor());
					} else {
						setBackgroundDrawable(background);
					}
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
		if (a.hasValue(com.android.internal.R.styleable.View_visibility)) {
			visibility = VISIBILITY_FLAGS[a.getInt(com.android.internal.R.styleable.View_visibility, 0)];
		}

		if (a.hasValue(com.android.internal.R.styleable.View_minWidth))
			minWidth = a.getDimensionPixelSize(com.android.internal.R.styleable.View_minWidth, 0);

		if (a.hasValue(com.android.internal.R.styleable.View_minHeight))
			minHeight = a.getDimensionPixelSize(com.android.internal.R.styleable.View_minHeight, 0);

		int padding = a.getDimensionPixelSize(com.android.internal.R.styleable.View_padding, -1);

		int paddingVertical = a.getDimensionPixelSize(com.android.internal.R.styleable.View_paddingVertical, -1);
		int paddingHorizontal = a.getDimensionPixelSize(com.android.internal.R.styleable.View_paddingHorizontal, -1);

		int paddingStart = a.getDimensionPixelSize(com.android.internal.R.styleable.View_paddingStart, -1);
		int paddingEnd = a.getDimensionPixelSize(com.android.internal.R.styleable.View_paddingEnd, -1);

		// Only override padding (possibly already derived from the background drawable
		// above) when an explicit padding attribute is actually present.
		if (padding >= 0) {
			paddingLeft = padding;
			paddingTop = padding;
			paddingRight = padding;
			paddingBottom = padding;
			userPaddingSet = true;
		} else {
			if (paddingVertical >= 0) {
				paddingTop = paddingVertical;
				paddingBottom = paddingVertical;
				userPaddingSet = true;
			} else {
				if (a.hasValue(com.android.internal.R.styleable.View_paddingTop)) {
					paddingTop = a.getDimensionPixelSize(com.android.internal.R.styleable.View_paddingTop, 0);
					userPaddingSet = true;
				}
				if (a.hasValue(com.android.internal.R.styleable.View_paddingBottom)) {
					paddingBottom = a.getDimensionPixelSize(com.android.internal.R.styleable.View_paddingBottom, 0);
					userPaddingSet = true;
				}
			}

			if (paddingHorizontal >= 0) {
				paddingLeft = paddingHorizontal;
				paddingRight = paddingHorizontal;
				userPaddingSet = true;
			} else {
				if (a.hasValue(com.android.internal.R.styleable.View_paddingLeft)) {
					paddingLeft = a.getDimensionPixelSize(com.android.internal.R.styleable.View_paddingLeft, 0);
					userPaddingSet = true;
				}
				if (a.hasValue(com.android.internal.R.styleable.View_paddingRight)) {
					paddingRight = a.getDimensionPixelSize(com.android.internal.R.styleable.View_paddingRight, 0);
					userPaddingSet = true;
				}

				if (paddingStart >= 0) {
					paddingLeft = paddingStart;
					userPaddingSet = true;
				}

				if (paddingEnd >= 0) {
					paddingRight = paddingEnd;
					userPaddingSet = true;
				}
			}
		}

		if (a.hasValue(com.android.internal.R.styleable.View_tag))
			tag = a.getText(com.android.internal.R.styleable.View_tag);

		if (a.hasValue(com.android.internal.R.styleable.View_textAlignment)) {
			int textAlignment = a.getInt(com.android.internal.R.styleable.View_textAlignment, 0);
			setTextAlignment(textAlignment);
		}

		String handlerName = a.getString(com.android.internal.R.styleable.View_onClick);
		if (handlerName != null)
			setOnClickListener(new DeclaredOnClickListener(this, handlerName));

		if (a.hasValue(com.android.internal.R.styleable.View_stateListAnimator)) {
			int resId = a.getResourceId(com.android.internal.R.styleable.View_stateListAnimator, 0);
			setStateListAnimator(AnimatorInflater.loadStateListAnimator(context, resId));
		}
		a.recycle();
		onCreateDrawableState(0);
	}

	/* AOSP contract: the returned array has extraSpace zeroed slots at the end
	 * which subclasses fill in (directly or via mergeDrawableStates). Apps rely
	 * on this, e.g. state[state.length - 1] = state_pressed. */
	protected int[] onCreateDrawableState(int extraSpace) {
		int[] state = new int[2 + extraSpace];
		state[0] = R.attr.state_enabled;
		if (pressed) {
			state[1] = R.attr.state_pressed;
		}
		return state;
	}

	protected static int[] mergeDrawableStates(int[] curState, int[] newState) {
		int i = curState.length - 1;
		while (i >= 0 && curState[i] == 0)
			i--;
		System.arraycopy(newState, 0, curState, i + 1, newState.length);
		return curState;
	}

	public View findViewById(int id) {
		if (this.id == id)
			return this;
		else
			return null;
	}

	public void onDraw(Canvas canvas) {}

	protected void dispatchDraw(Canvas canvas) {}

	void drawBackground(Canvas canvas) {
		if (background != null) {
			background.setBounds(0, 0, getWidth(), getHeight());
			// the parent draws us translated by our scroll; undo it for the background
			if ((scrollX | scrollY) == 0) {
				background.draw(canvas);
			} else {
				canvas.translate(scrollX, scrollY);
				background.draw(canvas);
				canvas.translate(-scrollX, -scrollY);
			}
		}
	}

	public void draw(Canvas canvas) {
		int outlineSaveCount = -1;
		if (clipToOutline && outlineProvider != null) {
			if (background != null)  // BACKGROUND provider queries the drawable's bounds
				background.setBounds(0, 0, getWidth(), getHeight());
			Outline outline = new Outline();
			outlineProvider.getOutline(this, outline);
			if (outline.mMode == Outline.MODE_ROUND_RECT && !outline.mRect.isEmpty()) {
				outlineSaveCount = canvas.save();
				if (outline.mRadius > 0.0f) {
					Path path = new Path();
					path.addRoundRect(new RectF(outline.mRect), outline.mRadius, outline.mRadius, Path.Direction.CW);
					canvas.clipPath(path);
				} else {
					canvas.clipRect(outline.mRect);
				}
			} else if (outline.mMode == Outline.MODE_PATH && outline.mPath != null) {
				outlineSaveCount = canvas.save();
				canvas.clipPath(outline.mPath);
			}
		}
		drawBackground(canvas);
		onDraw(canvas);
		// HACK: catch non critical exceptions happening in some composeUI apps
		try {
			dispatchDraw(canvas);
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		}
		if (outlineSaveCount >= 0)
			canvas.restoreToCount(outlineSaveCount);
	}

	public View(Context context) {
		this(context, null);
	}

	public final ViewParent getParent() {
		return parent;
	}

	public void setLayoutParams(ViewGroup.LayoutParams params) {
		if (params == null) {
			throw new NullPointerException("Layout parameters cannot be null");
		}
		params.resolveLayoutDirection(getLayoutDirection());

		int gravity = params.gravity;
		if (gravity == -1 && parent instanceof View)
			gravity = ((View)parent).gravity;

		int leftMargin = 0;
		int topMargin = 0;
		int rightMargin = 0;
		int bottomMargin = 0;
		if (params instanceof ViewGroup.MarginLayoutParams) {
			leftMargin = ((ViewGroup.MarginLayoutParams)params).leftMargin;
			topMargin = ((ViewGroup.MarginLayoutParams)params).topMargin;
			rightMargin = ((ViewGroup.MarginLayoutParams)params).rightMargin;
			bottomMargin = ((ViewGroup.MarginLayoutParams)params).bottomMargin;
		}

		layout_params = params;
		requestLayout();
	}

	public ViewGroup.LayoutParams getLayoutParams() {
		return layout_params;
	}

	protected final void setMeasuredDimension(int measuredWidth, int measuredHeight) {
		this.measuredWidth = measuredWidth;
		this.measuredHeight = measuredHeight;
	}

	public Resources getResources() {
		return Context.this_application.getResources();
	}

	public void setGravity(int gravity) {
		this.gravity = gravity;
	}

	private OnTouchListener on_touch_listener = null;

	public boolean onTouchEventInternal(MotionEvent event, boolean handle_gestures) {
		boolean handled = false;
		if (on_touch_listener != null)
			handled = on_touch_listener.onTouch(this, event);

		if (!handled)
			handled = onTouchEvent(event);

		if (handle_gestures && !handled && event.getAction() == MotionEvent.ACTION_UP)
			handled = performClick();

		return handled;
	}

	private boolean clickable = false;
	private boolean longClickable = false;

	private boolean isClickableForTouch() {
		return clickable || longClickable || on_click_listener != null || on_long_click_listener != null;
	}

	public boolean onTouchEvent(MotionEvent event) {
		/* A clickable view consumes the whole gesture (so the parent keeps
		 * routing events to it through to ACTION_UP) and fires performClick on
		 * release. Without this a clickable child never claims ACTION_DOWN, so
		 * ViewGroup never makes it the touch target and the click is lost. */
		if (isClickableForTouch()) {
			switch (event.getActionMasked()) {
				case MotionEvent.ACTION_DOWN:
					setPressed(true);
					break;
				case MotionEvent.ACTION_UP:
					if (isPressed()) {
						performClick();
						setPressed(false);
					}
					break;
				case MotionEvent.ACTION_CANCEL:
					setPressed(false);
					break;
			}
			return true;
		}
		return false;
	}

	public void setOnTouchListener(OnTouchListener l) {
		on_touch_listener = l;
	}
	private OnClickListener on_click_listener = null;
	public void setOnClickListener(OnClickListener l) {
		on_click_listener = l;
		clickable = true;
	}

	private OnScrollChangeListener on_scroll_change_listener = null;
	public void setOnScrollChangeListener(OnScrollChangeListener l) {}

	public /*native*/ void setOnSystemUiVisibilityChangeListener(OnSystemUiVisibilityChangeListener l) {}
	public final int getWidth() {
		return right - left;
	}
	public final int getHeight() {
		return bottom - top;
	}

	/* the GTK widget hierarchy is gone: views are pure java objects rendered
	 * through Skia by the window's ViewRootImpl; these remain as no-ops until
	 * the remaining callers are cleaned up */
	protected void native_addClass(long widget, String className) {}
	protected void native_removeClass(long widget, String className) {}

	protected void native_addClasses(long widget, String[] classNames) {}
	protected void native_removeClasses(long widget, String[] classNames) {}

	ViewRootImpl viewRootImpl; // set on the root view by ViewRootImpl.setView
	private boolean focused = false;
	private OnFocusChangeListener onFocusChangeListener;

	public ViewRootImpl getViewRootImpl() {
		View view = this;
		while (view.parent instanceof View)
			view = (View)view.parent;
		return view.viewRootImpl;
	}

	/* API 33 predictive back: hand out a no-op dispatcher (callbacks never invoked;
	 * back still routes through the legacy onBackPressed path). */
	private static final android.window.OnBackInvokedDispatcher NOOP_BACK_DISPATCHER = new android.window.OnBackInvokedDispatcher() {
		public void registerOnBackInvokedCallback(int priority, android.window.OnBackInvokedCallback callback) {}
		public void unregisterOnBackInvokedCallback(android.window.OnBackInvokedCallback callback) {}
	};

	public android.window.OnBackInvokedDispatcher findOnBackInvokedDispatcher() {
		return NOOP_BACK_DISPATCHER;
	}

	/* Rich-content receipt is not wired up; record the listener but never invoke it. */
	private OnReceiveContentListener onReceiveContentListener;

	public void setOnReceiveContentListener(String[] mimeTypes, OnReceiveContentListener listener) {
		onReceiveContentListener = listener;
	}

	public ContentInfo onReceiveContent(ContentInfo payload) {
		return payload;
	}

	void dispatchAttachedToWindow() {
		onAttachedToWindow();
	}

	void dispatchDetachedFromWindow() {
		onDetachedFromWindow();
	}

	// --- stubs

	public void setContentDescription(CharSequence contentDescription) {}

	public void setId(int id) {
		this.id = id;
	}

	private OnKeyListener onKeyListener;
	public void setOnKeyListener(OnKeyListener l) {
		onKeyListener = l;
	}

	private int focusableAttr = FOCUSABLE;
	private boolean focusableInTouchMode = false;
	public void setFocusable(boolean focusable) {
		focusableAttr = focusable ? FOCUSABLE : NOT_FOCUSABLE;
	}
	public void setFocusable(int focusable) {
		focusableAttr = focusable;
	}
	public void setFocusableInTouchMode(boolean focusableInTouchMode) {
		this.focusableInTouchMode = focusableInTouchMode;
		if (focusableInTouchMode)
			focusableAttr = FOCUSABLE;
	}
	public int getFocusable() {
		return focusableAttr;
	}
	public final boolean isFocusableInTouchMode() {
		return focusableInTouchMode;
	}
	public final boolean requestFocus() {
		return requestFocus(View.FOCUS_DOWN);
	}
	public final boolean requestFocus(int direction) {
		return requestFocus(direction, null);
	}
	public boolean requestFocus(int direction, Rect previouslyFocusedRect) {
		ViewRootImpl root = getViewRootImpl();
		if (root != null)
			root.setFocusedView(this);
		return true;
	}

	/** Called by ViewRootImpl when this view gains or loses input focus. */
	void dispatchFocusChanged(boolean gainFocus) {
		if (focused == gainFocus)
			return;
		focused = gainFocus;
		if (!gainFocus)
			onFocusLost();
		onFocusChanged(gainFocus, FOCUS_DOWN, null);
		if (onFocusChangeListener != null)
			onFocusChangeListener.onFocusChange(this, gainFocus);
		invalidate();
	}

	protected void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {}
	public final boolean requestFocusFromTouch() {
		// TODO: if (isInTouchMode()) leave touch mode
		return requestFocus(View.FOCUS_DOWN);
	}

	public void setSystemUiVisibility(int visibility) {
		// TODO: route fullscreen request to the window
		system_ui_visibility = visibility;
	}
	public int getSystemUiVisibility() {
		return system_ui_visibility;
	};

	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		setMeasuredDimension(getDefaultSize(getSuggestedMinimumWidth(), widthMeasureSpec), getDefaultSize(getSuggestedMinimumHeight(), heightMeasureSpec));
	}

	public void setPressed(boolean pressed) {
		if (this.pressed != pressed) {
			this.pressed = pressed;
			dispatchSetPressed(pressed);
			drawableStateChanged();
		}
	}

	/* minimal version of AOSP's drawable state plumbing: push the current
	 * state to the stateful drawables this view owns; subclasses with more
	 * drawables (e.g. TextView's compound drawables) override this */
	protected void drawableStateChanged() {
		final int[] state = getDrawableState();
		if (background != null && background.isStateful() && background.setState(state))
			invalidate();
	}

	private boolean selected = false;
	public void setSelected(boolean selected) {
		if (this.selected != selected) {
			this.selected = selected;
			dispatchSetSelected(selected);
			drawableStateChanged();
			invalidate();
		}
	}

	public Window native_get_window(long widget) {
		ViewRootImpl root = getViewRootImpl();
		return root != null ? root.window : null;
	}
	public ViewTreeObserver getViewTreeObserver() {
		Window window = native_get_window(widget);
		if (window != null) {
			if (window.view_tree_observer == null)
				window.view_tree_observer = new ViewTreeObserver(window);
			return window.view_tree_observer;
		} else {
			floating_observer = new ViewTreeObserver(null);
			return floating_observer;
		}
	}

	protected void onFinishInflate() {}

	public void invalidateDrawable(Drawable drawable) {
		invalidate();
	}

	public void scheduleDrawable(Drawable drawable, Runnable runnable, long time) {
		postDelayed(runnable, time - SystemClock.uptimeMillis());
	}

	public void unscheduleDrawable(Drawable drawable, Runnable runnable) {
		/* TODO */
	}

	public void unscheduleDrawable(Drawable drawable) {}

	public void invalidate(Rect dirty) {
		invalidateInternal(dirty.left - scrollX, dirty.top - scrollY,
		                   dirty.right - scrollX, dirty.bottom - scrollY);
	}
	public void invalidate(int l, int t, int r, int b) {
		invalidateInternal(l - scrollX, t - scrollY, r - scrollX, b - scrollY);
	}
	public void invalidate() {
		invalidateInternal(0, 0, right - left, bottom - top);
	}

	/* AOSP software invalidation: hand the damage rect (view-local coordinates)
	 * to the parent, which walks it up to the ViewRootImpl (invalidateInternal ->
	 * ViewGroup.invalidateChild -> ViewRootImpl.invalidateChildInParent). */
	void invalidateInternal(int l, int t, int r, int b) {
		final ViewParent p = parent;
		if (p != null && l < r && t < b)
			p.invalidateChild(this, new Rect(l, t, r, b));
	}

	public void setBackgroundColor(int color) {
		background = new ColorDrawable(color);
		invalidate();
	}

	public void native_setVisibility(long widget, int visibility, float alpha) {}

	protected void onVisibilityChanged(View changedView, int visibility) {
	}

	protected void dispatchVisibilityChanged(View changedView, int visibility) {
		onVisibilityChanged(changedView, visibility);
	}

	public void setVisibility(int visibility) {
		if (visibility == this.visibility)
			return;
		if ((visibility == View.GONE) != (this.visibility == View.GONE) && parent instanceof ViewGroup) {
			((ViewGroup)parent).requestLayout();
		}
		this.visibility = visibility;
		invalidate();
		dispatchVisibilityChanged(this, visibility);
	}

	public void setPadding(int left, int top, int right, int bottom) {
		userPaddingSet = true;
		paddingLeft = left;
		paddingTop = top;
		paddingRight = right;
		paddingBottom = bottom;
		requestLayout();
	}

	public void setBackgroundResource(int resid) {
		// Load through the context theme so theme attribute references
		// (?attr/...) inside the drawable XML are resolved against the
		// active theme, matching AOSP's mContext.getDrawable(resid).
		setBackgroundDrawable(resid == 0 ? null : getContext().getDrawable(resid));
	}

	public void getHitRect(Rect outRect) {
		outRect.set(left, top, right, bottom);
	}
	public final boolean getLocalVisibleRect(Rect r) { return false; }

	public final int getScrollX() {
		return scrollX;
	}
	public final int getScrollY() {
		return scrollY;
	}

	public void scrollTo(int x, int y) {
		scrollX = x;
		scrollY = y;
		invalidate();
	}

	protected int computeVerticalScrollOffset() {
		return -1;
	}

	protected void onScrollChanged(int l, int t, int oldl, int oldt) {}

	public void getLocationOnScreen(int[] location) {
		Rect rect = new Rect();
		getGlobalVisibleRect(rect);
		location[0] = rect.left;
		location[1] = rect.top;
	}

	public boolean performHapticFeedback(int feedbackConstant) {
		return performHapticFeedback(feedbackConstant, 0);
	}

	public boolean performHapticFeedback(int feedbackConstant, int flags) {
		// arbitrary; TODO: better mimic what AOSP does
		Vibrator vibrator = (Vibrator)getContext().getSystemService("vibrator");
		vibrator.vibrate(10);
		return true; // FIXME why is it not void
	}

	public int getPaddingLeft() {
		return paddingLeft;
	}

	public int getPaddingRight() {
		return paddingRight;
	}

	public int getPaddingTop() {
		return paddingTop;
	}

	public int getPaddingBottom() {
		return paddingBottom;
	}

	public int getPaddingStart() {
		return paddingLeft;
	}

	public int getPaddingEnd() {
		return paddingRight;
	}

	public void postInvalidate() {
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				invalidate();
			}
		});
	}

	public void postInvalidate(final int left, final int top, final int right, final int bottom) {
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				invalidate(left, top, right, bottom);
			}
		});
	}

	public void setOnGenericMotionListener(View.OnGenericMotionListener l) {}

	public IBinder getWindowToken() {
		return new Binder();
	}

	public void getLocationInWindow(int[] location) {
		getLocationOnScreen(location);
	}

	public void addOnAttachStateChangeListener(OnAttachStateChangeListener l) {
		onAttachStateChangeListener = l;
	}

	public Context getContext() {
		return this.context;
	}

	public void refreshDrawableState() {
		drawableStateChanged();
	}

	public void setDescendantFocusability(int value) {}

	public void setAccessibilityDelegate(AccessibilityDelegate delegate) {}

	public static class AccessibilityDelegate {
		public void sendAccessibilityEvent(View view, int eventType) {}
	}

	public Drawable getBackground() {
		return background;
	}

	public void setClickable(boolean clickable) {
		this.clickable = clickable;
	}

	public void setEnabled(boolean enabled) {
		if (this.enabled != enabled) {
			this.enabled = enabled;
			drawableStateChanged();
			invalidate();
		}
	}

	public CharSequence getContentDescription() {
		return null;
	}

	private OnLongClickListener on_long_click_listener = null;
	public boolean performLongClick(float x, float y) {
		if (on_long_click_listener != null) {
			return on_long_click_listener.onLongClick(this);
		}
		return false;
	}
	public void setOnLongClickListener(OnLongClickListener listener) {
		on_long_click_listener = listener;
		longClickable = true;
	}

	public void setLongClickable(boolean longClickable) {
		this.longClickable = longClickable;
	}

	private OnContextClickListener on_context_click_listener = null;
	public void setOnContextClickListener(OnContextClickListener listener) {
		on_context_click_listener = listener;
	}

	private OnHoverListener onHoverListener;
	public void setOnHoverListener(OnHoverListener listener) {
		onHoverListener = listener;
	}

	public final void measure(int widthMeasureSpec, int heightMeasureSpec) {
		if (layoutRequested || widthMeasureSpec != oldWidthMeasureSpec || heightMeasureSpec != oldHeightMeasureSpec) {
			layoutRequested = false;
			oldWidthMeasureSpec = widthMeasureSpec;
			oldHeightMeasureSpec = heightMeasureSpec;
			onMeasure(widthMeasureSpec, heightMeasureSpec);
		}
	}

	public final int getMeasuredState() {
		return (measuredWidth & MEASURED_STATE_MASK)
		    | ((measuredHeight >> MEASURED_HEIGHT_STATE_SHIFT)
		       & (MEASURED_STATE_MASK >> MEASURED_HEIGHT_STATE_SHIFT));
	}

	public static int combineMeasuredStates(int curState, int newState) {
		return curState | newState;
	}

	protected int getSuggestedMinimumHeight() {
		return getMinimumHeight();
	}
	protected int getSuggestedMinimumWidth() {
		return getMinimumWidth();
	}

	/**
	 * Utility to reconcile a desired size and state, with constraints imposed
	 * by a MeasureSpec.  Will take the desired size, unless a different size
	 * is imposed by the constraints.  The returned value is a compound integer,
	 * with the resolved size in the {@link #MEASURED_SIZE_MASK} bits and
	 * optionally the bit {@link #MEASURED_STATE_TOO_SMALL} set if the resulting
	 * size is smaller than the size the view wants to be.
	 *
	 * @param size How big the view wants to be
	 * @param measureSpec Constraints imposed by the parent
	 * @return Size information bit mask as defined by
	 * {@link #MEASURED_SIZE_MASK} and {@link #MEASURED_STATE_TOO_SMALL}.
	 */
	public static int resolveSizeAndState(int size, int measureSpec, int childMeasuredState) {
		int result = size;
		int specMode = MeasureSpec.getMode(measureSpec);
		int specSize = MeasureSpec.getSize(measureSpec);
		switch (specMode) {
			case MeasureSpec.UNSPECIFIED:
				result = size;
				break;
			case MeasureSpec.AT_MOST:
				if (specSize < size) {
					result = specSize | MEASURED_STATE_TOO_SMALL;
				} else {
					result = size;
				}
				break;
			case MeasureSpec.EXACTLY:
				result = specSize;
				break;
		}
		return result | (childMeasuredState & MEASURED_STATE_MASK);
	}

	public static int resolveSize(int size, int measureSpec) {
		return resolveSizeAndState(size, measureSpec, 0) & MEASURED_SIZE_MASK;
	}

	public final int getMeasuredWidth() {
		return this.measuredWidth & MEASURED_SIZE_MASK;
	}

	public final int getMeasuredHeight() {
		return this.measuredHeight & MEASURED_SIZE_MASK;
	}

	protected void onLayout(boolean changed, int l, int t, int r, int b) {}

	public void layout(int l, int t, int r, int b) {
		int oldL = this.left;
		int oldT = this.top;
		int oldR = this.right;
		int oldB = this.bottom;
		boolean moved = oldL != l || oldT != t || oldR != r || oldB != b;
		if (moved)
			invalidate(); // damage the old position (AOSP setFrame)
		this.left = l;
		this.top = t;
		this.right = r;
		this.bottom = b;
		int width = r - l;
		int height = b - t;
		boolean changed = oldWidth != width || oldHeight != height;
		if (changed)
			onSizeChanged(width, height, oldWidth, oldHeight);
		oldWidth = width;
		oldHeight = height;
		onLayout(changed, l, t, r, b);
		if (moved)
			invalidate(); // damage the new position
		layoutRequested = false;
		if (layout_change_listeners != null && !layout_change_listeners.isEmpty()) {
			java.util.ArrayList<OnLayoutChangeListener> listeners = new java.util.ArrayList<>(layout_change_listeners);
			for (OnLayoutChangeListener listener : listeners)
				listener.onLayoutChange(this, l, t, r, b, oldL, oldT, oldR, oldB);
		}
	}

	public int getLeft() {
		return left;
	}
	public int getTop() {
		return top;
	}
	public int getRight() {
		return right;
	}
	public int getBottom() {
		return bottom;
	}

	public void offsetTopAndBottom(int offset) {
		layout(left, top + offset, right, bottom + offset);
	}

	public void offsetLeftAndRight(int offset) {
		layout(left + offset, top, right + offset, bottom);
	}

	public void setBackgroundDrawable(Drawable backgroundDrawable) {
		this.background = backgroundDrawable;
		if (backgroundDrawable != null) {
			backgroundDrawable.setCallback(this);
			if (backgroundTint != 0)
				backgroundDrawable.setTint(backgroundTint);
			// Adopt the drawable's padding (e.g. an EditText's editbox nine-patch),
			// unless the view has explicitly-set padding. Matches AOSP and is what
			// gives backgrounded widgets their intrinsic content height.
			if (!userPaddingSet) {
				Rect pad = new Rect();
				if (backgroundDrawable.getPadding(pad)) {
					paddingLeft = pad.left;
					paddingTop = pad.top;
					paddingRight = pad.right;
					paddingBottom = pad.bottom;
					requestLayout();
				}
			}
		}
		invalidate();
	}

	private int overScrollMode = OVER_SCROLL_IF_CONTENT_SCROLLS;
	public int getOverScrollMode() { return overScrollMode; }

	private boolean fitsSystemWindows = false;
	public void setFitsSystemWindows(boolean fitsSystemWindows) {
		this.fitsSystemWindows = fitsSystemWindows;
	}
	public boolean getFitsSystemWindows() {
		return fitsSystemWindows;
	}
	public boolean fitsSystemWindows() {
		return fitsSystemWindows;
	}

	public void setWillNotDraw(boolean value) {}

	private boolean scrollContainer = false;
	public void setScrollContainer(boolean isScrollContainer) {
		scrollContainer = isScrollContainer;
	}
	public boolean isScrollContainer() {
		return scrollContainer;
	}

	public boolean removeCallbacks(Runnable action) {
		getHandler().removeCallbacks(action);
		return true;
	}

	/** legacy shim (used by Dialog/WindowManagerImpl sizing) */
	public void internalSetDefaultMeasureSpec(int widthMeasureSpec, int heightMeasureSpec) {
		requestLayout();
	}

	public void requestLayout() {
		layoutRequested = true;
		/* propagate through the parent's requestLayout() (not just up to the
		 * ViewRootImpl) so container overrides run: e.g. RecyclerView eats
		 * requestLayout from item views while it is laying them out — going
		 * straight to the ViewRootImpl instead turns every RecyclerView
		 * layout pass into another scheduled layout pass, forever. */
		if (parent != null && !parent.isLayoutRequested())
			parent.requestLayout();
	};

	public void setOverScrollMode(int mode) {
		overScrollMode = mode;
	}

	public int getId() { return id; }
	public String getIdName() {
		if (this.id == View.NO_ID) {
			return "NO_ID";
		}

		try {
			return getResources().getResourceName(this.id);
		} catch (Resources.NotFoundException e) {
			return "NOT_FOUND";
		}
	}
	public String getAllSuperClasses() {
		StringBuilder sb = new StringBuilder();
		Class<?> currentClass = getClass();

		sb.append(currentClass.getCanonicalName());
		currentClass = currentClass.getSuperclass();
		while (currentClass != null) {
			sb.append(" << ").append(currentClass.getCanonicalName());
			currentClass = currentClass.getSuperclass();
		}
		return sb.toString();
	}

	public boolean postDelayed(Runnable action, long delayMillis) {
		getHandler().postDelayed(action, delayMillis);
		return true;
	}

	public boolean post(Runnable action) {
		getHandler().post(action);
		return true;
	}

	public void setSaveFromParentEnabled(boolean enabled) {}

	public void setTag(int key, Object tag) {
		tags.put(key, tag);
	}
	public Object getTag(int key) {
		return tags.get(key);
	}

	public void setTag(Object tag) {
		this.tag = tag;
	}
	public Object getTag() {
		return tag;
	}

	private java.util.ArrayList<OnLayoutChangeListener> layout_change_listeners = null;

	public void addOnLayoutChangeListener(OnLayoutChangeListener listener) {
		if (layout_change_listeners == null)
			layout_change_listeners = new java.util.ArrayList<>();
		if (!layout_change_listeners.contains(listener))
			layout_change_listeners.add(listener);
	}

	public void removeOnLayoutChangeListener(OnLayoutChangeListener listener) {
		if (layout_change_listeners != null)
			layout_change_listeners.remove(listener);
	}

	public boolean isSelected() { return selected; }

	public void sendAccessibilityEvent(int eventType) {}

	public void setMinimumHeight(int minHeight) {
		this.minHeight = minHeight;
	}
	public void setMinimumWidth(int minWidth) {
		this.minWidth = minWidth;
	}

	private boolean activated = false;
	public void setActivated(boolean activated) {
		if (this.activated != activated) {
			this.activated = activated;
			dispatchSetActivated(activated);
			drawableStateChanged();
			invalidate();
		}
	}

	public boolean isActivated() { return activated; }

	public int getVisibility() { return visibility; }

	public boolean isInEditMode() { return false; }


	public final int[] getDrawableState() {
		return onCreateDrawableState(0);
	}

	public float getRotation() { return rotation; }

	public void bringToFront() {}

	private boolean enabled = true;
	public boolean isEnabled() { return enabled; }
	public boolean hasFocus() { return focused; }
	public boolean isLayoutRequested() { return layoutRequested; }
	public int getBaseline() { return -1; }
	public boolean hasFocusable() { return false; }
	public boolean isFocused() {
		return focused;
	}

	public void clearAnimation() {}

	private ViewPropertyAnimator viewPropertyAnimator;
	public ViewPropertyAnimator animate() {
		if (viewPropertyAnimator == null)
			viewPropertyAnimator = new ViewPropertyAnimator(this);
		return viewPropertyAnimator;
	}

	public float getTranslationX() {
		return translationX;
	}

	public float getTranslationY() {
		return translationY;
	}

	public void setTranslationX(float translationX) {
		if (this.translationX == translationX)
			return;
		invalidate(); // damage the old position (maps through the old matrix)
		this.translationX = translationX;
		invalidate();
	}

	public void setTranslationY(float translationY) {
		if (this.translationY == translationY)
			return;
		invalidate();
		this.translationY = translationY;
		invalidate();
	}

	public void setX(float x) {
		setTranslationX(x - left);
	}

	public void setY(float y) {
		setTranslationY(y - top);
	}

	public void setAlpha(float alpha) {
		if (this.alpha != alpha) {
			this.alpha = alpha;
			invalidate();
		}
	}

	public boolean onGenericMotionEvent(MotionEvent event) { return false; }

	public boolean dispatchGenericMotionEvent(MotionEvent event) {
		return onGenericMotionEvent(event);
	}

	protected boolean awakenScrollBars() { return false; }

	protected static final int[] EMPTY_STATE_SET = new int[0];

	protected static final int[] ENABLED_STATE_SET = new int[] {R.attr.state_enabled};

	protected static final int[] PRESSED_ENABLED_STATE_SET = new int[] {R.attr.state_pressed, R.attr.state_enabled};

	protected static final int[] SELECTED_STATE_SET = new int[] {R.attr.state_selected};

	/**
	 * Utility to return a default size. Uses the supplied size if the
	 * MeasureSpec imposed no constraints. Will get larger if allowed
	 * by the MeasureSpec.
	 *
	 * @param size Default size for this view
	 * @param measureSpec Constraints imposed by the parent
	 * @return The size this view should be.
	 */
	public static int getDefaultSize(int size, int measureSpec) {
		int result = size;
		int specMode = MeasureSpec.getMode(measureSpec);
		int specSize = MeasureSpec.getSize(measureSpec);
		switch (specMode) {
			case MeasureSpec.UNSPECIFIED:
				result = size;
				break;
			case MeasureSpec.AT_MOST:
			case MeasureSpec.EXACTLY:
				result = specSize;
				break;
		}
		return result;
	}

	public static class BaseSavedState extends AbsSavedState {
	}

	public void clearFocus() {
		ViewRootImpl root = getViewRootImpl();
		if (root != null && root.getFocusedView() == this)
			root.setFocusedView(null);
	}

	public static View inflate(Context context, int resource, ViewGroup root) {
		LayoutInflater factory = LayoutInflater.from(context);
		return factory.inflate(resource, root);
	}

	public void saveHierarchyState(SparseArray<Parcelable> array) {}

	public void setDuplicateParentStateEnabled(boolean enabled) {}

	public boolean performClick() {
		return callOnClick();
	}

	public void playSoundEffect(int soundConstant) {}

	public void computeScroll() {}

	public void jumpDrawablesToCurrentState() {}

	public void setOnFocusChangeListener(View.OnFocusChangeListener l) { onFocusChangeListener = l; }
	public View.OnFocusChangeListener getOnFocusChangeListener() { return onFocusChangeListener; }

	public boolean hasWindowFocus() { return true; }

	public void setSaveEnabled(boolean enabled) {}

	public boolean willNotDraw() { return false; }

	public void setOnCreateContextMenuListener(View.OnCreateContextMenuListener l) {}

	protected void onAnimationStart() {}

	protected void onAnimationEnd() {}

	public void startAnimation(Animation animation) {
		if (animation == null)
			return;
		onAnimationStart();
		animation.start();
		onAnimationEnd();
	}

	public void getDrawingRect(Rect rect) {
		rect.left = getScrollX();
		rect.top = getScrollY();
		rect.right = getScrollX() + getWidth();
		rect.bottom = getScrollY() + getHeight();
	}

	public Display getDisplay() {
		return new Display();
	}

	private int importantForAccessibility;
	public void setImportantForAccessibility(int importantForAccessibility) {
		this.importantForAccessibility = importantForAccessibility;
	}
	public int getImportantForAccessibility() {
		return importantForAccessibility;
	}


	private OnApplyWindowInsetsListener onApplyWindowInsetsListener;
	public void setOnApplyWindowInsetsListener(View.OnApplyWindowInsetsListener l) {
		onApplyWindowInsetsListener = l;
	}
	public WindowInsets dispatchApplyWindowInsets(WindowInsets insets) {
		if (onApplyWindowInsetsListener != null)
			return onApplyWindowInsetsListener.onApplyWindowInsets(this, insets);
		return onApplyWindowInsets(insets);
	}
	public WindowInsets onApplyWindowInsets(WindowInsets insets) {
		return insets;
	}

	public final boolean isFocusable() { return true; }

	/* AOSP semantics: setOnClickListener() marks the view clickable. Apps use
	 * this to probe for views that handle clicks themselves (e.g. Telegram's
	 * RecyclerListView skips the item-click gesture when a child under the
	 * touch point reports isClickable()), so always returning true here broke
	 * list item selection. */
	public boolean isClickable() {
		return clickable || on_click_listener != null;
	}

	public boolean isLongClickable() {
		return longClickable || on_long_click_listener != null;
	}

	public int getLayoutDirection() { return LAYOUT_DIRECTION_LTR; }

	public void setBackground(Drawable background) {
		setBackgroundDrawable(background);
	}

	private float elevation;
	public void setElevation(float elevation) {
		this.elevation = elevation;
	}
	public float getElevation() {
		return elevation;
	}

	public boolean isLaidOut() { return true; }

	public void postOnAnimation(Runnable action) {
		postDelayed(action, 1000 / 60);
	}

	public void postOnAnimationDelayed(Runnable action, long delayMillis) {
		if (delayMillis < 1000 / 60)
			delayMillis = 1000 / 60;
		postDelayed(action, delayMillis);
	}

	/* scrollbars are not rendered, but the state must round-trip: apps and
	 * libraries save/restore these (e.g. across a programmatic style switch) */
	private boolean horizontalScrollBarEnabled = false;
	private boolean verticalScrollBarEnabled = false;
	private boolean scrollbarFadingEnabled = true;
	private int scrollBarStyle = SCROLLBARS_INSIDE_OVERLAY;

	public void setHorizontalScrollBarEnabled(boolean enabled) {
		horizontalScrollBarEnabled = enabled;
	}

	public boolean isHorizontalScrollBarEnabled() {
		return horizontalScrollBarEnabled;
	}

	public void setVerticalScrollBarEnabled(boolean enabled) {
		verticalScrollBarEnabled = enabled;
	}

	public boolean isVerticalScrollBarEnabled() {
		return verticalScrollBarEnabled;
	}

	public boolean isScrollbarFadingEnabled() {
		return scrollbarFadingEnabled;
	}

	public int getScrollBarStyle() {
		return scrollBarStyle;
	}

	public void postInvalidateOnAnimation() {
		postInvalidate();
	}

	public void setPaddingRelative(int start, int top, int end, int bottom) {
		// ATL is LTR-only: relative padding maps directly to absolute
		setPadding(start, top, end, bottom);
	}

	public boolean isAttachedToWindow() {
		return getViewRootImpl() != null;
	}

	public void requestApplyInsets() {}

	public View getRootView() {
		View view = this;
		while (view.parent instanceof View) {
			view = (View)view.parent;
		}
		return view;
	}

	public boolean isShown() { return true; }

	public int getWindowVisibility() { return VISIBLE; }

	public float getAlpha() { return alpha; }

	public View findFocus() { return this; }

	public int getMinimumHeight() { return minHeight; }
	public int getMinimumWidth() { return minWidth; }

	public boolean isNestedScrollingEnabled() { return false; }

	public void setClipToOutline(boolean clipToOutline) {
		this.clipToOutline = clipToOutline;
		invalidate();
	}

	public boolean getClipToOutline() { return clipToOutline; }

	public boolean hasTransientState() { return false; }

	public final void cancelPendingInputEvents() {}

	public ViewOutlineProvider getOutlineProvider() { return outlineProvider; }
	public void setOutlineProvider(ViewOutlineProvider provider) {
		outlineProvider = provider;
		invalidate();
	}

	public void setStateListAnimator(StateListAnimator stateListAnimator) {
		if (stateListAnimator != null && stateListAnimator.enabledAnimator != null) {
			stateListAnimator.enabledAnimator.setTarget(this);
			stateListAnimator.enabledAnimator.start();
		}
	}

	private static final AtomicInteger nextGeneratedId = new AtomicInteger(1);
	/**
     * Generate a value suitable for use in {@link #setId(int)}.
     * This value will not collide with ID values generated at build time by aapt for R.id.
     *
     * @return a generated ID value
     */
	public static int generateViewId() {
		for (;;) {
			final int result = nextGeneratedId.get();
			// aapt-generated IDs have the high byte nonzero; clamp to the range under that.
			int newValue = result + 1;
			if (newValue > 0x00FFFFFF)
				newValue = 1; // Roll over to 1, not 0.
			if (nextGeneratedId.compareAndSet(result, newValue)) {
				return result;
			}
		}
	}

	public boolean isLayoutDirectionResolved() { return true; }

	public boolean isPaddingRelative() { return false; }

	private Drawable foreground;
	public void setForeground(Drawable foreground) {
		this.foreground = foreground;
		invalidate();
	}

	public boolean canScrollVertically(int value) { return false; }

	public boolean isInTouchMode() { return false; }

	public void stopNestedScroll() {}

	public long getDrawingTime() {
		return System.currentTimeMillis();
	}

	private boolean keepScreenOn = false;
	public void setKeepScreenOn(boolean screenOn) {
		keepScreenOn = screenOn; // TODO: route to the window (gtk_application_inhibit)
	}

	protected void onAttachedToWindow() {
		if (onAttachStateChangeListener != null) {
			onAttachStateChangeListener.onViewAttachedToWindow(this);
		}
		if (floating_observer != null) {
			getViewTreeObserver().merge(floating_observer);
			floating_observer = null;
		}
	}
	protected void onDetachedFromWindow() {
		if (onAttachStateChangeListener != null) {
			onAttachStateChangeListener.onViewDetachedFromWindow(this);
		}
	}

	private int mLayerType = LAYER_TYPE_NONE;

	public void setLayerType(int layerType, Paint paint) {
		mLayerType = layerType;
	}

	public int getLayerType() {
		return mLayerType;
	}

	public float getZ() {
		return elevation;
	}

	protected void onSizeChanged(int w, int h, int oldw, int oldh) {}

	public void setBackgroundTintList(ColorStateList tint) {}

	protected int computeHorizontalScrollRange() {
		return getWidth();
	}

	protected int computeHorizontalScrollExtent() {
		return getWidth();
	}

	protected int computeVerticalScrollRange() {
		return getHeight();
	}

	protected int computeVerticalScrollExtent() {
		return getHeight();
	}

	public void setAccessibilityLiveRegion(int mode) {}

	public void invalidateOutline() {
		invalidate();
	}

	public int getMeasuredWidthAndState() {
		return measuredWidth;
	}

	public void forceLayout() {
		if (Looper.myLooper() == Looper.getMainLooper()) {
			requestLayout();
		} else {
			new Handler(Looper.getMainLooper()).post(new Runnable() {
				@Override
				public void run() {
					requestLayout();
				}
			});
		}
	}

	private OnAttachStateChangeListener onAttachStateChangeListener;
	public void removeOnAttachStateChangeListener(OnAttachStateChangeListener listener) {
		if (onAttachStateChangeListener == listener) {
			onAttachStateChangeListener = null;
		}
	}

	public boolean onInterceptTouchEvent(MotionEvent event) { return false; }

	public boolean dispatchTouchEvent(MotionEvent event) {
		return onTouchEventInternal(event, true);
	}

	public boolean canScrollHorizontally(int direction) { return false; }

	public boolean getGlobalVisibleRect(Rect visibleRect) {
		int x = 0;
		int y = 0;
		View view = this;
		while (true) {
			x += view.left + (int)view.translationX;
			y += view.top + (int)view.translationY;
			if (!(view.parent instanceof View))
				break;
			view = (View)view.parent;
			x -= view.scrollX;
			y -= view.scrollY;
		}
		visibleRect.set(x, y, x + getWidth(), y + getHeight());
		return getWidth() > 0 && getHeight() > 0;
	}

	public boolean onCheckIsTextEditor() { return false; }

	public boolean hasOnClickListeners() { return false; }

	public void setTextAlignment(int textAlignment) {
		String[] classesToRemove = {"ATL-text-align-left", "ATL-text-align-center", "ATL-text-align-right"};
		native_removeClasses(widget, classesToRemove);

		switch (textAlignment) {
			case TEXT_ALIGNMENT_CENTER:
				native_addClass(widget, "ATL-text-align-center");
				break;
			case TEXT_ALIGNMENT_TEXT_START:
			case TEXT_ALIGNMENT_VIEW_START:
				native_addClass(widget, "ATL-text-align-left");
				break;
			case TEXT_ALIGNMENT_TEXT_END:
			case TEXT_ALIGNMENT_VIEW_END:
				native_addClass(widget, "ATL-text-align-right");
				break;
		}
	}

	private boolean hapticFeedbackEnabled = true;
	public void setHapticFeedbackEnabled(boolean hapticFeedbackEnabled) {
		this.hapticFeedbackEnabled = hapticFeedbackEnabled;
	}
	public boolean isHapticFeedbackEnabled() {
		return hapticFeedbackEnabled;
	}

	public StateListAnimator getStateListAnimator() { return null; }

	public void requestFitSystemWindows() {}

	public boolean isPressed() { return pressed; }

	public void getWindowVisibleDisplayFrame(Rect rect) {
		ViewRootImpl root = getViewRootImpl();
		if (root != null && root.getWidth() > 0) {
			// excludes the soft keyboard: apps measure it as the difference
			// between this and the root view's height
			rect.set(0, 0, root.getWidth(), root.getVisibleHeight());
		} else {
			Display display = new Display();
			rect.set(0, 0, display.getWidth(), display.getHeight());
		}
	}

	public void setRotation(float rotation) {
		if (this.rotation != rotation) {
			invalidate(); // damage the old position (maps through the old matrix)
			this.rotation = rotation;
			invalidate();
		}
	}

	// no 3D transforms: the canvas is 2D, so these stay no-ops
	public void setRotationX(float deg) {}
	public void setRotationY(float deg) {}

	public float getRotationX() { return 0.f; }
	public float getRotationY() { return 0.f; }

	public void setScaleX(float scaleX) {
		if (this.scaleX != scaleX) {
			invalidate();
			this.scaleX = scaleX;
			invalidate();
		}
	}

	public void setScaleY(float scaleY) {
		if (this.scaleY != scaleY) {
			invalidate();
			this.scaleY = scaleY;
			invalidate();
		}
	}

	public float getScaleX() { return scaleX; }
	public float getScaleY() { return scaleY; }

	public void setPivotX(float pivot_x) {
		pivotExplicitlySet = true;
		if (pivotX != pivot_x) {
			invalidate();
			pivotX = pivot_x;
			invalidate();
		}
	}

	public void setPivotY(float pivot_y) {
		pivotExplicitlySet = true;
		if (pivotY != pivot_y) {
			invalidate();
			pivotY = pivot_y;
			invalidate();
		}
	}

	public float getPivotX() { return pivotExplicitlySet ? pivotX : getWidth() / 2.f; }
	public float getPivotY() { return pivotExplicitlySet ? pivotY : getHeight() / 2.f; }

	public boolean hasIdentityMatrix() {
		return translationX == 0 && translationY == 0 && scaleX == 1 && scaleY == 1 && rotation == 0;
	}

	/** Transform from this view's local space to its parent's, relative to left/top. */
	public Matrix getMatrix() {
		if (matrix == null)
			matrix = new Matrix();
		matrix.setTranslate(translationX, translationY);
		if (rotation != 0)
			matrix.preRotate(rotation, getPivotX(), getPivotY());
		if (scaleX != 1 || scaleY != 1)
			matrix.preScale(scaleX, scaleY, getPivotX(), getPivotY());
		return matrix;
	}

	Matrix getInverseMatrix() {
		if (inverseMatrix == null)
			inverseMatrix = new Matrix();
		if (!getMatrix().invert(inverseMatrix))
			inverseMatrix.reset();
		return inverseMatrix;
	}

	public float getTranslationZ() { return 0.f; }

	public void setTranslationZ(float translationZ) {}

	public int getWindowSystemUiVisibility() {
		return 0;
	}

	public void setScrollIndicators(int indicators, int mask) {}

	public void setLayoutDirection(int layoutDirection) {}

	public ColorStateList getBackgroundTintList() { return null; }

	public PorterDuff.Mode getBackgroundTintMode() { return null; }

	public String getTransitionName() { return null; }

	public WindowId getWindowId() { return null; }

	public boolean isInLayout() {
		return false; // FIXME
	}

	private int textDirection = TEXT_DIRECTION_INHERIT;

	public void setTextDirection(int textDirection) { this.textDirection = textDirection; }

	public int getTextDirection() { return textDirection; }

	public Drawable getForeground() { return foreground; }

	public void setScrollbarFadingEnabled(boolean fadeEnabled) {
		scrollbarFadingEnabled = fadeEnabled;
	}

	public void setScrollBarStyle(int style) {
		scrollBarStyle = style;
	}

	private int verticalScrollbarPosition = 0;
	public void setVerticalScrollbarPosition(int position) {
		verticalScrollbarPosition = position;
	}
	public int getVerticalScrollbarPosition() {
		return verticalScrollbarPosition;
	}

	public void setNestedScrollingEnabled(boolean enabled) {}

	private TouchDelegate touchDelegate;
	public void setTouchDelegate(TouchDelegate touchDelegate) {
		this.touchDelegate = touchDelegate;
	}
	public TouchDelegate getTouchDelegate() {
		return touchDelegate;
	}

	public void setOnDragListener(OnDragListener onDragListener) {}

	public void setTransitionName(String transitionName) {}

	public Animation getAnimation() { return null; }

	public ViewOverlay getOverlay() {
		return new ViewOverlay();
	}

	public void cancelLongPress() {}

	public int getTextAlignment() { return 0; }

	public View findViewWithTag(Object tag) {
		if (Objects.equals(tag, this.tag))
			return this;
		else
			return null;
	}

	private CharSequence tooltipText;
	public void setTooltipText(CharSequence tooltip) {
		tooltipText = tooltip;
	}
	public CharSequence getTooltipText() {
		return tooltipText;
	}

	public int getImportantForAutofill() { return 0; }

	public void setImportantForAutofill(int flag) {}

	public void setDefaultFocusHighlightEnabled(boolean enabled) {}

	public void setHorizontalFadingEdgeEnabled(boolean horizontalFadingEdgeEnabled) {}

	// A single stable Handler per View: post() and removeCallbacks() must share
	// one instance, since MessageQueue matches pending runnables by their target
	// Handler. A fresh Handler each call would make removeCallbacks() a no-op.
	private Handler handler;
	public Handler getHandler() {
		if (handler == null)
			handler = new Handler(Looper.getMainLooper());
		return handler;
	}

	public boolean isHardwareAccelerated() {
		return false;
	}

	private Rect clipBounds;
	public void setClipBounds(Rect clipBounds) {
		this.clipBounds = clipBounds != null ? new Rect(clipBounds) : null;
	}
	public Rect getClipBounds() {
		return clipBounds != null ? new Rect(clipBounds) : null;
	}
	public boolean getClipBounds(Rect outRect) {
		if (clipBounds == null)
			return false;
		outRect.set(clipBounds);
		return true;
	}


	public void setLeft(int left) {
		layout(left, top, right, bottom);
	}

	public void setTop(int top) {
		layout(left, top, right, bottom);
	}

	public void setRight(int right) {
		layout(left, top, right, bottom);
	}

	public void setBottom(int bottom) {
		layout(left, top, right, bottom);
	}

	private float cameraDistance = 0;
	public void setCameraDistance(float distance) {
		cameraDistance = distance;
	}

	public boolean requestRectangleOnScreen(Rect rectangle, boolean immediate) { return false; }

	public boolean requestRectangleOnScreen(Rect rectangle) { return false; }

	public boolean onKeyDown(int keyCode, KeyEvent event) {
		return false;
	}

	/** A typed character (Unicode codepoint, post keyboard-layout). Editable views override. */
	/** IME committed text (finalized); default treats it as typed input. */
	public boolean onCommitText(CharSequence text) {
		boolean any = false;
		for (int i = 0; i < text.length(); ) {
			int cp = Character.codePointAt(text, i);
			any |= onTextInput(cp);
			i += Character.charCount(cp);
		}
		return any;
	}

	/** IME composing (preedit) text; overridden by editable views. */
	public boolean onComposingText(CharSequence text) {
		return false;
	}

	/** Finalize any composing region, keeping its current text. */
	public void onFinishComposing() {
	}

	public boolean onTextInput(int codePoint) {
		return false;
	}

	public boolean dispatchKeyEvent(KeyEvent event) {
		if (onKeyListener != null && onKeyListener.onKey(this, event.getKeyCode(), event))
			return true;
		switch (event.getAction()) {
			case KeyEvent.ACTION_DOWN:
				return onKeyDown(event.getKeyCode(), event);
			case KeyEvent.ACTION_UP:
				return onKeyUp(event.getKeyCode(), event);
			case KeyEvent.ACTION_MULTIPLE:
				return onKeyMultiple(event.getKeyCode(), event.getRepeatCount(), event);
		}
		return false;
	}

	public WindowInsets getRootWindowInsets() {
		ViewRootImpl root = getViewRootImpl();
		return root != null ? root.getWindowInsets() : null; // AOSP: null while detached
	}

	public PointerIcon getPointerIcon() { return null; }

	public void setPointerIcon(PointerIcon pointerIcon) {}

	public IBinder getApplicationWindowToken() { return null; }

	public int getVerticalFadingEdgeLength() { return 0; }
	public int getVerticalScrollbarWidth() { return 0; }

	public void saveAttributeDataForStyleable(Context ctxt, int[] styleable, AttributeSet attrs, TypedArray t, int defStyleAttr, int defStyleRes) {}

	public final View requireViewById(int id) {
		View view = findViewById(id);
		if (view == null)
			throw new IllegalArgumentException("ID does not reference a View inside this View");
		return view;
	}
	public float getTransitionAlpha() {
		return transitionAlpha;
	}

	public void onWindowFocusChanged(boolean hasFocus) {}

	public void setAnimation(Animation animation) {}

	public boolean performAccessibilityAction(int action, Bundle arguments) { return false; }

	public boolean isDirty() { return false; }

	public float getX() {
		return getLeft() + getTranslationX();
	}
	public float getY() {
		return getTop() + getTranslationY();
	}

	public boolean getGlobalVisibleRect(Rect visibleRect, Point globalOffset) {
		boolean result = getGlobalVisibleRect(visibleRect);
		globalOffset.set(visibleRect.left, visibleRect.top);
		return result;
	}

	public void restoreHierarchyState(SparseArray<Parcelable> container) {}

	public boolean isHovered() { return false; }

	public void scrollBy(int x, int y) {
		scrollTo(scrollX + x, scrollY + y);
	}

	public void setAccessibilityPaneTitle(CharSequence paneTitle) {}

	public void setAccessibilityHeading(boolean heading) {}

	public WindowInsets computeSystemWindowInsets(WindowInsets insets, Rect contentInsets) { return insets; }

	public boolean isDuplicateParentStateEnabled() { return false; }

	public void setBackgroundTintMode(PorterDuff.Mode tintMode) {}

	private int nextFocusLeftId = NO_ID;
	public void setNextFocusLeftId(int id) { nextFocusLeftId = id; }
	public int getNextFocusLeftId() { return nextFocusLeftId; }

	private int nextFocusRightId = NO_ID;
	public void setNextFocusRightId(int id) { nextFocusRightId = id; }
	public int getNextFocusRightId() { return nextFocusRightId; }

	private int nextFocusDownId = NO_ID;
	public void setNextFocusDownId(int id) { nextFocusDownId = id; }
	public int getNextFocusDownId() { return nextFocusDownId; }

	private int nextFocusUpId = NO_ID;
	public void setNextFocusUpId(int id) { nextFocusUpId = id; }
	public int getNextFocusUpId() { return nextFocusUpId; }

	private int nextFocusForwardId = NO_ID;
	public void setNextFocusForwardId(int id) { nextFocusForwardId = id; }
	public int getNextFocusForwardId() { return nextFocusForwardId; }

	public void setHasTransientState(boolean hasTransientState) {}

	protected void onConfigurationChanged(Configuration newConfig) {}

	public boolean isDrawingCacheEnabled() { return false; }

	public void setDrawingCacheEnabled(boolean enabled) {}

	public void buildDrawingCache(boolean autoScale) {}

	public Bitmap getDrawingCache() { return null; }

	public void announceForAccessibility(CharSequence text) {}

	public WindowInsetsController getWindowInsetsController() {
		return native_get_window(widget).getInsetsController();
	}

	public final boolean isKeyboardNavigationCluster() { return false; }

	public void setKeyboardNavigationCluster(boolean isCluster) {}

	public AutofillId getAutofillId() { return new AutofillId(); }

	public AccessibilityDelegate getAccessibilityDelegate() { return null; }

	public void setScrollX(int value) {
		this.scrollX = value;
	}
	public void setScrollY(int value) {
		this.scrollY = value;
	}

	protected boolean dispatchHoverEvent(MotionEvent event) {
		if (onHoverListener != null && onHoverListener.onHover(this, event))
			return true;
		return onHoverEvent(event);
	}

	public ActionMode startActionMode(ActionMode.Callback callback, int type) { return null; }

	public InputConnection onCreateInputConnection(EditorInfo outAttrs) { return null; }

	public ActionMode startActionMode(ActionMode.Callback callback) { return null; }

	public void getFocusedRect(Rect r) {}

	/**
	* An implementation of OnClickListener that attempts to lazily load a
	* named click handling method from a parent or ancestor context.
	* See AOSP commit d13f0e21bf8059779c9330a5993907ec88c8e781
	* Copyright (C) 2006 The Android Open Source Project
	*/
	private static class DeclaredOnClickListener implements OnClickListener {
		private final View mHostView;
		private final String mMethodName;
		private Method mResolvedMethod;
		private Context mResolvedContext;
		public DeclaredOnClickListener(@NonNull View hostView, @NonNull String methodName) {
			mHostView = hostView;
			mMethodName = methodName;
		}
		@Override
		public void onClick(@NonNull View v) {
			if (mResolvedMethod == null)
				resolveMethod(mHostView.getContext(), mMethodName);
			try {
				mResolvedMethod.invoke(mResolvedContext, v);
			} catch (IllegalAccessException e) {
				throw new IllegalStateException(
				    "Could not execute non-public method for android:onClick", e);
			} catch (InvocationTargetException e) {
				throw new IllegalStateException(
				    "Could not execute method for android:onClick", e);
			}
		}

		@NonNull
		private void resolveMethod(@Nullable Context context, @NonNull String name) {
			while (context != null) {
				try {
					if (!context.isRestricted()) {
						final Method method = context.getClass().getMethod(mMethodName, View.class);
						if (method != null) {
							mResolvedMethod = method;
							mResolvedContext = context;
							return;
						}
					}
				} catch (NoSuchMethodException e) {
					// Failed to find method, keep searching up the hierarchy.
				}
				if (context instanceof ContextWrapper) {
					context = ((ContextWrapper)context).getBaseContext();
				} else {
					// Can't search up the hierarchy, null out and fail.
					context = null;
				}
			}

			final int id = mHostView.getId();
			final String idText = id == NO_ID ? "" : " with id '" + mHostView.getContext().getResources().getResourceEntryName(id) + "'";
			throw new IllegalStateException("Could not find method " + mMethodName
			                                + "(View) in a parent or ancestor Context for android:onClick "
			                                + "attribute defined on view " + mHostView.getClass() + idText);
		}
	}
	public void setForceDarkAllowed(boolean forceDark) {}
	public void setWindowInsetsAnimationCallback(WindowInsetsAnimation.Callback callback) {}
	public void setViewTranslationCallback(ViewTranslationCallback callback) {}
	public void clearViewTranslationCallback() {}
	public void transformMatrixToGlobal(Matrix matrix) {}
	public boolean isShowingLayoutBounds() {
		return true;
	}

	public boolean callOnClick() {
		if (on_click_listener != null) {
			on_click_listener.onClick(this);
			return true;
		} else {
			return false;
		}
	}

	public void destroyDrawingCache() {}

	public void setHovered(boolean isHovered) {}

	public void setVerticalFadingEdgeEnabled(boolean verticalFadingEdgeEnabled) {}

	public void setFadingEdgeLength(int length) {}

	public int getAccessibilityLiveRegion() { return 0; }

	public void setLabelFor(int id) {}

	/* --- AOSP-13 API surface: methods apps call or override-and-chain to super.
	 * Implementations are simple but semantically faithful; subsystems ATL does
	 * not have (scrollbar rendering, RTL resolution, pointer capture, IME
	 * pre-dispatch) report their inert defaults. --- */

	/* key events */

	public boolean onKeyUp(int keyCode, KeyEvent event) {
		if ((keyCode == KeyEvent.KEYCODE_DPAD_CENTER || keyCode == KeyEvent.KEYCODE_ENTER)
		    && isClickable() && isEnabled()) {
			return performClick();
		}
		return false;
	}

	public boolean onKeyLongPress(int keyCode, KeyEvent event) {
		return false;
	}

	public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
		return false;
	}

	public boolean onKeyPreIme(int keyCode, KeyEvent event) {
		return false;
	}

	public boolean onKeyShortcut(int keyCode, KeyEvent event) {
		return false;
	}

	public boolean dispatchKeyEventPreIme(KeyEvent event) {
		return onKeyPreIme(event.getKeyCode(), event);
	}

	public boolean dispatchKeyShortcutEvent(KeyEvent event) {
		return onKeyShortcut(event.getKeyCode(), event);
	}

	private KeyEvent.DispatcherState keyDispatcherState;

	public KeyEvent.DispatcherState getKeyDispatcherState() {
		if (keyDispatcherState == null)
			keyDispatcherState = new KeyEvent.DispatcherState();
		return keyDispatcherState;
	}

	private java.util.ArrayList<OnUnhandledKeyEventListener> unhandledKeyListeners;

	/** listeners are recorded but never invoked: unhandled keys are consumed by the window */
	public void addOnUnhandledKeyEventListener(OnUnhandledKeyEventListener listener) {
		if (unhandledKeyListeners == null)
			unhandledKeyListeners = new java.util.ArrayList<>();
		unhandledKeyListeners.add(listener);
	}

	public void removeOnUnhandledKeyEventListener(OnUnhandledKeyEventListener listener) {
		if (unhandledKeyListeners != null)
			unhandledKeyListeners.remove(listener);
	}

	public boolean checkInputConnectionProxy(View view) {
		return false;
	}

	public void onCancelPendingInputEvents() {}

	public void requestUnbufferedDispatch(MotionEvent event) {}

	public void requestUnbufferedDispatch(int source) {}

	/* focus */

	protected void onFocusLost() {}

	public void addFocusables(java.util.ArrayList<View> views, int direction) {
		addFocusables(views, direction, FOCUSABLES_TOUCH_MODE);
	}

	public void addFocusables(java.util.ArrayList<View> views, int direction, int focusableMode) {
		if (views != null && getFocusable() != NOT_FOCUSABLE && getVisibility() == VISIBLE)
			views.add(this);
	}

	public java.util.ArrayList<View> getFocusables(int direction) {
		java.util.ArrayList<View> result = new java.util.ArrayList<View>(24);
		addFocusables(result, direction);
		return result;
	}

	public View focusSearch(int direction) {
		return null;
	}

	public boolean hasExplicitFocusable() {
		return getFocusable() != NOT_FOCUSABLE;
	}

	private boolean focusedByDefault = false;

	public void setFocusedByDefault(boolean isFocusedByDefault) {
		focusedByDefault = isFocusedByDefault;
	}

	public final boolean isFocusedByDefault() {
		return focusedByDefault;
	}

	public boolean restoreDefaultFocus() {
		return requestFocus();
	}

	public void dispatchWindowFocusChanged(boolean hasFocus) {
		onWindowFocusChanged(hasFocus);
	}

	public boolean isAccessibilityFocused() {
		return false;
	}

	private boolean screenReaderFocusable = false;

	public void setScreenReaderFocusable(boolean screenReaderFocusable) {
		this.screenReaderFocusable = screenReaderFocusable;
	}

	public boolean isScreenReaderFocusable() {
		return screenReaderFocusable;
	}

	/* scrolling */

	protected int computeHorizontalScrollOffset() {
		return scrollX;
	}

	protected boolean overScrollBy(int deltaX, int deltaY, int scrollX, int scrollY,
	                               int scrollRangeX, int scrollRangeY,
	                               int maxOverScrollX, int maxOverScrollY, boolean isTouchEvent) {
		int newScrollX = scrollX + deltaX;
		int newScrollY = scrollY + deltaY;
		boolean clampedX = false;
		boolean clampedY = false;
		if (newScrollX > scrollRangeX) {
			newScrollX = scrollRangeX;
			clampedX = true;
		} else if (newScrollX < 0) {
			newScrollX = 0;
			clampedX = true;
		}
		if (newScrollY > scrollRangeY) {
			newScrollY = scrollRangeY;
			clampedY = true;
		} else if (newScrollY < 0) {
			newScrollY = 0;
			clampedY = true;
		}
		onOverScrolled(newScrollX, newScrollY, clampedX, clampedY);
		return clampedX || clampedY;
	}

	protected void onOverScrolled(int scrollX, int scrollY, boolean clampedX, boolean clampedY) {}

	public boolean startNestedScroll(int axes) {
		return false;
	}

	public boolean hasNestedScrollingParent() {
		return false;
	}

	public boolean dispatchNestedFling(float velocityX, float velocityY, boolean consumed) {
		return false;
	}

	public boolean dispatchNestedPreFling(float velocityX, float velocityY) {
		return false;
	}

	protected boolean awakenScrollBars(int startDelay) {
		return false;
	}

	protected boolean awakenScrollBars(int startDelay, boolean invalidate) {
		return false;
	}

	private int scrollBarSize = 0;
	private int scrollBarFadeDuration = 250;
	private int scrollBarDefaultDelayBeforeFade = 300;

	public int getScrollBarSize() {
		return scrollBarSize != 0 ? scrollBarSize
		    : (int)(4 * getContext().getResources().getDisplayMetrics().density);
	}

	public void setScrollBarSize(int scrollBarSize) {
		this.scrollBarSize = scrollBarSize;
	}

	public int getScrollBarFadeDuration() {
		return scrollBarFadeDuration;
	}

	public void setScrollBarFadeDuration(int scrollBarFadeDuration) {
		this.scrollBarFadeDuration = scrollBarFadeDuration;
	}

	public int getScrollBarDefaultDelayBeforeFade() {
		return scrollBarDefaultDelayBeforeFade;
	}

	public void setScrollBarDefaultDelayBeforeFade(int scrollBarDefaultDelayBeforeFade) {
		this.scrollBarDefaultDelayBeforeFade = scrollBarDefaultDelayBeforeFade;
	}

	public int getHorizontalScrollbarHeight() {
		return getScrollBarSize();
	}

	public void setVerticalScrollbarThumbDrawable(Drawable drawable) {}

	public void setVerticalScrollbarTrackDrawable(Drawable drawable) {}

	public void setHorizontalScrollbarThumbDrawable(Drawable drawable) {}

	public void setHorizontalScrollbarTrackDrawable(Drawable drawable) {}

	protected void onDrawScrollBars(Canvas canvas) {}

	protected void initializeScrollbars(TypedArray a) {}

	public boolean isInScrollingContainer() {
		return false;
	}

	protected boolean isVerticalScrollBarHidden() {
		return false;
	}

	public int getScrollIndicators() {
		return 0;
	}

	public void setScrollIndicators(int indicators) {}

	protected float getVerticalScrollFactor() {
		return 64 * getContext().getResources().getDisplayMetrics().density;
	}

	protected float getHorizontalScrollFactor() {
		return getVerticalScrollFactor();
	}

	/* fading edges: never rendered, state round-trips */

	private boolean horizontalFadingEdgeEnabled = false;
	private boolean verticalFadingEdgeEnabled = false;

	public boolean isHorizontalFadingEdgeEnabled() {
		return horizontalFadingEdgeEnabled;
	}

	public boolean isVerticalFadingEdgeEnabled() {
		return verticalFadingEdgeEnabled;
	}

	public int getHorizontalFadingEdgeLength() {
		return 0;
	}

	protected float getLeftFadingEdgeStrength() {
		return 0.0f;
	}

	protected float getRightFadingEdgeStrength() {
		return 0.0f;
	}

	protected float getTopFadingEdgeStrength() {
		return 0.0f;
	}

	protected float getBottomFadingEdgeStrength() {
		return 0.0f;
	}

	/* hover / pointer */

	public boolean onHoverEvent(MotionEvent event) {
		return false;
	}

	public void onHoverChanged(boolean hovered) {}

	protected boolean dispatchGenericPointerEvent(MotionEvent event) {
		return onGenericMotionEvent(event);
	}

	protected boolean dispatchGenericFocusedEvent(MotionEvent event) {
		return onGenericMotionEvent(event);
	}

	public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
		return getPointerIcon();
	}

	public boolean hasPointerCapture() {
		return false;
	}

	public void requestPointerCapture() {}

	public void releasePointerCapture() {}

	public void onPointerCaptureChange(boolean hasCapture) {}

	public void dispatchPointerCaptureChanged(boolean hasCapture) {
		onPointerCaptureChange(hasCapture);
	}

	public boolean onCapturedPointerEvent(MotionEvent event) {
		return false;
	}

	/* window / insets */

	public boolean fitSystemWindows(Rect insets) {
		return false;
	}

	public void dispatchWindowVisibilityChanged(int visibility) {
		onWindowVisibilityChanged(visibility);
	}

	protected void onWindowVisibilityChanged(int visibility) {}

	private int windowAttachCount = 0;

	public int getWindowAttachCount() {
		return windowAttachCount;
	}

	/* temporary detach */

	private boolean temporarilyDetached = false;

	public void onStartTemporaryDetach() {
		temporarilyDetached = true;
	}

	public void onFinishTemporaryDetach() {
		temporarilyDetached = false;
	}

	public void dispatchStartTemporaryDetach() {
		onStartTemporaryDetach();
	}

	public void dispatchFinishTemporaryDetach() {
		onFinishTemporaryDetach();
	}

	public final boolean isTemporarilyDetached() {
		return temporarilyDetached;
	}

	/* protected hooks subclasses override and chain to super */

	protected boolean verifyDrawable(Drawable who) {
		return who == background || who == foreground;
	}

	public void onDrawForeground(Canvas canvas) {
		Drawable fg = getForeground();
		if (fg != null) {
			fg.setBounds(0, 0, getWidth(), getHeight());
			fg.draw(canvas);
		}
	}

	protected boolean onSetAlpha(int alpha) {
		return false;
	}

	protected void dispatchSetPressed(boolean pressed) {}

	protected void dispatchSetActivated(boolean activated) {}

	protected void dispatchSetSelected(boolean selected) {}

	public void onVisibilityAggregated(boolean isVisible) {}

	public void onRtlPropertiesChanged(int layoutDirection) {}

	public boolean isLayoutRtl() {
		return false;
	}

	public void onScreenStateChanged(int screenState) {}

	protected void onDisplayHint(int hint) {}

	public void dispatchDisplayHint(int hint) {
		onDisplayHint(hint);
	}

	public boolean onTrackballEvent(MotionEvent event) {
		return false;
	}

	public boolean dispatchTrackballEvent(MotionEvent event) {
		return onTrackballEvent(event);
	}

	public boolean onFilterTouchEventForSecurity(MotionEvent event) {
		return true;
	}

	/* public in AOSP: apps call it from their own touch listeners */
	public void drawableHotspotChanged(float x, float y) {}

	public void dispatchDrawableHotspotChanged(float x, float y) {
		drawableHotspotChanged(x, y);
	}

	/* context menu / context click */

	public void createContextMenu(ContextMenu menu) {
		onCreateContextMenu(menu);
	}

	protected void onCreateContextMenu(ContextMenu menu) {}

	protected ContextMenu.ContextMenuInfo getContextMenuInfo() {
		return null;
	}

	public boolean showContextMenu() {
		return false;
	}

	public boolean showContextMenu(float x, float y) {
		return false;
	}

	public boolean performContextClick() {
		return performContextClick(0, 0);
	}

	public boolean performContextClick(float x, float y) {
		return on_context_click_listener != null && on_context_click_listener.onContextClick(this);
	}

	private boolean contextClickable = false;

	public void setContextClickable(boolean contextClickable) {
		this.contextClickable = contextClickable;
	}

	public boolean isContextClickable() {
		return contextClickable;
	}

	/* misc state round-trips */

	public boolean getKeepScreenOn() {
		return keepScreenOn;
	}

	private boolean soundEffectsEnabled = true;

	public void setSoundEffectsEnabled(boolean soundEffectsEnabled) {
		this.soundEffectsEnabled = soundEffectsEnabled;
	}

	public boolean isSoundEffectsEnabled() {
		return soundEffectsEnabled;
	}

	private boolean filterTouchesWhenObscured = false;

	public void setFilterTouchesWhenObscured(boolean enabled) {
		filterTouchesWhenObscured = enabled;
	}

	public boolean getFilterTouchesWhenObscured() {
		return filterTouchesWhenObscured;
	}

	public OnLongClickListener getOnLongClickListener() {
		return on_long_click_listener;
	}

	public boolean hasOnLongClickListeners() {
		return on_long_click_listener != null;
	}

	public boolean performLongClick() {
		return performLongClick(0, 0);
	}

	public void setAllowClickWhenDisabled(boolean clickableWhenDisabled) {}

	public void setStateDescription(CharSequence stateDescription) {}

	private java.util.List<Rect> systemGestureExclusionRects = java.util.Collections.emptyList();

	public void setSystemGestureExclusionRects(java.util.List<Rect> rects) {
		systemGestureExclusionRects = rects;
	}

	public java.util.List<Rect> getSystemGestureExclusionRects() {
		return systemGestureExclusionRects;
	}

	private boolean preferKeepClear = false;
	private java.util.List<Rect> preferKeepClearRects = java.util.Collections.emptyList();

	public void setPreferKeepClear(boolean preferKeepClear) {
		this.preferKeepClear = preferKeepClear;
	}

	public boolean isPreferKeepClear() {
		return preferKeepClear;
	}

	public void setPreferKeepClearRects(java.util.List<Rect> rects) {
		preferKeepClearRects = rects;
	}

	public java.util.List<Rect> getPreferKeepClearRects() {
		return preferKeepClearRects;
	}

	private boolean willNotCacheDrawing = false;

	public void setWillNotCacheDrawing(boolean willNotCacheDrawing) {
		this.willNotCacheDrawing = willNotCacheDrawing;
	}

	public boolean willNotCacheDrawing() {
		return willNotCacheDrawing;
	}

	public void buildLayer() {}

	/* rendering properties */

	public boolean isOpaque() {
		return false;
	}

	private boolean forcedHasOverlappingRendering = false;
	private boolean forcedOverlappingRenderingValue = true;

	public boolean hasOverlappingRendering() {
		return true;
	}

	public final boolean getHasOverlappingRendering() {
		return forcedHasOverlappingRendering ? forcedOverlappingRenderingValue : hasOverlappingRendering();
	}

	public void forceHasOverlappingRendering(boolean hasOverlappingRendering) {
		forcedHasOverlappingRendering = true;
		forcedOverlappingRenderingValue = hasOverlappingRendering;
	}

	public int getSolidColor() {
		return 0;
	}

	public void setOutlineAmbientShadowColor(int color) {}

	public void setOutlineSpotShadowColor(int color) {}

	public void setZ(float z) {
		setTranslationZ(z - getElevation());
	}

	public float getCameraDistance() {
		return cameraDistance;
	}

	public void setTransitionAlpha(float alpha) {
		if (transitionAlpha != alpha) {
			transitionAlpha = alpha;
			invalidate();
		}
	}

	public void setTransitionVisibility(int visibility) {
		setVisibility(visibility);
	}

	private android.graphics.Matrix animationMatrix;

	public void setAnimationMatrix(android.graphics.Matrix matrix) {
		animationMatrix = matrix;
	}

	public android.graphics.Matrix getAnimationMatrix() {
		return animationMatrix;
	}

	public boolean isPivotSet() {
		return false;
	}

	public void resetPivot() {
		if (pivotExplicitlySet) {
			pivotExplicitlySet = false;
			invalidate();
		}
	}

	/* geometry helpers */

	public int getMeasuredHeightAndState() {
		return measuredHeight;
	}

	public int[] getLocationOnScreen() {
		int[] location = new int[2];
		getLocationOnScreen(location);
		return location;
	}

	public void getBoundsOnScreen(Rect outRect) {
		getBoundsOnScreen(outRect, false);
	}

	public void getBoundsOnScreen(Rect outRect, boolean clipToParent) {
		int[] location = new int[2];
		getLocationOnScreen(location);
		outRect.set(location[0], location[1], location[0] + getWidth(), location[1] + getHeight());
	}

	final boolean pointInView(float localX, float localY, float slop) {
		return localX >= -slop && localY >= -slop && localX < (getWidth() + slop)
		    && localY < (getHeight() + slop);
	}

	protected boolean setFrame(int left, int top, int right, int bottom) {
		boolean changed = this.left != left || this.top != top
		    || this.right != right || this.bottom != bottom;
		if (changed)
			invalidate(); // damage the old position (AOSP setFrame)
		this.left = left;
		this.top = top;
		this.right = right;
		this.bottom = bottom;
		if (changed)
			invalidate();
		return changed;
	}

	public void setLeftTopRightBottom(int left, int top, int right, int bottom) {
		setFrame(left, top, right, bottom);
	}

	public boolean isVisibleToUser() {
		return isAttachedToWindow() && getVisibility() == VISIBLE;
	}

	public void getWindowDisplayFrame(Rect outRect) {
		getWindowVisibleDisplayFrame(outRect);
	}

	public long getUniqueDrawingId() {
		return System.identityHashCode(this);
	}

	public int getSourceLayoutResId() {
		return 0;
	}

	public int getExplicitStyle() {
		return 0;
	}

	/* touchables */

	public java.util.ArrayList<View> getTouchables() {
		java.util.ArrayList<View> result = new java.util.ArrayList<View>();
		addTouchables(result);
		return result;
	}

	public void addTouchables(java.util.ArrayList<View> views) {
		if (isClickable() && getVisibility() == VISIBLE)
			views.add(this);
	}

	/* invalidation */

	public void invalidate(boolean invalidateCache) {
		invalidate();
	}

	public void postInvalidateDelayed(long delayMilliseconds) {
		new Handler(Looper.getMainLooper()).postDelayed(new Runnable() {
			@Override
			public void run() {
				invalidate();
			}
		}, delayMilliseconds);
	}

	public void postInvalidateDelayed(long delayMilliseconds, int left, int top, int right, int bottom) {
		postInvalidateDelayed(delayMilliseconds);
	}

	/* content receiving */

	public String[] getReceiveContentMimeTypes() {
		return null;
	}

	public ContentInfo performReceiveContent(ContentInfo payload) {
		return onReceiveContent(payload);
	}

	/* RTL / text direction resolution: ATL is LTR-only for now */

	public int getRawLayoutDirection() {
		return LAYOUT_DIRECTION_LTR;
	}

	public boolean canResolveLayoutDirection() {
		return true;
	}

	public boolean isLayoutDirectionInherited() {
		return false;
	}

	public boolean canResolveTextDirection() {
		return true;
	}

	public boolean isTextDirectionInherited() {
		return false;
	}

	public boolean isTextDirectionResolved() {
		return true;
	}

	public boolean canResolveTextAlignment() {
		return true;
	}

	public boolean isTextAlignmentInherited() {
		return false;
	}

	public boolean isTextAlignmentResolved() {
		return true;
	}

	public boolean resolveLayoutDirection() {
		return true;
	}

	public void resolveLayoutParams() {}

	public boolean resolveTextDirection() {
		return true;
	}

	public boolean resolveTextAlignment() {
		return true;
	}

	public void resolvePadding() {}

	public boolean resolveRtlPropertiesIfNeeded() {
		return false;
	}

	protected void resetResolvedLayoutDirection() {}

	protected void resetResolvedTextDirection() {}

	protected void resetResolvedTextAlignment() {}

	protected void resetResolvedPadding() {}

	protected void resetResolvedDrawables() {}

	protected void resolveDrawables() {}

	protected void onResolveDrawables(int layoutDirection) {}

	public void resetRtlProperties() {}
}
