package android.hardware.camera2;

import android.hardware.camera2.params.MultiResolutionStreamInfo;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.view.Surface;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.concurrent.Executor;

/**
 * An ImageReader per physical camera behind one surface.
 *
 * No ATL backend has physical sub-cameras, so the group holds exactly one
 * reader — the largest stream the app asked for — and every query answers
 * about it. Apps written for multi-resolution cameras then work unchanged.
 */
public class MultiResolutionImageReader implements AutoCloseable {

	private final List<MultiResolutionStreamInfo> streams = new ArrayList<MultiResolutionStreamInfo>();
	private final MultiResolutionStreamInfo largest;
	private final ImageReader reader;

	public MultiResolutionImageReader(Collection<MultiResolutionStreamInfo> streams,
	    int format, int maxImages) {
		if (streams == null || streams.isEmpty())
			throw new IllegalArgumentException("a multi-resolution reader needs at least one stream");

		MultiResolutionStreamInfo biggest = null;
		for (MultiResolutionStreamInfo stream : streams) {
			this.streams.add(stream);
			if (biggest == null ||
			    stream.getWidth() * stream.getHeight() > biggest.getWidth() * biggest.getHeight())
				biggest = stream;
		}
		this.largest = biggest;
		this.reader = ImageReader.newInstance(biggest.getWidth(), biggest.getHeight(), format, maxImages);
	}

	public Surface getSurface() {
		return reader.getSurface();
	}

	public void setOnImageAvailableListener(ImageReader.OnImageAvailableListener listener,
	    Executor executor) {
		reader.setOnImageAvailableListener(listener, null);
	}

	public void setOnImageAvailableListener(ImageReader.OnImageAvailableListener listener,
	    Handler handler) {
		reader.setOnImageAvailableListener(listener, handler);
	}

	public MultiResolutionStreamInfo getStreamInfoForImageReader(ImageReader reader) {
		if (reader != this.reader)
			throw new IllegalArgumentException("that reader is not part of this group");
		return largest;
	}

	public Collection<MultiResolutionStreamInfo> getStreamInfo() {
		return streams;
	}

	/** Drops everything the app has not acquired, the way AOSP's flush() does. */
	public void flush() {
		Image image;

		while ((image = reader.acquireNextImage()) != null)
			image.close();
		reader.discardFreeBuffers();
	}

	@Override
	public void close() {
		reader.close();
	}

	/* the readers themselves, for OutputConfiguration and the session */
	ImageReader getReader() {
		return reader;
	}

	public static final class Builder {
		private final List<MultiResolutionStreamInfo> streams = new ArrayList<MultiResolutionStreamInfo>();
		private int format = android.graphics.ImageFormat.PRIVATE;
		private int maxImages = 2;
		private long usage;

		public Builder(Collection<MultiResolutionStreamInfo> streams) {
			if (streams == null || streams.isEmpty())
				throw new IllegalArgumentException("a multi-resolution reader needs at least one stream");
			this.streams.addAll(streams);
		}

		public Builder setFormat(int format) {
			this.format = format;
			return this;
		}

		public Builder setMaxImages(int maxImages) {
			this.maxImages = maxImages;
			return this;
		}

		public Builder setUsage(long usage) {
			this.usage = usage;
			return this;
		}

		/** One reader is always concurrent with itself; the flag is remembered, not acted on. */
		public Builder setConcurrentOutputsEnabled(boolean enabled) {
			return this;
		}

		public MultiResolutionImageReader build() {
			return new MultiResolutionImageReader(streams, format, maxImages);
		}
	}
}
