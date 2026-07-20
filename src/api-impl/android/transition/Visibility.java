package android.transition;

import android.animation.Animator;
import android.view.View;
import android.view.ViewGroup;

public class Visibility extends Transition {

	public Visibility() {}

	public Animator onAppear(ViewGroup sceneRoot, View view, TransitionValues startValues, TransitionValues endValues) {
		return null;
	}

	public Animator onDisappear(ViewGroup sceneRoot, View view, TransitionValues startValues, TransitionValues endValues) {
		return null;
	}
}
