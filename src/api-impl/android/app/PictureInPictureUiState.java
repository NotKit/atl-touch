package android.app;

/**
 * Stub. There is no picture-in-picture mode here, so nothing ever constructs
 * one and both answers are false.
 *
 * It exists because the type has to resolve: androidx.activity's
 * ComponentActivity declares
 * onPictureInPictureUiStateChanged(PictureInPictureUiState) as an override, and
 * loading that class - which anything reflecting over an activity does -
 * resolves the parameter type.
 */
public class PictureInPictureUiState {

	private final boolean isStashed;
	private final boolean isTransitioningToPip;

	private PictureInPictureUiState(Builder builder) {
		this.isStashed = builder.isStashed;
		this.isTransitioningToPip = builder.isTransitioningToPip;
	}

	public boolean isStashed() { return isStashed; }

	public boolean isTransitioningToPip() { return isTransitioningToPip; }

	public static final class Builder {

		private boolean isStashed;
		private boolean isTransitioningToPip;

		public Builder() {}

		public Builder setStashed(boolean isStashed) {
			this.isStashed = isStashed;
			return this;
		}

		public Builder setTransitioningToPip(boolean isTransitioningToPip) {
			this.isTransitioningToPip = isTransitioningToPip;
			return this;
		}

		public PictureInPictureUiState build() {
			return new PictureInPictureUiState(this);
		}
	}
}
