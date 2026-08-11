package android.widget;

import android.content.Context;
import android.util.AttributeSet;

public class EdgeEffect {

	public EdgeEffect(Context context) {}

	public EdgeEffect(Context context, AttributeSet attributeSet) {}

	public void setSize(int width, int height) {}
	public void onPull(float deltaDistance) {}
	public void onPull(float deltaDistance, float displacement) {}
	public float onPullDistance(float deltaDistance, float displacement) { return 0; }
	public boolean isFinished() { return true; }
	public void onRelease() {}
	public void onAbsorb(int velocity) {}
	public int getColor() { return 0; }
	public void setColor(int color) {}
	public float getDistance() { return 0; }
	public void finish() {}

	public void setBlendMode(android.graphics.BlendMode a0) { }

	public boolean draw(android.graphics.Canvas a0) { return false; }
}
