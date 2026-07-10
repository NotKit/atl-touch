package android.media;

import android.content.Context;
import android.net.Uri;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Map;

/**
 * MediaPlayer over a native GStreamer playbin peer. Native bus events arrive
 * through postEventFromNative on the main (GLib/Looper) thread.
 */
public class MediaPlayer {
	private long nativePtr;

	private OnCompletionListener onCompletionListener;
	private OnPreparedListener onPreparedListener;
	private OnErrorListener onErrorListener;
	private OnSeekCompleteListener onSeekCompleteListener;

	// must match the EVENT_* defines in android_media_MediaPlayer.c
	private static final int EVENT_PREPARED = 1;
	private static final int EVENT_COMPLETION = 2;
	private static final int EVENT_SEEK_COMPLETE = 3;
	private static final int EVENT_ERROR = 100;

	public static final int MEDIA_ERROR_UNKNOWN = 1;

	public interface OnCompletionListener {
		void onCompletion(MediaPlayer mp);
	}
	public interface OnErrorListener {
		boolean onError(MediaPlayer mp, int what, int extra);
	}
	public interface OnPreparedListener {
		void onPrepared(MediaPlayer mp);
	}
	public interface OnBufferingUpdateListener {
		void onBufferingUpdate(MediaPlayer mp, int percent);
	}
	public interface OnInfoListener {
		boolean onInfo(MediaPlayer mp, int what, int extra);
	}
	public interface OnSeekCompleteListener {
		void onSeekComplete(MediaPlayer mp);
	}
	public interface OnVideoSizeChangedListener {
		void onVideoSizeChanged(MediaPlayer mp, int width, int height);
	}
	public interface MediaPlayerControl {
	}

	public MediaPlayer() {
		nativePtr = native_create();
	}

	public static MediaPlayer create(Context context, int resId) {
		try {
			MediaPlayer mp = new MediaPlayer();
			String fileName = context.getResources().getResourceEntryName(resId);
			android.content.res.AssetManager.extractFromAPK(context.getPackageCodePath(), fileName, fileName);
			mp.setDataSource(android.os.Environment.getExternalStorageDirectory().getPath() + "/" + fileName);
			mp.prepare();
			return mp;
		} catch (Exception e) {
			e.printStackTrace();
			return null;
		}
	}

	// called from the native bus watch (main thread)
	private void postEventFromNative(int what, int arg1, int arg2) {
		switch (what) {
			case EVENT_PREPARED:
				if (onPreparedListener != null)
					onPreparedListener.onPrepared(this);
				break;
			case EVENT_COMPLETION:
				if (onCompletionListener != null)
					onCompletionListener.onCompletion(this);
				break;
			case EVENT_SEEK_COMPLETE:
				if (onSeekCompleteListener != null)
					onSeekCompleteListener.onSeekComplete(this);
				break;
			case EVENT_ERROR:
				boolean handled = false;
				if (onErrorListener != null)
					handled = onErrorListener.onError(this, arg1, arg2);
				if (!handled && onCompletionListener != null)
					onCompletionListener.onCompletion(this);
				break;
		}
	}

	public void setDataSource(String path) {
		if (nativePtr != 0)
			native_setDataSource(nativePtr, path);
	}

	public void setDataSource(FileDescriptor fd) throws IOException {
		setDataSource(fd, 0, 0);
	}

	/* GStreamer wants a URI; materialize the fd range as a cache file */
	public void setDataSource(FileDescriptor fd, long offset, long length) throws IOException {
		File cacheFile = File.createTempFile("mediaplayer", null,
		                                     Context.this_application.getCacheDir());
		cacheFile.deleteOnExit();
		try (InputStream in = new FileInputStream(fd);
		     FileOutputStream out = new FileOutputStream(cacheFile)) {
			long skipped = 0;
			while (skipped < offset) {
				long n = in.skip(offset - skipped);
				if (n <= 0)
					break;
				skipped += n;
			}
			byte[] buffer = new byte[65536];
			long remaining = length > 0 ? length : Long.MAX_VALUE;
			while (remaining > 0) {
				int n = in.read(buffer, 0, (int)Math.min(buffer.length, remaining));
				if (n < 0)
					break;
				out.write(buffer, 0, n);
				remaining -= n;
			}
		}
		setDataSource(cacheFile.getPath());
	}

	public void setDataSource(Context context, Uri uri) {
		setDataSource(uri.getPath());
	}

	public void setDataSource(Context context, Uri uri, Map<String, String> headers) {
		setDataSource(uri.getPath());
	}

	public void setLooping(boolean looping) {
		if (nativePtr != 0)
			native_setLooping(nativePtr, looping);
	}

	public void setOnCompletionListener(OnCompletionListener listener) {
		onCompletionListener = listener;
	}
	public void setOnErrorListener(OnErrorListener listener) {
		onErrorListener = listener;
	}
	public void setOnPreparedListener(OnPreparedListener listener) {
		onPreparedListener = listener;
	}
	public void setOnSeekCompleteListener(OnSeekCompleteListener listener) {
		onSeekCompleteListener = listener;
	}
	public void setOnBufferingUpdateListener(OnBufferingUpdateListener dummy) {}
	public void setOnInfoListener(OnInfoListener dummy) {}
	public void setOnVideoSizeChangedListener(OnVideoSizeChangedListener dummy) {}
	public void setAudioAttributes(AudioAttributes attributes) {}
	public void setAudioStreamType(int dummy) {}

	public void start() {
		if (nativePtr != 0)
			native_start(nativePtr);
	}

	public void stop() {
		if (nativePtr != 0)
			native_stop(nativePtr);
	}

	public void pause() {
		if (nativePtr != 0)
			native_pause(nativePtr);
	}

	public void prepare() throws IOException {
		if (nativePtr != 0 && !native_prepare(nativePtr))
			throw new IOException("MediaPlayer: prepare failed");
	}

	public void prepareAsync() {
		if (nativePtr != 0)
			native_prepareAsync(nativePtr);
	}

	public void reset() {
		if (nativePtr != 0)
			native_reset(nativePtr);
	}

	public void release() {
		if (nativePtr != 0) {
			native_release(nativePtr);
			nativePtr = 0;
		}
		onCompletionListener = null;
		onPreparedListener = null;
		onErrorListener = null;
		onSeekCompleteListener = null;
	}

	public boolean isPlaying() {
		return nativePtr != 0 && native_isPlaying(nativePtr);
	}

	public void seekTo(int msec) {
		if (nativePtr != 0)
			native_seekTo(nativePtr, msec);
	}

	public void setVolume(float leftVolume, float rightVolume) {
		if (nativePtr != 0)
			native_setVolume(nativePtr, (leftVolume + rightVolume) / 2.f);
	}

	public int getDuration() {
		return nativePtr != 0 ? native_getDuration(nativePtr) : -1;
	}

	public int getCurrentPosition() {
		return nativePtr != 0 ? native_getCurrentPosition(nativePtr) : 0;
	}

	public int getAudioSessionId() { return 0; }

	public void setWakeMode(Context context, int mode) {}

	private native long native_create();
	private static native void native_setDataSource(long ptr, String path);
	private static native boolean native_prepare(long ptr);
	private static native void native_prepareAsync(long ptr);
	private static native void native_start(long ptr);
	private static native void native_pause(long ptr);
	private static native void native_stop(long ptr);
	private static native boolean native_isPlaying(long ptr);
	private static native void native_seekTo(long ptr, int msec);
	private static native void native_setLooping(long ptr, boolean looping);
	private static native void native_setVolume(long ptr, float volume);
	private static native int native_getDuration(long ptr);
	private static native int native_getCurrentPosition(long ptr);
	private static native void native_reset(long ptr);
	private static native void native_release(long ptr);
}
