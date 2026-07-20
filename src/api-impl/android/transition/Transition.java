package android.transition;

import java.util.List;

import android.animation.Animator;
import android.animation.TimeInterpolator;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

// ATL does not actually run transitions. These are enough for apps that build
// transition graphs and hand them to TransitionManager: animation-producing
// methods return null, setters record nothing and return the builder.
public class Transition {

	public interface TransitionListener {}

	public static abstract class EpicenterCallback {
		public abstract Rect onGetEpicenter(Transition transition);
	}

	public Transition() {}

	public Transition clone() {
		return new Transition();
	}

	public Transition addListener(TransitionListener listener) {
		return this;
	}

	public Transition removeListener(TransitionListener listener) {
		return this;
	}

	public Transition addTarget(View target) {
		return this;
	}

	public Transition removeTarget(View target) {
		return this;
	}

	public Transition excludeTarget(int targetId, boolean exclude) {
		return this;
	}

	public Transition excludeChildren(Class type, boolean exclude) {
		return this;
	}

	public List getTargetIds() {
		return null;
	}

	public List getTargets() {
		return null;
	}

	public List getTargetNames() {
		return null;
	}

	public List getTargetTypes() {
		return null;
	}

	public Transition setDuration(long duration) {
		return this;
	}

	public Transition setInterpolator(TimeInterpolator interpolator) {
		return this;
	}

	public void setEpicenterCallback(EpicenterCallback callback) {}

	public void captureStartValues(TransitionValues transitionValues) {}

	public void captureEndValues(TransitionValues transitionValues) {}

	public Animator createAnimator(ViewGroup sceneRoot, TransitionValues startValues, TransitionValues endValues) {
		return null;
	}
}
