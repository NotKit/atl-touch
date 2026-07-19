package android.view.animation;

import android.content.Context;
import android.util.AttributeSet;

/**
 * Minimal stub: ATL does not run layout animations. Present so views that set a
 * layout-animation controller (Telegram's EmojiView passes null) link.
 */
public class LayoutAnimationController {

	public static final int ORDER_NORMAL = 0;
	public static final int ORDER_REVERSE = 1;
	public static final int ORDER_RANDOM = 2;

	private Animation animation;
	private float delay;
	private int order;
	private Interpolator interpolator;

	public LayoutAnimationController() {}

	public LayoutAnimationController(Animation animation) {
		this(animation, 0.5f);
	}

	public LayoutAnimationController(Animation animation, float delay) {
		this.animation = animation;
		this.delay = delay;
	}

	public LayoutAnimationController(Context context, AttributeSet attrs) {}

	public void setDelay(float delay) { this.delay = delay; }
	public float getDelay() { return delay; }

	public void setOrder(int order) { this.order = order; }
	public int getOrder() { return order; }

	public void setAnimation(Animation animation) { this.animation = animation; }
	public Animation getAnimation() { return animation; }

	public void setInterpolator(Interpolator interpolator) { this.interpolator = interpolator; }
	public Interpolator getInterpolator() { return interpolator; }

	public boolean isDone() { return true; }
}
