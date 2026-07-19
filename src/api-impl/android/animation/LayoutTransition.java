package android.animation;

import android.view.View;
import android.view.ViewGroup;

import java.util.ArrayList;

public class LayoutTransition {

	public static final int CHANGE_APPEARING = 0;
	public static final int CHANGE_DISAPPEARING = 1;
	public static final int APPEARING = 2;
	public static final int DISAPPEARING = 3;
	public static final int CHANGING = 4;

	public interface TransitionListener {
		void startTransition(LayoutTransition transition, ViewGroup container, View view, int transitionType);
		void endTransition(LayoutTransition transition, ViewGroup container, View view, int transitionType);
	}

	private ArrayList<TransitionListener> transitionListeners;

	public void enableTransitionType(int transitionType) {}

	public void disableTransitionType(int transitionType) {}

	public void setStartDelay(int transitionType, long startDelay) {}

	public void setAnimator(int transitionType, Animator animator) {}

	public void setDuration(long duration) {}

	public Animator getAnimator(int transitionType) { return null; }

	public void setDuration(int transitionType, long duration) {}

	public void setInterpolator(int transitionType, TimeInterpolator interpolator) {}

	public void setAnimateParentHierarchy(boolean animateParentHierarchy) {}

	public void addTransitionListener(TransitionListener listener) {
		if (transitionListeners == null)
			transitionListeners = new ArrayList<TransitionListener>();
		transitionListeners.add(listener);
	}

	public void removeTransitionListener(TransitionListener listener) {
		if (transitionListeners != null)
			transitionListeners.remove(listener);
	}
}
