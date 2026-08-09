package android.view;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.AttributeSet;
import java.util.ArrayList;

public class SurfaceView extends View {

	final ArrayList<SurfaceHolder.Callback> mCallbacks = new ArrayList<SurfaceHolder.Callback>();

	/* the wl_subsurface this view's content presents through, if the platform
	 * has one. While it is live the view draws a transparent hole instead of
	 * pixels, and the layer shows through from below - which is how anything
	 * ATL draws afterwards (toolbars, dialogs, panels) ends up on top. */
	private long mLayer;
	private boolean mLayerVisible = true;
	private boolean mZOrderOnTop;
	private boolean mFixedSize;
	private int mFixedWidth, mFixedHeight;
	private int mSurfaceWidth, mSurfaceHeight;
	private final int[] mLayerLocation = new int[2];
	private int mLayerX = -1, mLayerY = -1, mLayerW = -1, mLayerH = -1;
	private static int sLayersAvailable; // 0 unknown, 1 yes, -1 no

	public SurfaceView(Context context) {
		super(context);

		mSurface.widget = this.widget;
		mSurface.view = this;
	}

	public SurfaceView(Context context, AttributeSet attrs) {
		super(context, attrs);

		mSurface.widget = this.widget;
		mSurface.view = this;
	}

	public SurfaceView(Context context, AttributeSet attrs, int defStyleAttr) {
		super(context, attrs, defStyleAttr);

		mSurface.widget = this.widget;
		mSurface.view = this;
	}

	private void surfaceChanged(int format, int width, int height) {
		for (SurfaceHolder.Callback c : mCallbacks) {
			c.surfaceChanged(mSurfaceHolder, format, width, height);
		}
	}

	private void surfaceCreated() {
		for (SurfaceHolder.Callback c : mCallbacks) {
			c.surfaceCreated(mSurfaceHolder);
		}
	}

	/* the latest frame posted through the surface, drawn into the scene */
	private android.graphics.Bitmap frontBuffer;
	private boolean reportedCreated = false;

	@Override
	protected void onAttachedToWindow() {
		super.onAttachedToWindow();
		post(new Runnable() {
			@Override
			public void run() {
				updateLayer(); // holder.getSurface() must be backed before the callbacks
				if (!reportedCreated) {
					reportedCreated = true;
					surfaceCreated();
					if (getWidth() > 0 && getHeight() > 0)
						surfaceChanged(1 /*RGBA_8888*/, getWidth(), getHeight());
				}
			}
		});
	}

	@Override
	protected void onSizeChanged(int w, int h, int oldw, int oldh) {
		super.onSizeChanged(w, h, oldw, oldh);
		updateLayer();
		if (reportedCreated && w > 0 && h > 0)
			surfaceChanged(1 /*RGBA_8888*/, w, h);
	}

	private final Rect frameSrc = new Rect();
	private final Rect frameDst = new Rect();

	/*
	 * draw(), not onDraw(): onDraw is recorded into the view's display list and
	 * replayed, so a layout or scroll that moves the view without changing a
	 * pixel of it would never re-run it - and both the hole and the layer's
	 * position have to follow the view every frame.
	 *
	 * On AOSP the posted frames live in a separate compositor layer below the
	 * view, so a subclass drawing its own content in onDraw() (Open Camera puts
	 * its whole HUD there) ends up on top of them. Blitting here rather than in
	 * onDraw() reproduces that order, and keeps working when the subclass does
	 * not chain to super.onDraw().
	 */
	@Override
	public void draw(android.graphics.Canvas canvas) {
		updateLayer();
		android.graphics.Bitmap frame = frontBuffer;
		if (frame != null) {
			/* a CPU producer (camera preview, MediaCodec) posts into this view.
			 * Nothing is presenting into the layer then, so a hole would show
			 * the desktop instead of the frame: unmap the layer and blit. */
			if (mLayerVisible) {
				mLayerVisible = false;
				if (mLayer != 0)
					native_setLayerVisible(mLayer, false);
			}
			frameSrc.set(0, 0, frame.getWidth(), frame.getHeight());
			frameDst.set(0, 0, getWidth(), getHeight());
			canvas.drawBitmap(frame, frameSrc, frameDst, null);
		} else if (mLayer != 0 && !mZOrderOnTop) {
			/* the punch-hole: the scene reaches the toplevel's buffer unblended,
			 * so this really does write alpha 0 and let the layer through */
			int save = canvas.save();
			canvas.clipRect(0, 0, getWidth(), getHeight());
			canvas.drawColor(0, android.graphics.PorterDuff.Mode.CLEAR);
			canvas.restoreToCount(save);
		}
		super.draw(canvas);
	}

	/* --- the layer's lifecycle, mirroring AOSP's --- */

	private void updateLayer() {
		if (sLayersAvailable == 0)
			sLayersAvailable = native_layersAvailable() ? 1 : -1;
		if (sLayersAvailable < 0)
			return;
		ViewRootImpl root = getViewRootImpl();
		int width = getWidth(), height = getHeight();
		if (root == null || root.scene == 0 || width <= 0 || height <= 0)
			return;
		if (mLayer == 0) {
			mLayer = native_createLayer(root.scene);
			if (mLayer == 0) {
				sLayersAvailable = -1;
				return;
			}
			if (mZOrderOnTop)
				native_setLayerZ(mLayer, true);
			if (!mLayerVisible)
				native_setLayerVisible(mLayer, false);
		}
		locationInWindow(mLayerLocation);
		if (mLayerLocation[0] != mLayerX || mLayerLocation[1] != mLayerY ||
		    width != mLayerW || height != mLayerH) {
			mLayerX = mLayerLocation[0];
			mLayerY = mLayerLocation[1];
			mLayerW = width;
			mLayerH = height;
			native_setLayerGeometry(mLayer, mLayerX, mLayerY, width, height);
		}
		mSurfaceWidth = mFixedSize ? mFixedWidth : width;
		mSurfaceHeight = mFixedSize ? mFixedHeight : height;
		if (mSurface.nativeWindow == 0) {
			native_bindSurface(mSurface, mLayer, mSurfaceWidth, mSurfaceHeight);
			native_startTestClient(mSurface);
		}
	}

	/* View.getLocationInWindow() goes through getGlobalVisibleRect(), which is
	 * only right once everything above has been laid out; the layer's position
	 * has to be exact from the first frame, so walk the parents like AOSP does */
	private void locationInWindow(int[] out) {
		View v = this;
		int x = 0, y = 0;
		while (true) {
			x += v.getLeft();
			y += v.getTop();
			ViewParent parent = v.getParent();
			if (!(parent instanceof View))
				break;
			v = (View)parent;
			x -= v.getScrollX();
			y -= v.getScrollY();
		}
		out[0] = x;
		out[1] = y;
	}

	private void destroyLayer() {
		if (mLayer == 0)
			return;
		/* AOSP's contract: the app tears its EGLSurface down inside this call,
		 * so nothing is using the wl_egl_window by the time it is destroyed */
		for (SurfaceHolder.Callback c : mCallbacks)
			c.surfaceDestroyed(mSurfaceHolder);
		reportedCreated = false;
		native_destroyLayer(mLayer, mSurface);
		mLayer = 0;
		mLayerX = mLayerY = mLayerW = mLayerH = -1;
	}

	@Override
	protected void onDetachedFromWindow() {
		destroyLayer();
		super.onDetachedFromWindow();
	}

	int getSurfaceWidth() {
		return mSurfaceWidth > 0 ? mSurfaceWidth : getWidth();
	}

	int getSurfaceHeight() {
		return mSurfaceHeight > 0 ? mSurfaceHeight : getHeight();
	}

	void postFrame(android.graphics.Bitmap frame) {
		frontBuffer = frame;
		postInvalidate();
	}

	protected native long native_createSnapshot(int width, int height);
	private static native boolean native_layersAvailable();
	private native long native_createLayer(long windowPtr);
	private native void native_destroyLayer(long layer, Surface surface);
	private native void native_bindSurface(Surface surface, long layer, int width, int height);
	private native void native_setLayerGeometry(long layer, int x, int y, int width, int height);
	private native void native_setLayerBufferSize(long layer, int width, int height);
	private native void native_setLayerZ(long layer, boolean above);
	private native void native_setLayerVisible(long layer, boolean visible);
	/** debug: a colour-cycling GL client on the layer, only with ATL_SURFACE_TEST set */
	private native void native_startTestClient(Surface surface);
	/** detach the canvas's backing bitmap (frees the canvas) */
	protected static native long native_canvas_to_bitmap(long canvas);

	public SurfaceHolder getHolder() {
		return mSurfaceHolder;
	}

	final Surface mSurface = new Surface();

	private final SurfaceHolder mSurfaceHolder = new SurfaceHolder() {
		private static final String LOG_TAG = "SurfaceHolder";

		@Override
		public boolean isCreating() {
			//		return mIsCreating;
			return false;
		}

		@Override
		public void addCallback(Callback callback) {
			synchronized (mCallbacks) {
				if (mCallbacks.contains(callback) == false) {
					mCallbacks.add(callback);
				}
			}
		}

		@Override
		public void removeCallback(Callback callback) {
			/*		synchronized (mCallbacks) {
					mCallbacks.remove(callback);
					}*/
		}

		@Override
		public void setFixedSize(int width, int height) {
			if (mFixedSize && mFixedWidth == width && mFixedHeight == height)
				return;
			mFixedSize = width > 0 && height > 0;
			mFixedWidth = width;
			mFixedHeight = height;
			if (mLayer != 0) {
				native_setLayerBufferSize(mLayer, width, height);
				mSurfaceWidth = mFixedSize ? width : getWidth();
				mSurfaceHeight = mFixedSize ? height : getHeight();
				if (reportedCreated)
					surfaceChanged(1 /*RGBA_8888*/, mSurfaceWidth, mSurfaceHeight);
			}
		}

		@Override
		public void setSizeFromLayout() {
			/*		if (mRequestedWidth != -1 || mRequestedHeight != -1) {
					mRequestedWidth = mRequestedHeight = -1;
					requestLayout();
					}*/
		}

		@Override
		public void setFormat(int format) {
			/*
					// for backward compatibility reason, OPAQUE always
					// means 565 for SurfaceView
					if (format == PixelFormat.OPAQUE)
					format = PixelFormat.RGB_565;

					mRequestedFormat = format;
					if (mWindow != null) {
					updateWindow(false, false);
					}*/
		}

		/**
		 * @deprecated setType is now ignored.
		 */
		@Override
		@Deprecated
		public void setType(int type) {}

		@Override
		public void setKeepScreenOn(boolean screenOn) {
			//		Message msg = mHandler.obtainMessage(KEEP_SCREEN_ON_MSG);
			//		msg.arg1 = screenOn ? 1 : 0;
			//		mHandler.sendMessage(msg);
		}

		/**
		 * Gets a {@link Canvas} for drawing into the SurfaceView's Surface
		 *
		 * After drawing into the provided {@link Canvas}, the caller must
		 * invoke {@link #unlockCanvasAndPost} to post the new contents to the surface.
		 *
		 * The caller must redraw the entire surface.
		 * @return A canvas for drawing into the surface.
		 */
		@Override
		public Canvas lockCanvas() {
			return internalLockCanvas(null);
		}

		/**
		 * Gets a {@link Canvas} for drawing into the SurfaceView's Surface
		 *
		 * After drawing into the provided {@link Canvas}, the caller must
		 * invoke {@link #unlockCanvasAndPost} to post the new contents to the surface.
		 *
		 * @param inOutDirty A rectangle that represents the dirty region that the caller wants
		 * to redraw.  This function may choose to expand the dirty rectangle if for example
		 * the surface has been resized or if the previous contents of the surface were
		 * not available.  The caller must redraw the entire dirty region as represented
		 * by the contents of the inOutDirty rectangle upon return from this function.
		 * The caller may also pass <code>null</code> instead, in the case where the
		 * entire surface should be redrawn.
		 * @return A canvas for drawing into the surface.
		 */
		@Override
		public Canvas lockCanvas(Rect inOutDirty) {
			return internalLockCanvas(inOutDirty);
		}

		private final Canvas internalLockCanvas(Rect dirty) {
			/*		mSurfaceLock.lock();

					if (DEBUG) Log.i(TAG, "Locking canvas... stopped="
						+ mDrawingStopped + ", win=" + mWindow);

					Canvas c = null;
					if (!mDrawingStopped && mWindow != null) {
					try {
						c = mSurface.lockCanvas(dirty);
					} catch (Exception e) {
						Log.e(LOG_TAG, "Exception locking surface", e);
					}
					}

					if (DEBUG) Log.i(TAG, "Returned canvas: " + c);
					if (c != null) {
					mLastLockTime = SystemClock.uptimeMillis();
					return c;
					}

					// If the Surface is not ready to be drawn, then return null,
					// but throttle calls to this function so it isn't called more
					// than every 100ms.
					long now = SystemClock.uptimeMillis();
					long nextTime = mLastLockTime + 100;
					if (nextTime > now) {
					try {
						Thread.sleep(nextTime-now);
					} catch (InterruptedException e) {
					}
					now = SystemClock.uptimeMillis();
					}
					mLastLockTime = now;
					mSurfaceLock.unlock();
			*/
			if (getWidth() == 0 || getHeight() == 0)
				return null;

			return new DisplayListCanvas(native_createSnapshot(getWidth(), getHeight()));
		}

		/**
		 * Posts the new contents of the {@link Canvas} to the surface and
		 * releases the {@link Canvas}.
		 *
		 * @param canvas The canvas previously obtained from {@link #lockCanvas}.
		 */
		@Override
		public void unlockCanvasAndPost(Canvas canvas) {
			long bitmap = native_canvas_to_bitmap(canvas.getNativeCanvasWrapper());
			postFrame(android.graphics.Bitmap.fromNative(bitmap));
			//		mSurface.unlockCanvasAndPost(canvas);
			//		mSurfaceLock.unlock();
		}

		@Override
		public Surface getSurface() {
			return mSurface;
		}

		@Override
		public Rect getSurfaceFrame() {
			return new Rect(0, 0, getSurfaceWidth(), getSurfaceHeight());
		}
	};

	public void setZOrderOnTop(boolean onTop) {
		if (mZOrderOnTop == onTop)
			return;
		mZOrderOnTop = onTop;
		if (mLayer != 0) {
			native_setLayerZ(mLayer, onTop);
			invalidate(); // the hole becomes (or stops being) opaque content
		}
	}

	public void setZOrderMediaOverlay(boolean mediaOverlay) {
		/* both media overlays and plain content sit below the parent here;
		 * ordering between two layers of the same window is their creation
		 * order, which is what AOSP's default gives too */
	}
}
