package android.hardware.camera2.params;

import android.graphics.Point;
import android.graphics.Rect;

/**
 * One detected face. SIMPLE face detection reports only the bounds and a score;
 * the landmarks and the id come with FULL, so they stay null / ID_UNSUPPORTED
 * unless the camera reported them.
 */
public final class Face {

	public static final int ID_UNSUPPORTED = -1;
	public static final int SCORE_MIN = 1;
	public static final int SCORE_MAX = 100;

	private final Rect bounds;
	private final int score;
	private final int id;
	private final Point leftEye;
	private final Point rightEye;
	private final Point mouth;

	private Face(Rect bounds, int score, int id, Point leftEye, Point rightEye, Point mouth) {
		this.bounds = bounds;
		this.score = score;
		this.id = id;
		this.leftEye = leftEye;
		this.rightEye = rightEye;
		this.mouth = mouth;
	}

	public Rect getBounds() {
		return bounds;
	}

	public int getScore() {
		return score;
	}

	public int getId() {
		return id;
	}

	public Point getLeftEyePosition() {
		return leftEye;
	}

	public Point getRightEyePosition() {
		return rightEye;
	}

	public Point getMouthPosition() {
		return mouth;
	}

	@Override
	public String toString() {
		return "Face(id=" + id + ", score=" + score + ", bounds=" + bounds + ")";
	}

	public static final class Builder {
		private Rect bounds;
		private int score = SCORE_MAX;
		private int id = ID_UNSUPPORTED;
		private Point leftEye;
		private Point rightEye;
		private Point mouth;

		public Builder() {
		}

		public Builder(Face face) {
			bounds = face.bounds;
			score = face.score;
			id = face.id;
			leftEye = face.leftEye;
			rightEye = face.rightEye;
			mouth = face.mouth;
		}

		public Builder setBounds(Rect bounds) {
			this.bounds = bounds;
			return this;
		}

		public Builder setScore(int score) {
			if (score < SCORE_MIN || score > SCORE_MAX)
				throw new IllegalArgumentException("a face score is " + SCORE_MIN + ".." + SCORE_MAX);
			this.score = score;
			return this;
		}

		public Builder setId(int id) {
			this.id = id;
			return this;
		}

		public Builder setLeftEyePosition(Point leftEyePosition) {
			this.leftEye = leftEyePosition;
			return this;
		}

		public Builder setRightEyePosition(Point rightEyePosition) {
			this.rightEye = rightEyePosition;
			return this;
		}

		public Builder setMouthPosition(Point mouthPosition) {
			this.mouth = mouthPosition;
			return this;
		}

		/** An id or a landmark makes the face a FULL one, which needs them all. */
		public Face build() {
			if (bounds == null)
				throw new IllegalStateException("a face needs its bounds");
			boolean full = id != ID_UNSUPPORTED;
			boolean landmarks = leftEye != null || rightEye != null || mouth != null;

			if (full && !landmarks)
				throw new IllegalStateException("a face with an id needs its landmarks");
			if (landmarks && (leftEye == null || rightEye == null || mouth == null))
				throw new IllegalStateException("a face needs all three landmarks or none");
			return new Face(bounds, score, id, leftEye, rightEye, mouth);
		}
	}
}
