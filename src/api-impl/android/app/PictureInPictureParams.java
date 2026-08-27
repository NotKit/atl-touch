package android.app;

import android.graphics.Rect;
import android.util.Rational;

import java.util.List;

/**
 * Stub. There is no picture-in-picture mode here: a Builder collects the values
 * the app sets and build() hands them back, but nothing ever reads them and
 * Activity.setPictureInPictureParams() drops them.
 *
 * It exists because the class has to resolve, not because the feature works:
 * LaunchActivity declares setPictureInPictureParams(PictureInPictureParams),
 * and reflecting over its declared methods loads the parameter type.
 */
public class PictureInPictureParams {

	private final Rational aspectRatio;
	private final Rational expandedAspectRatio;
	private final Rect sourceRectHint;
	private final List<?> actions;
	private final boolean autoEnterEnabled;

	private PictureInPictureParams(Builder builder) {
		this.aspectRatio = builder.aspectRatio;
		this.expandedAspectRatio = builder.expandedAspectRatio;
		this.sourceRectHint = builder.sourceRectHint;
		this.actions = builder.actions;
		this.autoEnterEnabled = builder.autoEnterEnabled;
	}

	public Rational getAspectRatio() { return aspectRatio; }

	public Rational getExpandedAspectRatio() { return expandedAspectRatio; }

	public Rect getSourceRectHint() { return sourceRectHint; }

	public List<?> getActions() { return actions; }

	public boolean isAutoEnterEnabled() { return autoEnterEnabled; }

	public static class Builder {

		private Rational aspectRatio;
		private Rational expandedAspectRatio;
		private Rect sourceRectHint;
		private List<?> actions;
		private boolean autoEnterEnabled;

		public Builder() {}

		public Builder setAspectRatio(Rational aspectRatio) {
			this.aspectRatio = aspectRatio;
			return this;
		}

		public Builder setExpandedAspectRatio(Rational expandedAspectRatio) {
			this.expandedAspectRatio = expandedAspectRatio;
			return this;
		}

		public Builder setSourceRectHint(Rect sourceRectHint) {
			this.sourceRectHint = sourceRectHint;
			return this;
		}

		/* The SDK types this List<RemoteAction>; api-impl has no RemoteAction and
		 * the erased descriptor an app call site uses is the same either way. */
		public Builder setActions(List<?> actions) {
			this.actions = actions;
			return this;
		}

		public Builder setAutoEnterEnabled(boolean autoEnterEnabled) {
			this.autoEnterEnabled = autoEnterEnabled;
			return this;
		}

		public PictureInPictureParams build() {
			return new PictureInPictureParams(this);
		}
	}
}
