package android.view;

import android.content.Context;

public class ScaleGestureDetector {

	public interface OnScaleGestureListener {
		boolean onScale(ScaleGestureDetector detector);
		boolean onScaleBegin(ScaleGestureDetector detector);
		void onScaleEnd(ScaleGestureDetector detector);
	}

	public ScaleGestureDetector(Context context, OnScaleGestureListener listener) {}
	public ScaleGestureDetector(Context context, OnScaleGestureListener listener, android.os.Handler handler) {}

	public void setQuickScaleEnabled(boolean enabled) {}
	public void setStylusScaleEnabled(boolean enabled) {}

	public boolean onTouchEvent(MotionEvent event) {
		return false;
	}

	public boolean isInProgress() {
		return false;
	}

	/* onTouchEvent never starts a gesture, so the scale never leaves 1 and
	   the focus point stays at the origin */
	public float getScaleFactor() {
		return 1.0f;
	}

	public float getFocusX() {
		return 0.0f;
	}

	public float getFocusY() {
		return 0.0f;
	}

	public static class SimpleOnScaleGestureListener implements OnScaleGestureListener {
		public SimpleOnScaleGestureListener() {
		}

		public boolean onScale(ScaleGestureDetector detector) {
			return false;
		}
		public boolean onScaleBegin(ScaleGestureDetector detector) {
			return true;
		}
		public void onScaleEnd(ScaleGestureDetector detector) {
		}
	}
}
