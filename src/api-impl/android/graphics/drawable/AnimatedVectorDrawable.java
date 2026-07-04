/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.graphics.drawable;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorInflater;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.Resources.Theme;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Rect;
import android.util.ArrayMap;
import android.util.AttributeSet;
import android.util.Log;

import com.android.internal.R;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.ArrayList;

/**
 * This class animates properties of a {@link android.graphics.drawable.VectorDrawable} with
 * animations defined using {@link android.animation.ObjectAnimator} or
 * {@link android.animation.AnimatorSet}.
 * <p>
 * AOSP 13 source, reduced to the UI-thread animator path: the RenderThread
 * implementation (VectorDrawableAnimatorRT) depends on hwui RenderNodes, which
 * do not exist here; VectorDrawableAnimatorUI drives a plain AnimatorSet whose
 * ObjectAnimators set properties on the (pure-Java) VectorDrawable targets via
 * reflection, invalidating this drawable every frame.
 */
public class AnimatedVectorDrawable extends Drawable implements Animatable2 {
	private static final String LOGTAG = "AnimatedVectorDrawable";

	private static final String ANIMATED_VECTOR = "animated-vector";
	private static final String TARGET = "target";

	private static final boolean DBG_ANIMATION_VECTOR_DRAWABLE = false;

	/** Local, mutable animator set. */
	private VectorDrawableAnimator mAnimatorSet;

	/**
	 * The resources against which this drawable was created. Used to attempt
	 * to inflate animators if applyTheme() doesn't get called.
	 */
	private Resources mRes;

	private AnimatedVectorDrawableState mAnimatedVectorState;

	/** The animator set that is parsed from the xml. */
	private AnimatorSet mAnimatorSetFromXml = null;

	private boolean mMutated;

	/** Use a internal AnimatorListener to support callbacks during animation events. */
	private ArrayList<Animatable2.AnimationCallback> mAnimationCallbacks = null;
	private AnimatorListener mAnimatorListener = null;

	public AnimatedVectorDrawable() {
		this(null, null);
	}

	private AnimatedVectorDrawable(AnimatedVectorDrawableState state, Resources res) {
		mAnimatedVectorState = new AnimatedVectorDrawableState(state, mCallback, res);
		mAnimatorSet = new VectorDrawableAnimatorUI(this);
		mRes = res;
	}

	@Override
	public Drawable mutate() {
		if (!mMutated && super.mutate() == this) {
			mAnimatedVectorState = new AnimatedVectorDrawableState(
			    mAnimatedVectorState, mCallback, mRes);
			mMutated = true;
		}
		return this;
	}

	/**
	 * @hide
	 */
	public void clearMutated() {
		if (mAnimatedVectorState.mVectorDrawable != null) {
			mAnimatedVectorState.mVectorDrawable.clearMutated();
		}
		mMutated = false;
	}

	@Override
	public ConstantState getConstantState() {
		mAnimatedVectorState.mChangingConfigurations = getChangingConfigurations();
		return mAnimatedVectorState;
	}

	@Override
	public int getChangingConfigurations() {
		return super.getChangingConfigurations() | mAnimatedVectorState.mChangingConfigurations;
	}

	@Override
	public void draw(Canvas canvas) {
		mAnimatorSet.onDraw(canvas);
		mAnimatedVectorState.mVectorDrawable.draw(canvas);
	}

	@Override
	protected void onBoundsChange(Rect bounds) {
		mAnimatedVectorState.mVectorDrawable.setBounds(bounds);
	}

	@Override
	protected boolean onStateChange(int[] state) {
		return mAnimatedVectorState.mVectorDrawable.setState(state);
	}

	@Override
	public void setAlpha(int alpha) {
		mAnimatedVectorState.mVectorDrawable.setAlpha(alpha);
	}

	@Override
	public void setColorFilter(ColorFilter colorFilter) {
		mAnimatedVectorState.mVectorDrawable.setColorFilter(colorFilter);
	}

	@Override
	public void setTintList(ColorStateList tint) {
		mAnimatedVectorState.mVectorDrawable.setTintList(tint);
	}

	@Override
	public boolean setVisible(boolean visible, boolean restart) {
		if (mAnimatorSet.isInfinite() && mAnimatorSet.isStarted()) {
			if (visible) {
				// Resume the infinite animation when the drawable becomes visible again.
				mAnimatorSet.resume();
			} else {
				// Pause the infinite animation once the drawable is no longer visible.
				mAnimatorSet.pause();
			}
		}
		mAnimatedVectorState.mVectorDrawable.setVisible(visible, restart);
		return super.setVisible(visible, restart);
	}

	@Override
	public boolean isStateful() {
		return mAnimatedVectorState.mVectorDrawable.isStateful();
	}

	@Override
	public int getOpacity() {
		return mAnimatedVectorState.mVectorDrawable.getOpacity();
	}

	@Override
	public int getIntrinsicWidth() {
		return mAnimatedVectorState.mVectorDrawable.getIntrinsicWidth();
	}

	@Override
	public int getIntrinsicHeight() {
		return mAnimatedVectorState.mVectorDrawable.getIntrinsicHeight();
	}

	public void inflate(Resources res, XmlPullParser parser, AttributeSet attrs, Theme theme)
	    throws XmlPullParserException, IOException {
		final AnimatedVectorDrawableState state = mAnimatedVectorState;

		int eventType = parser.getEventType();
		float pathErrorScale = 1;
		final int innerDepth = parser.getDepth() + 1;

		// Parse everything until the end of the animated-vector element.
		while (eventType != XmlPullParser.END_DOCUMENT
		       && (parser.getDepth() >= innerDepth || eventType != XmlPullParser.END_TAG)) {
			if (eventType == XmlPullParser.START_TAG) {
				final String tagName = parser.getName();
				if (ANIMATED_VECTOR.equals(tagName)) {
					final TypedArray a = res.obtainAttributes(attrs,
					                                          R.styleable.AnimatedVectorDrawable);
					int drawableRes = a.getResourceId(
					    R.styleable.AnimatedVectorDrawable_drawable, 0);
					if (drawableRes != 0) {
						VectorDrawable vectorDrawable = (VectorDrawable) res.getDrawable(
						    drawableRes, theme).mutate();
						vectorDrawable.setAllowCaching(false);
						vectorDrawable.setCallback(mCallback);
						pathErrorScale = vectorDrawable.getPixelSize();
						if (state.mVectorDrawable != null) {
							state.mVectorDrawable.setCallback(null);
						}
						state.mVectorDrawable = vectorDrawable;
					}
					a.recycle();
				} else if (TARGET.equals(tagName)) {
					final TypedArray a = res.obtainAttributes(attrs,
					                                          R.styleable.AnimatedVectorDrawableTarget);
					final String target = a.getString(
					    R.styleable.AnimatedVectorDrawableTarget_name);
					final int animResId = a.getResourceId(
					    R.styleable.AnimatedVectorDrawableTarget_animation, 0);
					if (animResId != 0) {
						if (theme != null) {
							// The animator here could be ObjectAnimator or AnimatorSet.
							final Animator animator = AnimatorInflater.loadAnimator(
							    res, theme, animResId, pathErrorScale);
							state.addTargetAnimator(target, animator);
						} else {
							// The animation may be theme-dependent. As a
							// workaround until Animator has full support for
							// applyTheme(), postpone loading the animator
							// until we have a theme in applyTheme().
							state.addPendingAnimator(animResId, pathErrorScale, target);
						}
					}
					a.recycle();
				}
			}

			eventType = parser.next();
		}

		// If we don't have any pending animations, we don't need to hold a
		// reference to the resources.
		mRes = state.mPendingAnims == null ? null : res;
	}

	/**
	 * Gets the total duration of the animation
	 * @hide
	 */
	public long getTotalDuration() {
		return mAnimatorSet.getTotalDuration();
	}

	private static class AnimatedVectorDrawableState extends ConstantState {
		int mChangingConfigurations;
		VectorDrawable mVectorDrawable;

		/** Animators that require a theme before inflation. */
		ArrayList<PendingAnimator> mPendingAnims;

		/** Fully inflated animators awaiting cloning into an AnimatorSet. */
		ArrayList<Animator> mAnimators;

		/** Map of animators to their target object names */
		ArrayMap<Animator, String> mTargetNameMap;

		public AnimatedVectorDrawableState(AnimatedVectorDrawableState copy,
		                                   Callback owner, Resources res) {
			if (copy != null) {
				mChangingConfigurations = copy.mChangingConfigurations;

				if (copy.mVectorDrawable != null) {
					final ConstantState cs = copy.mVectorDrawable.getConstantState();
					if (res != null) {
						mVectorDrawable = (VectorDrawable) cs.newDrawable(res);
					} else {
						mVectorDrawable = (VectorDrawable) cs.newDrawable();
					}
					mVectorDrawable = (VectorDrawable) mVectorDrawable.mutate();
					mVectorDrawable.setCallback(owner);
					mVectorDrawable.setBounds(copy.mVectorDrawable.getBounds());
					mVectorDrawable.setAllowCaching(false);
				}

				if (copy.mAnimators != null) {
					mAnimators = new ArrayList<>(copy.mAnimators);
				}

				if (copy.mTargetNameMap != null) {
					mTargetNameMap = new ArrayMap<>(copy.mTargetNameMap);
				}

				if (copy.mPendingAnims != null) {
					mPendingAnims = new ArrayList<>(copy.mPendingAnims);
				}
			} else {
				mVectorDrawable = new VectorDrawable();
			}
		}

		@Override
		public Drawable newDrawable() {
			return new AnimatedVectorDrawable(this, null);
		}

		@Override
		public Drawable newDrawable(Resources res) {
			return new AnimatedVectorDrawable(this, res);
		}

		@Override
		public int getChangingConfigurations() {
			return mChangingConfigurations;
		}

		public void addPendingAnimator(int resId, float pathErrorScale, String target) {
			if (mPendingAnims == null) {
				mPendingAnims = new ArrayList<>(1);
			}
			mPendingAnims.add(new PendingAnimator(resId, pathErrorScale, target));
		}

		public void addTargetAnimator(String targetName, Animator animator) {
			if (mAnimators == null) {
				mAnimators = new ArrayList<>(1);
				mTargetNameMap = new ArrayMap<>(1);
			}
			mAnimators.add(animator);
			mTargetNameMap.put(animator, targetName);

			if (DBG_ANIMATION_VECTOR_DRAWABLE) {
				Log.v(LOGTAG, "add animator  for target " + targetName + " " + animator);
			}
		}

		/**
		 * Prepares a local set of mutable animators based on the constant
		 * state.
		 * <p>
		 * If there are any pending uninflated animators, attempts to inflate
		 * them immediately against the provided resources object.
		 *
		 * @param animatorSet the animator set to which the animators should
		 *                    be added
		 * @param res the resources against which to inflate any pending
		 *            animators, or {@code null} if not available
		 */
		public void prepareLocalAnimators(AnimatorSet animatorSet, Resources res) {
			// Check for uninflated animators. We can remove this after we add
			// support for Animator.applyTheme(). See comments in inflate().
			if (mPendingAnims != null) {
				// Attempt to load animators without applying a theme.
				if (res != null) {
					inflatePendingAnimators(res, null);
				} else {
					Log.e(LOGTAG, "Failed to load animators. Either the AnimatedVectorDrawable"
					              + " must be created using a Resources object or applyTheme() must be"
					              + " called with a non-null Theme object.");
				}

				mPendingAnims = null;
			}

			// Perform a deep copy of the constant state's animators.
			final int count = mAnimators == null ? 0 : mAnimators.size();
			if (count > 0) {
				final Animator firstAnim = prepareLocalAnimator(0);
				final AnimatorSet.Builder builder = animatorSet.play(firstAnim);
				for (int i = 1; i < count; ++i) {
					final Animator nextAnim = prepareLocalAnimator(i);
					builder.with(nextAnim);
				}
			}
		}

		/**
		 * Prepares a local animator for the given index within the constant
		 * state's list of animators.
		 *
		 * @param index the index of the animator within the constant state
		 */
		private Animator prepareLocalAnimator(int index) {
			final Animator animator = mAnimators.get(index);
			final Animator localAnimator = animator.clone();
			final String targetName = mTargetNameMap.get(animator);
			final Object target = mVectorDrawable.getTargetByName(targetName);
			if (target == null) {
				throw new IllegalStateException("Target with the name \"" + targetName
				                                + "\" cannot be found in the VectorDrawable to be animated.");
			}
			localAnimator.setTarget(target);
			return localAnimator;
		}

		/**
		 * Inflates pending animators, if any, against a theme. Clears the list of
		 * pending animators.
		 *
		 * @param t the theme against which to inflate the animators
		 */
		public void inflatePendingAnimators(Resources res, Theme t) {
			final ArrayList<PendingAnimator> pendingAnims = mPendingAnims;
			if (pendingAnims != null) {
				mPendingAnims = null;

				for (int i = 0, count = pendingAnims.size(); i < count; i++) {
					final PendingAnimator pendingAnimator = pendingAnims.get(i);
					final Animator animator = pendingAnimator.newInstance(res, t);
					addTargetAnimator(pendingAnimator.target, animator);
				}
			}
		}

		/**
		 * Basically a constant state for Animators until we actually implement
		 * constant states for Animators.
		 */
		private static class PendingAnimator {
			public final int animResId;
			public final float pathErrorScale;
			public final String target;

			public PendingAnimator(int animResId, float pathErrorScale, String target) {
				this.animResId = animResId;
				this.pathErrorScale = pathErrorScale;
				this.target = target;
			}

			public Animator newInstance(Resources res, Theme theme) {
				return AnimatorInflater.loadAnimator(res, theme, animResId, pathErrorScale);
			}
		}
	}

	@Override
	public boolean isRunning() {
		return mAnimatorSet.isRunning();
	}

	/**
	 * Resets the AnimatedVectorDrawable to the start state as specified in the animators.
	 */
	public void reset() {
		ensureAnimatorSet();
		mAnimatorSet.reset();
	}

	@Override
	public void start() {
		ensureAnimatorSet();
		mAnimatorSet.start();
	}

	private void ensureAnimatorSet() {
		if (mAnimatorSetFromXml == null) {
			// TODO: Skip the AnimatorSet creation and init the VectorDrawableAnimator directly
			// with a list of LocalAnimators.
			mAnimatorSetFromXml = new AnimatorSet();
			mAnimatedVectorState.prepareLocalAnimators(mAnimatorSetFromXml, mRes);
			mAnimatorSet.init(mAnimatorSetFromXml);
			mRes = null;
		}
	}

	@Override
	public void stop() {
		mAnimatorSet.end();
	}

	/**
	 * Reverses ongoing animations or starts pending animations in reverse.
	 * <p>
	 * NOTE: Only works if all animations support reverse. Otherwise, this will
	 * do nothing.
	 * @hide
	 */
	public void reverse() {
		ensureAnimatorSet();

		// Only reverse when all the animators can be reversed.
		if (!canReverse()) {
			Log.w(LOGTAG, "AnimatedVectorDrawable can't reverse()");
			return;
		}

		mAnimatorSet.reverse();
	}

	/**
	 * @hide
	 */
	public boolean canReverse() {
		return mAnimatorSet.canReverse();
	}

	private final Callback mCallback = new Callback() {
		@Override
		public void invalidateDrawable(Drawable who) {
			invalidateSelf();
		}

		@Override
		public void scheduleDrawable(Drawable who, Runnable what, long when) {
			scheduleSelf(what, when);
		}

		@Override
		public void unscheduleDrawable(Drawable who, Runnable what) {
			unscheduleSelf(what);
		}
	};

	@Override
	public void registerAnimationCallback(AnimationCallback callback) {
		if (callback == null) {
			return;
		}

		// Add listener accordingly.
		if (mAnimationCallbacks == null) {
			mAnimationCallbacks = new ArrayList<>();
		}

		mAnimationCallbacks.add(callback);

		if (mAnimatorListener == null) {
			// Create a animator listener and trigger the callback events when listener is
			// triggered.
			mAnimatorListener = new AnimatorListenerAdapter() {
				@Override
				public void onAnimationStart(Animator animation) {
					ArrayList<AnimationCallback> tmpCallbacks = new ArrayList<>(mAnimationCallbacks);
					int size = tmpCallbacks.size();
					for (int i = 0; i < size; i++) {
						tmpCallbacks.get(i).onAnimationStart(AnimatedVectorDrawable.this);
					}
				}

				@Override
				public void onAnimationEnd(Animator animation) {
					ArrayList<AnimationCallback> tmpCallbacks = new ArrayList<>(mAnimationCallbacks);
					int size = tmpCallbacks.size();
					for (int i = 0; i < size; i++) {
						tmpCallbacks.get(i).onAnimationEnd(AnimatedVectorDrawable.this);
					}
				}
			};
		}
		mAnimatorSet.setListener(mAnimatorListener);
	}

	// A helper function to clean up the animator listener in the mAnimatorSet.
	private void removeAnimatorSetListener() {
		if (mAnimatorListener != null) {
			mAnimatorSet.removeListener(mAnimatorListener);
			mAnimatorListener = null;
		}
	}

	@Override
	public boolean unregisterAnimationCallback(AnimationCallback callback) {
		if (mAnimationCallbacks == null || callback == null) {
			// Nothing to be removed.
			return false;
		}
		boolean removed = mAnimationCallbacks.remove(callback);

		//  When the last call back unregistered, remove the listener accordingly.
		if (mAnimationCallbacks.size() == 0) {
			removeAnimatorSetListener();
		}
		return removed;
	}

	@Override
	public void clearAnimationCallbacks() {
		removeAnimatorSetListener();
		if (mAnimationCallbacks == null) {
			return;
		}

		mAnimationCallbacks.clear();
	}

	private interface VectorDrawableAnimator {
		void init(AnimatorSet set);
		void start();
		void end();
		void reset();
		void reverse();
		boolean canReverse();
		void setListener(AnimatorListener listener);
		void removeListener(AnimatorListener listener);
		void onDraw(Canvas canvas);
		boolean isStarted();
		boolean isRunning();
		boolean isInfinite();
		void pause();
		void resume();
		long getTotalDuration();
	}

	private static class VectorDrawableAnimatorUI implements VectorDrawableAnimator {
		// mSet is only initialized in init(). So we need to check whether it is null before any
		// operation.
		private AnimatorSet mSet = null;
		private final Drawable mDrawable;
		// Caching the listener in the case when listener operation is called before the mSet is
		// setup by init().
		private ArrayList<AnimatorListener> mListenerArray = null;
		private boolean mIsInfinite = false;
		private long mTotalDuration;

		VectorDrawableAnimatorUI(AnimatedVectorDrawable drawable) {
			mDrawable = drawable;
		}

		@Override
		public void init(AnimatorSet set) {
			if (mSet != null) {
				// Already initialized
				throw new UnsupportedOperationException("VectorDrawableAnimator cannot be " +
				                                        "re-initialized");
			}
			// Keep a deep copy of the set, such that set can be still be constantly representing
			// the static content from XML file.
			mSet = set.clone();
			mTotalDuration = mSet.getTotalDuration();
			mIsInfinite = mTotalDuration == Animator.DURATION_INFINITE;

			// If there are listeners added before calling init(), now they should be setup.
			if (mListenerArray != null && !mListenerArray.isEmpty()) {
				for (int i = 0; i < mListenerArray.size(); i++) {
					mSet.addListener(mListenerArray.get(i));
				}
				mListenerArray.clear();
				mListenerArray = null;
			}
		}

		// Although start(), reset() and reverse() should call init() already, it is better to
		// protect these functions from NPE in any situation.
		@Override
		public void start() {
			if (mSet == null || mSet.isStarted()) {
				return;
			}
			mSet.start();
			invalidateOwningView();
		}

		@Override
		public void end() {
			if (mSet == null) {
				return;
			}
			mSet.end();
		}

		@Override
		public void reset() {
			if (mSet == null) {
				return;
			}
			start();
			mSet.cancel();
		}

		@Override
		public void reverse() {
			if (mSet == null) {
				return;
			}
			mSet.reverse();
			invalidateOwningView();
		}

		@Override
		public boolean canReverse() {
			return mSet != null && mSet.canReverse();
		}

		@Override
		public void setListener(AnimatorListener listener) {
			if (mSet == null) {
				if (mListenerArray == null) {
					mListenerArray = new ArrayList<AnimatorListener>();
				}
				mListenerArray.add(listener);
			} else {
				mSet.addListener(listener);
			}
		}

		@Override
		public void removeListener(AnimatorListener listener) {
			if (mSet == null) {
				if (mListenerArray == null) {
					return;
				}
				mListenerArray.remove(listener);
			} else {
				mSet.removeListener(listener);
			}
		}

		@Override
		public void onDraw(Canvas canvas) {
			if (mSet != null && mSet.isStarted()) {
				invalidateOwningView();
			}
		}

		@Override
		public boolean isStarted() {
			return mSet != null && mSet.isStarted();
		}

		@Override
		public boolean isRunning() {
			return mSet != null && mSet.isRunning();
		}

		@Override
		public boolean isInfinite() {
			return mIsInfinite;
		}

		@Override
		public void pause() {
			if (mSet == null) {
				return;
			}
			mSet.pause();
		}

		@Override
		public void resume() {
			if (mSet == null) {
				return;
			}
			mSet.resume();
		}

		private void invalidateOwningView() {
			mDrawable.invalidateSelf();
		}

		@Override
		public long getTotalDuration() {
			return mTotalDuration;
		}
	}
}
