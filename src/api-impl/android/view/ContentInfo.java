package android.view;

import android.content.ClipData;

/**
 * Minimal ContentInfo holder. ATL does not implement rich-content insertion
 * (drag/paste/autofill of images etc.); this exists so apps that register an
 * OnReceiveContentListener link and run.
 */
public final class ContentInfo {
	public static final int SOURCE_APP = 0;
	public static final int SOURCE_CLIPBOARD = 1;
	public static final int SOURCE_INPUT_METHOD = 2;
	public static final int SOURCE_DRAG_AND_DROP = 3;
	public static final int SOURCE_AUTOFILL = 4;
	public static final int SOURCE_PROCESS_TEXT = 5;

	public static final int FLAG_CONVERT_TO_PLAIN_TEXT = 1 << 0;

	private final ClipData clip;
	private final int source;
	private final int flags;

	public ContentInfo(ClipData clip, int source) {
		this(clip, source, 0);
	}

	public ContentInfo(ClipData clip, int source, int flags) {
		this.clip = clip;
		this.source = source;
		this.flags = flags;
	}

	public ClipData getClip() {
		return clip;
	}

	public int getSource() {
		return source;
	}

	public int getFlags() {
		return flags;
	}
}
