package android.transition;

import java.util.ArrayList;

import android.animation.TimeInterpolator;
import android.view.View;

public class TransitionSet extends Transition {

	public static final int ORDERING_TOGETHER = 0;
	public static final int ORDERING_SEQUENTIAL = 1;

	private final ArrayList<Transition> transitions = new ArrayList<>();
	private int ordering = ORDERING_TOGETHER;

	public TransitionSet() {}

	public TransitionSet setOrdering(int ordering) {
		this.ordering = ordering;
		return this;
	}

	public int getOrdering() {
		return ordering;
	}

	public TransitionSet addTransition(Transition transition) {
		if (transition != null) {
			transitions.add(transition);
		}
		return this;
	}

	public TransitionSet removeTransition(Transition transition) {
		transitions.remove(transition);
		return this;
	}

	public int getTransitionCount() {
		return transitions.size();
	}

	public Transition getTransitionAt(int index) {
		if (index < 0 || index >= transitions.size()) {
			return null;
		}
		return transitions.get(index);
	}

	@Override
	public TransitionSet setDuration(long duration) {
		return this;
	}

	@Override
	public TransitionSet setInterpolator(TimeInterpolator interpolator) {
		return this;
	}

	@Override
	public TransitionSet addTarget(View target) {
		return this;
	}

	@Override
	public TransitionSet addListener(TransitionListener listener) {
		return this;
	}
}
