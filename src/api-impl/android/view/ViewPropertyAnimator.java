package android.view;

import android.animation.Animator;
import android.animation.TimeInterpolator;
import android.animation.ValueAnimator;
import java.util.ArrayList;
import java.util.HashMap;

/* Follows AOSP's ViewPropertyAnimator: each start() runs one ValueAnimator over
 * the properties queued since the last one, a new animation on a property takes
 * it over from the one already running, and start/end actions belong to a
 * single animation and are dropped when it is cancelled. */
public class ViewPropertyAnimator {

	private static final int TRANSLATION_X = 0x0001;
	private static final int TRANSLATION_Y = 0x0002;
	private static final int TRANSLATION_Z = 0x0004;
	private static final int SCALE_X = 0x0008;
	private static final int SCALE_Y = 0x0010;
	private static final int ROTATION = 0x0020;
	private static final int ROTATION_X = 0x0040;
	private static final int ROTATION_Y = 0x0080;
	private static final int X = 0x0100;
	private static final int Y = 0x0200;
	private static final int Z = 0x0400;
	private static final int ALPHA = 0x0800;

	private final View view;
	private long duration;
	private boolean durationSet = false;
	private long startDelay = 0;
	private TimeInterpolator interpolator;
	private boolean interpolatorSet = false;
	private Animator.AnimatorListener listener;
	private ValueAnimator.AnimatorUpdateListener updateListener;

	private final ArrayList<NameValuesHolder> pendingAnimations = new ArrayList<>();
	private Runnable pendingOnStartAction;
	private Runnable pendingOnEndAction;
	private final HashMap<Animator, PropertyBundle> animatorMap = new HashMap<>();
	private final HashMap<Animator, Runnable> animatorOnStartMap = new HashMap<>();
	private final HashMap<Animator, Runnable> animatorOnEndMap = new HashMap<>();
	private final AnimatorEventListener animatorEventListener = new AnimatorEventListener();

	private final Runnable animationStarter = new Runnable() {
		@Override
		public void run() {
			startAnimation();
		}
	};

	private static class NameValuesHolder {
		int nameConstant;
		float fromValue;
		float deltaValue;

		NameValuesHolder(int nameConstant, float fromValue, float deltaValue) {
			this.nameConstant = nameConstant;
			this.fromValue = fromValue;
			this.deltaValue = deltaValue;
		}
	}

	private static class PropertyBundle {
		int propertyMask;
		ArrayList<NameValuesHolder> nameValuesHolder;

		PropertyBundle(int propertyMask, ArrayList<NameValuesHolder> nameValuesHolder) {
			this.propertyMask = propertyMask;
			this.nameValuesHolder = nameValuesHolder;
		}

		/* removes the property from this bundle; true if it was there */
		boolean cancel(int propertyConstant) {
			if ((propertyMask & propertyConstant) != 0 && nameValuesHolder != null) {
				for (int i = 0; i < nameValuesHolder.size(); i++) {
					if (nameValuesHolder.get(i).nameConstant == propertyConstant) {
						nameValuesHolder.remove(i);
						propertyMask &= ~propertyConstant;
						return true;
					}
				}
			}
			return false;
		}
	}

	ViewPropertyAnimator(View view) {
		this.view = view;
	}

	public ViewPropertyAnimator setDuration(long duration) {
		if (duration < 0)
			throw new IllegalArgumentException("Animators cannot have negative duration: " + duration);
		durationSet = true;
		this.duration = duration;
		return this;
	}

	public long getDuration() {
		return durationSet ? duration : new ValueAnimator().getDuration();
	}

	public long getStartDelay() {
		return startDelay;
	}

	public ViewPropertyAnimator setStartDelay(long startDelay) {
		if (startDelay < 0)
			throw new IllegalArgumentException("Animators cannot have negative start delay: " + startDelay);
		this.startDelay = startDelay;
		return this;
	}

	public ViewPropertyAnimator setInterpolator(TimeInterpolator interpolator) {
		interpolatorSet = true;
		this.interpolator = interpolator;
		return this;
	}

	public TimeInterpolator getInterpolator() {
		return interpolator;
	}

	public ViewPropertyAnimator setListener(Animator.AnimatorListener listener) {
		this.listener = listener;
		return this;
	}

	public ViewPropertyAnimator setUpdateListener(ValueAnimator.AnimatorUpdateListener listener) {
		this.updateListener = listener;
		return this;
	}

	public void start() {
		view.removeCallbacks(animationStarter);
		startAnimation();
	}

	public void cancel() {
		if (animatorMap.size() > 0) {
			for (Animator runningAnim : new ArrayList<>(animatorMap.keySet()))
				runningAnim.cancel();
		}
		pendingAnimations.clear();
		pendingOnStartAction = null;
		pendingOnEndAction = null;
		view.removeCallbacks(animationStarter);
	}

	public ViewPropertyAnimator x(float value) { animateProperty(X, value); return this; }
	public ViewPropertyAnimator xBy(float value) { animatePropertyBy(X, value); return this; }
	public ViewPropertyAnimator y(float value) { animateProperty(Y, value); return this; }
	public ViewPropertyAnimator yBy(float value) { animatePropertyBy(Y, value); return this; }
	public ViewPropertyAnimator z(float value) { animateProperty(Z, value); return this; }
	public ViewPropertyAnimator zBy(float value) { animatePropertyBy(Z, value); return this; }
	public ViewPropertyAnimator rotation(float value) { animateProperty(ROTATION, value); return this; }
	public ViewPropertyAnimator rotationBy(float value) { animatePropertyBy(ROTATION, value); return this; }
	public ViewPropertyAnimator rotationX(float value) { animateProperty(ROTATION_X, value); return this; }
	public ViewPropertyAnimator rotationXBy(float value) { animatePropertyBy(ROTATION_X, value); return this; }
	public ViewPropertyAnimator rotationY(float value) { animateProperty(ROTATION_Y, value); return this; }
	public ViewPropertyAnimator rotationYBy(float value) { animatePropertyBy(ROTATION_Y, value); return this; }
	public ViewPropertyAnimator translationX(float value) { animateProperty(TRANSLATION_X, value); return this; }
	public ViewPropertyAnimator translationXBy(float value) { animatePropertyBy(TRANSLATION_X, value); return this; }
	public ViewPropertyAnimator translationY(float value) { animateProperty(TRANSLATION_Y, value); return this; }
	public ViewPropertyAnimator translationYBy(float value) { animatePropertyBy(TRANSLATION_Y, value); return this; }
	public ViewPropertyAnimator translationZ(float value) { animateProperty(TRANSLATION_Z, value); return this; }
	public ViewPropertyAnimator translationZBy(float value) { animatePropertyBy(TRANSLATION_Z, value); return this; }
	public ViewPropertyAnimator scaleX(float value) { animateProperty(SCALE_X, value); return this; }
	public ViewPropertyAnimator scaleXBy(float value) { animatePropertyBy(SCALE_X, value); return this; }
	public ViewPropertyAnimator scaleY(float value) { animateProperty(SCALE_Y, value); return this; }
	public ViewPropertyAnimator scaleYBy(float value) { animatePropertyBy(SCALE_Y, value); return this; }
	public ViewPropertyAnimator alpha(float value) { animateProperty(ALPHA, value); return this; }
	public ViewPropertyAnimator alphaBy(float value) { animatePropertyBy(ALPHA, value); return this; }

	public ViewPropertyAnimator withLayer() {
		return this;
	}

	public ViewPropertyAnimator withStartAction(Runnable runnable) {
		pendingOnStartAction = runnable;
		return this;
	}

	public ViewPropertyAnimator withEndAction(Runnable runnable) {
		pendingOnEndAction = runnable;
		return this;
	}

	private void startAnimation() {
		ValueAnimator animator = ValueAnimator.ofFloat(1.0f);
		@SuppressWarnings("unchecked")
		ArrayList<NameValuesHolder> nameValueList = (ArrayList<NameValuesHolder>)pendingAnimations.clone();
		pendingAnimations.clear();
		int propertyMask = 0;
		for (NameValuesHolder holder : nameValueList)
			propertyMask |= holder.nameConstant;
		animatorMap.put(animator, new PropertyBundle(propertyMask, nameValueList));
		if (pendingOnStartAction != null) {
			animatorOnStartMap.put(animator, pendingOnStartAction);
			pendingOnStartAction = null;
		}
		if (pendingOnEndAction != null) {
			animatorOnEndMap.put(animator, pendingOnEndAction);
			pendingOnEndAction = null;
		}
		animator.addUpdateListener(animatorEventListener);
		animator.addListener(animatorEventListener);
		if (startDelay != 0)
			animator.setStartDelay(startDelay);
		if (durationSet)
			animator.setDuration(duration);
		if (interpolatorSet)
			animator.setInterpolator(interpolator);
		animator.start();
	}

	private void animateProperty(int constantName, float toValue) {
		float fromValue = getValue(constantName);
		animatePropertyBy(constantName, fromValue, toValue - fromValue);
	}

	private void animatePropertyBy(int constantName, float byValue) {
		animatePropertyBy(constantName, getValue(constantName), byValue);
	}

	private void animatePropertyBy(int constantName, float startValue, float byValue) {
		// a property has one animation at a time: take it from the running one
		if (animatorMap.size() > 0) {
			Animator animatorToCancel = null;
			for (Animator runningAnim : animatorMap.keySet()) {
				PropertyBundle bundle = animatorMap.get(runningAnim);
				if (bundle.cancel(constantName)) {
					if (bundle.propertyMask == 0)
						animatorToCancel = runningAnim;
					break;
				}
			}
			if (animatorToCancel != null)
				animatorToCancel.cancel();
		}
		pendingAnimations.add(new NameValuesHolder(constantName, startValue, byValue));
		view.removeCallbacks(animationStarter);
		view.postOnAnimation(animationStarter);
	}

	private void setValue(int propertyConstant, float value) {
		switch (propertyConstant) {
			case TRANSLATION_X: view.setTranslationX(value); break;
			case TRANSLATION_Y: view.setTranslationY(value); break;
			case TRANSLATION_Z: view.setTranslationZ(value); break;
			case ROTATION: view.setRotation(value); break;
			case ROTATION_X: view.setRotationX(value); break;
			case ROTATION_Y: view.setRotationY(value); break;
			case SCALE_X: view.setScaleX(value); break;
			case SCALE_Y: view.setScaleY(value); break;
			case X: view.setX(value); break;
			case Y: view.setY(value); break;
			case Z: view.setZ(value); break;
			case ALPHA: view.setAlpha(value); break;
		}
	}

	private float getValue(int propertyConstant) {
		switch (propertyConstant) {
			case TRANSLATION_X: return view.getTranslationX();
			case TRANSLATION_Y: return view.getTranslationY();
			case TRANSLATION_Z: return view.getTranslationZ();
			case ROTATION: return view.getRotation();
			case ROTATION_X: return view.getRotationX();
			case ROTATION_Y: return view.getRotationY();
			case SCALE_X: return view.getScaleX();
			case SCALE_Y: return view.getScaleY();
			case X: return view.getX();
			case Y: return view.getY();
			case Z: return view.getZ();
			case ALPHA: return view.getAlpha();
		}
		return 0;
	}

	private class AnimatorEventListener implements Animator.AnimatorListener, ValueAnimator.AnimatorUpdateListener {
		@Override
		public void onAnimationStart(Animator animation) {
			Runnable r = animatorOnStartMap.remove(animation);
			if (r != null)
				r.run();
			if (listener != null)
				listener.onAnimationStart(animation);
		}

		@Override
		public void onAnimationCancel(Animator animation) {
			if (listener != null)
				listener.onAnimationCancel(animation);
			// a cancelled animation does not run its end action
			animatorOnEndMap.remove(animation);
		}

		@Override
		public void onAnimationRepeat(Animator animation) {
			if (listener != null)
				listener.onAnimationRepeat(animation);
		}

		@Override
		public void onAnimationEnd(Animator animation) {
			if (listener != null)
				listener.onAnimationEnd(animation);
			Runnable r = animatorOnEndMap.remove(animation);
			if (r != null)
				r.run();
			animatorMap.remove(animation);
			animatorOnStartMap.remove(animation);
		}

		@Override
		public void onAnimationUpdate(ValueAnimator animation) {
			PropertyBundle propertyBundle = animatorMap.get(animation);
			if (propertyBundle == null)
				return;
			float fraction = animation.getAnimatedFraction();
			ArrayList<NameValuesHolder> valueList = propertyBundle.nameValuesHolder;
			if (valueList != null) {
				for (NameValuesHolder values : valueList)
					setValue(values.nameConstant, values.fromValue + fraction * values.deltaValue);
			}
			if (updateListener != null)
				updateListener.onAnimationUpdate(animation);
		}
	}
}
