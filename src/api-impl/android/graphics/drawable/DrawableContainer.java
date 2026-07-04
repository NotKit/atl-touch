package android.graphics.drawable;

import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Rect;

public class DrawableContainer extends Drawable implements Drawable.Callback {

	private DrawableContainerState state;
	private int curIndex = -1;

	protected native long native_constructor();
	protected native void native_selectChild(long container, long child);

	public DrawableContainer() {
		setPaintable(native_constructor());
	}

	public boolean selectDrawable(int idx) {
		if (idx >= 0 && idx < state.childCount && idx != curIndex && state.drawables[idx] != null) {
			curIndex = idx;
			final Drawable child = state.drawables[idx];
			// The newly selected child may never have seen the container's
			// bounds (they are only forwarded to the current child).
			final Rect bounds = getBounds();
			child.setBounds(bounds.left, bounds.top, bounds.right, bounds.bottom);
			native_selectChild(paintable, child.paintable);
			invalidateSelf();
			return true;
		}
		return false;
	}

	/* --- Drawable.Callback: forward child invalidates (e.g. an animating
	 * transition of an AnimatedStateListDrawable) up to the view --- */

	@Override
	public void invalidateDrawable(Drawable who) {
		if (who == getCurrent())
			invalidateSelf();
	}

	@Override
	public void scheduleDrawable(Drawable who, Runnable what, long when) {
		if (who == getCurrent()) {
			final Callback callback = getCallback();
			if (callback != null)
				callback.scheduleDrawable(this, what, when);
		}
	}

	@Override
	public void unscheduleDrawable(Drawable who, Runnable what) {
		final Callback callback = getCallback();
		if (callback != null)
			callback.unscheduleDrawable(this, what);
	}

	/** index of the currently selected child, or -1 */
	public int getCurrentIndex() {
		return curIndex;
	}

	@Override
	public Drawable getCurrent() {
		return curIndex != -1 ? state.drawables[curIndex] : null;
	}

	protected void setConstantState(DrawableContainerState state) {
		this.state = state;
	}

	@Override
	public ConstantState getConstantState() {
		return state;
	}

	public static class DrawableContainerState extends ConstantState {

		private Drawable drawables[] = new Drawable[10];
		private int childCount = 0;
		private DrawableContainer owner;

		public DrawableContainerState(DrawableContainerState orig, DrawableContainer owner, Resources res) {
			this.owner = owner;
		}

		public int getCapacity() {
			return drawables.length;
		}

		public int getChildCount() {
			return childCount;
		}

		public Drawable[] getChildren() {
			return drawables;
		}

		public Drawable getChild(int idx) {
			return drawables[idx];
		}

		public int addChild(Drawable dr) {
			if (childCount >= drawables.length) {
				growArray(drawables.length, drawables.length * 2);
			}
			if (dr != null)
				dr.setCallback(owner);
			drawables[childCount] = dr;
			return childCount++;
		}

		public void growArray(int oldSize, int newSize) {
			Drawable[] newDrawables = new Drawable[newSize];
			System.arraycopy(drawables, 0, newDrawables, 0, oldSize);
			drawables = newDrawables;
		}

		@Override
		public Drawable newDrawable(Resources res) {
			return owner;
		}

		@Override
		public Drawable newDrawable() {
			return owner;
		}

		@Override
		public int getChangingConfigurations() {
			return owner.getChangingConfigurations();
		}
	}

	@Override
	public void draw(Canvas canvas) {
		if (curIndex != -1)
			state.drawables[curIndex].draw(canvas);
	}

	@Override
	public int getIntrinsicHeight() {
		return curIndex != -1 ? state.drawables[curIndex].getIntrinsicHeight() : -1;
	}

	@Override
	public int getIntrinsicWidth() {
		return curIndex != -1 ? state.drawables[curIndex].getIntrinsicWidth() : -1;
	}

	@Override
	public void setBounds(int left, int top, int right, int bottom) {
		super.setBounds(left, top, right, bottom); // remember for later-selected children
		if (curIndex != -1)
			state.drawables[curIndex].setBounds(left, top, right, bottom);
	}

	public void setEnterFadeDuration(int duration) {}

	public void setExitFadeDuration(int duration) {}
}
