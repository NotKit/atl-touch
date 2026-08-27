package android.media;

/* API 26's audio focus request. Fenix's own media-session code builds one for
 * every playing media element (AudioFocusControllerV26), so the class has to
 * exist; focus itself is not arbitrated here, requestAudioFocus just grants. */
public class AudioFocusRequest {

	private final int focusGain;
	private final AudioAttributes attributes;
	private final AudioManager.OnAudioFocusChangeListener listener;

	private AudioFocusRequest(Builder builder) {
		this.focusGain = builder.focusGain;
		this.attributes = builder.attributes;
		this.listener = builder.listener;
	}

	public int getFocusGain() {
		return focusGain;
	}

	public AudioAttributes getAudioAttributes() {
		return attributes;
	}

	public AudioManager.OnAudioFocusChangeListener getOnAudioFocusChangeListener() {
		return listener;
	}

	public boolean acceptsDelayedFocusGain() {
		return false;
	}

	public boolean willPauseWhenDucked() {
		return false;
	}

	public static final class Builder {

		private int focusGain;
		private AudioAttributes attributes;
		private AudioManager.OnAudioFocusChangeListener listener;

		public Builder(int focusGain) {
			this.focusGain = focusGain;
		}

		public Builder(AudioFocusRequest request) {
			this.focusGain = request.focusGain;
			this.attributes = request.attributes;
			this.listener = request.listener;
		}

		public Builder setFocusGain(int focusGain) {
			this.focusGain = focusGain;
			return this;
		}

		public Builder setAudioAttributes(AudioAttributes attributes) {
			this.attributes = attributes;
			return this;
		}

		public Builder setOnAudioFocusChangeListener(AudioManager.OnAudioFocusChangeListener listener) {
			this.listener = listener;
			return this;
		}

		public Builder setOnAudioFocusChangeListener(AudioManager.OnAudioFocusChangeListener listener, android.os.Handler handler) {
			this.listener = listener;
			return this;
		}

		public Builder setAcceptsDelayedFocusGain(boolean accepts) {
			return this;
		}

		public Builder setWillPauseWhenDucked(boolean pause) {
			return this;
		}

		public Builder setForceDucking(boolean force) {
			return this;
		}

		public AudioFocusRequest build() {
			return new AudioFocusRequest(this);
		}
	}
}
