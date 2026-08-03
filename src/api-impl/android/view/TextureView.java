package android.view;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.util.AttributeSet;

/**
 * A view backed by a SurfaceTexture. There is no GL layer here: frames are
 * taken from the SurfaceTexture as bitmaps and drawn into the Skia scene, the
 * same compositing SurfaceView does for camera and video frames.
 */
public class TextureView extends View {

	private SurfaceTextureListener surfaceTextureListener;
	private SurfaceTexture surfaceTexture;
	private boolean available;

	public TextureView(Context context) {
		super(context);
	}

	public TextureView(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	public interface SurfaceTextureListener {
		void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height);
		void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height);
		/** returning true hands the SurfaceTexture back to the view to release */
		boolean onSurfaceTextureDestroyed(SurfaceTexture surface);
		void onSurfaceTextureUpdated(SurfaceTexture surface);
	}

	public void setSurfaceTextureListener(SurfaceTextureListener surfaceTextureListener) {
		this.surfaceTextureListener = surfaceTextureListener;
		if (available && surfaceTextureListener != null)
			surfaceTextureListener.onSurfaceTextureAvailable(surfaceTexture, getWidth(), getHeight());
	}

	public void setOpaque(boolean opaque) {}

	public SurfaceTextureListener getSurfaceTextureListener() {
		return surfaceTextureListener;
	}

	public boolean isAvailable() {
		return available;
	}

	public SurfaceTexture getSurfaceTexture() {
		return surfaceTexture;
	}

	public void setSurfaceTexture(SurfaceTexture surfaceTexture) {
		if (this.surfaceTexture != null)
			this.surfaceTexture.release();
		this.surfaceTexture = surfaceTexture;
		if (surfaceTexture != null) {
			listenForFrames();
			available = true;
		}
	}

	/** The newest frame, or null before the first one arrives. */
	public Bitmap getBitmap() {
		return surfaceTexture != null ? surfaceTexture.getLatestFrameBitmap() : null;
	}

	@Override
	protected void onAttachedToWindow() {
		super.onAttachedToWindow();
		post(new Runnable() {
			@Override
			public void run() {
				createSurfaceTexture();
			}
		});
	}

	@Override
	protected void onDetachedFromWindow() {
		super.onDetachedFromWindow();
		if (surfaceTexture == null)
			return;

		boolean release = true;
		if (surfaceTextureListener != null)
			release = surfaceTextureListener.onSurfaceTextureDestroyed(surfaceTexture);
		if (release)
			surfaceTexture.release();
		surfaceTexture = null;
		available = false;
	}

	@Override
	protected void onSizeChanged(int w, int h, int oldw, int oldh) {
		super.onSizeChanged(w, h, oldw, oldh);
		if (surfaceTexture == null || w <= 0 || h <= 0)
			return;
		surfaceTexture.setDefaultBufferSize(w, h);
		if (surfaceTextureListener != null)
			surfaceTextureListener.onSurfaceTextureSizeChanged(surfaceTexture, w, h);
	}

	@Override
	public void onDraw(Canvas canvas) {
		Bitmap frame = surfaceTexture != null ? surfaceTexture.getLatestFrameBitmap() : null;
		if (frame != null)
			canvas.drawBitmap(frame, new Rect(0, 0, frame.getWidth(), frame.getHeight()),
			                  new Rect(0, 0, getWidth(), getHeight()), null);
	}

	private void createSurfaceTexture() {
		if (surfaceTexture != null)
			return;

		/* detached: nothing here samples it from GL, frames are drawn as bitmaps */
		surfaceTexture = new SurfaceTexture(false);
		if (getWidth() > 0 && getHeight() > 0)
			surfaceTexture.setDefaultBufferSize(getWidth(), getHeight());
		listenForFrames();
		available = true;
		if (surfaceTextureListener != null)
			surfaceTextureListener.onSurfaceTextureAvailable(surfaceTexture, getWidth(), getHeight());
	}

	/*
	 * AOSP fires onSurfaceTextureUpdated after the app's updateTexImage(); here
	 * the frame is taken at draw time, so it fires as the frame arrives.
	 */
	private void listenForFrames() {
		surfaceTexture.setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
			@Override
			public void onFrameAvailable(SurfaceTexture surface) {
				postInvalidate();
				if (surfaceTextureListener != null)
					surfaceTextureListener.onSurfaceTextureUpdated(surface);
			}
		});
	}
}
