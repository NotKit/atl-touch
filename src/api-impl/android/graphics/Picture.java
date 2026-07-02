package android.graphics;

/**
 * Bitmap-backed Picture: records into an offscreen bitmap instead of a
 * display list. Good enough for the common record-once/draw-few case.
 */
public class Picture {

	private Bitmap bitmap;
	private Canvas recordingCanvas;
	private int width;
	private int height;

	public Canvas beginRecording(int width, int height) {
		this.width = width;
		this.height = height;
		bitmap = Bitmap.createBitmap(Math.max(width, 1), Math.max(height, 1), Bitmap.Config.ARGB_8888);
		recordingCanvas = new Canvas(bitmap);
		return recordingCanvas;
	}

	public void endRecording() {
		recordingCanvas = null;
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	public boolean requiresHardwareAcceleration() {
		return false;
	}

	public void draw(Canvas canvas) {
		if (bitmap != null)
			canvas.drawBitmap(bitmap, 0, 0, null);
	}
}
