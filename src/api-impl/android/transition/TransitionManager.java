package android.transition;

import android.view.ViewGroup;

// No-op: ATL applies layout changes directly, transitions just do not animate.
public class TransitionManager {

	public static void beginDelayedTransition(ViewGroup sceneRoot) {}

	public static void beginDelayedTransition(ViewGroup sceneRoot, Transition transition) {}

	public static void endTransitions(ViewGroup sceneRoot) {}
}
