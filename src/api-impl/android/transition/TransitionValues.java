package android.transition;

import java.util.HashMap;
import java.util.Map;

import android.view.View;

public class TransitionValues {

	public View view;
	public final Map<String, Object> values = new HashMap<>();

	public TransitionValues() {}

	public TransitionValues(View view) {
		this.view = view;
	}
}
