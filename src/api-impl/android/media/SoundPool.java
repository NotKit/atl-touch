package android.media;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.os.Handler;
import android.os.Looper;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class SoundPool {

	private long nativePool;
	private OnLoadCompleteListener onLoadCompleteListener;
	private final List<Integer> loadedSounds = new ArrayList<Integer>();

	public interface OnLoadCompleteListener {
		/**
		 * Called when a sound has completed loading.
		 *
		 * @param soundPool SoundPool object from the load() method
		 * @param sampleId the sample ID of the sound loaded.
		 * @param status the status of the load operation (0 = success)
		 */
		public void onLoadComplete(SoundPool soundPool, int sampleId, int status);
	}

	public SoundPool(int maxStreams, int streamType, int srcQuality) {
		nativePool = native_constructor();
	}

	/* loading is synchronous (the sample is a local file), but apps register
	 * the listener after load() returns — deliver the callback from the main
	 * looper like Android does, so both orders work */
	private void notifyLoaded(final int soundID) {
		if (soundID == 0 || onLoadCompleteListener == null)
			return;
		final OnLoadCompleteListener listener = onLoadCompleteListener;
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				listener.onLoadComplete(SoundPool.this, soundID, 0);
			}
		});
	}

	private int registerLoad(int soundID) {
		if (soundID != 0) {
			loadedSounds.add(soundID);
			notifyLoaded(soundID);
		}
		return soundID;
	}

	public int load(AssetFileDescriptor afd, int priority) throws IOException {
		AssetManager.extractFromAPK(Context.this_application.getPackageCodePath(), afd.fileName, afd.fileName);
		return registerLoad(nativeLoad(nativePool, android.os.Environment.getExternalStorageDirectory().getPath() + "/" + afd.fileName));
	}

	public int load(Context context, int resId, int priority) throws IOException {
		String fileName = context.getResources().getResourceEntryName(resId);
		AssetManager.extractFromAPK(context.getPackageCodePath(), fileName, fileName);
		return registerLoad(nativeLoad(nativePool, android.os.Environment.getExternalStorageDirectory().getPath() + "/" + fileName));
	}

	public int load(String path, int priority) {
		return registerLoad(nativeLoad(nativePool, path));
	}

	public void setOnLoadCompleteListener(OnLoadCompleteListener listener) {
		onLoadCompleteListener = listener;
		for (int soundID : loadedSounds)
			notifyLoaded(soundID);
	}

	public final int play(int soundID, float leftVolume, float rightVolume, int priority, int loop, float rate) {
		return nativePlay(nativePool, soundID, (leftVolume + rightVolume) / 2.f, loop);
	}

	public final void stop(int streamID) {}
	public final void pause(int streamID) {}
	public final void resume(int streamID) {}

	public void autoResume() {}

	public void autoPause() {}

	public void setVolume(int streamType, float leftVolume, float rightVolume) {}

	public class Builder {
		public Builder setAudioAttributes(AudioAttributes attributes) {
			return this;
		}

		public Builder setMaxStreams(int maxStreams) {
			return this;
		}

		public SoundPool build() {
			return new SoundPool(0, 0, 0); // FIXME
		}
	}

	public void release() {}

	private static native long native_constructor();
	private static native int nativeLoad(long nativePool, String path);
	private static native int nativePlay(long nativePool, int soundID, float volume, int loop);
}
