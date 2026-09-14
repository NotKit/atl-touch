package android.hardware.camera2.params;

import java.util.Collection;
import java.util.Collections;
import java.util.List;

/** The size and format of a reprocessing session's input stream. */
public final class InputConfiguration {

	private final int width;
	private final int height;
	private final int format;
	private final boolean multiResolution;

	public InputConfiguration(int width, int height, int format) {
		this.width = width;
		this.height = height;
		this.format = format;
		this.multiResolution = false;
	}

	public InputConfiguration(Collection<MultiResolutionStreamInfo> streams, int format) {
		if (streams == null || streams.isEmpty())
			throw new IllegalArgumentException("a multi-resolution input needs at least one stream");
		MultiResolutionStreamInfo first = streams.iterator().next();

		this.width = first.getWidth();
		this.height = first.getHeight();
		this.format = format;
		this.multiResolution = true;
	}

	public int getWidth() {
		return width;
	}

	public int getHeight() {
		return height;
	}

	public int getFormat() {
		return format;
	}

	public boolean isMultiResolution() {
		return multiResolution;
	}

	public List<MultiResolutionStreamInfo> getMultiResolutionInfo() {
		return Collections.emptyList();
	}

	@Override
	public boolean equals(Object other) {
		if (!(other instanceof InputConfiguration))
			return false;
		InputConfiguration o = (InputConfiguration)other;
		return width == o.width && height == o.height && format == o.format;
	}

	@Override
	public int hashCode() {
		return (width * 31 + height) * 31 + format;
	}

	@Override
	public String toString() {
		return "InputConfiguration(" + width + "x" + height + ", format " + format + ")";
	}
}
