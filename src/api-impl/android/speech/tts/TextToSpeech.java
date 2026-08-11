package android.speech.tts;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;

public class TextToSpeech {
	public static final int ERROR = -1;

	public TextToSpeech(Context context, TextToSpeech.OnInitListener listener) {
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				listener.onInit(ERROR);
			}
		});
	}

	public int setOnUtteranceCompletedListener(TextToSpeech.OnUtteranceCompletedListener listener) {
		return ERROR;
	}

	public int setOnUtteranceProgressListener(UtteranceProgressListener listener) {
		return ERROR;
	}

	public void shutdown() {
	}

	public int stop() {
		return ERROR;
	}

	public static interface OnInitListener {
		abstract void onInit(int status);
	}

	public static interface OnUtteranceCompletedListener {
		public abstract void onUtteranceCompleted(String utteranceId);
	}

	public int isLanguageAvailable(java.util.Locale a0) { return 0; }

	public int setLanguage(java.util.Locale a0) { return 0; }

	public int setPitch(float a0) { return 0; }

	public int setSpeechRate(float a0) { return 0; }

	public java.util.Locale getDefaultLanguage() { return null; }

	public java.util.Set getAvailableLanguages() { return null; }

	public java.util.Set getFeatures(java.util.Locale a0) { return null; }

	public static final int QUEUE_FLUSH = 0;

	public static final int SUCCESS = 0;

	public int speak(java.lang.String a0, int a1, java.util.HashMap a2) { return 0; }
}
