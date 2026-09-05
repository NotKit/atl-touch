package android.media.projection;

public class MediaProjectionManager {

	/**
	 * There is no screen-capture consent activity to start, so the intent
	 * resolves to nothing and the caller never gets a result back.
	 */
	public android.content.Intent createScreenCaptureIntent() {
		return new android.content.Intent("android.media.projection.MediaProjectionManager.ACTION_SCREEN_CAPTURE");
	}

	public android.media.projection.MediaProjection getMediaProjection(int a0, android.content.Intent a1) { return null; }
}
