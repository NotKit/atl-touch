package android.hardware.camera2;

import android.util.Size;

import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * The vendor capture extensions (night, bokeh, HDR) a camera offers.
 *
 * No ATL backend implements any, so the supported list is empty and every
 * per-extension query rejects the extension rather than answering about one
 * that does not exist.
 */
public final class CameraExtensionCharacteristics {

	public static final int EXTENSION_AUTOMATIC = 0;
	public static final int EXTENSION_FACE_RETOUCH = 1;
	public static final int EXTENSION_BOKEH = 2;
	public static final int EXTENSION_HDR = 3;
	public static final int EXTENSION_NIGHT = 4;

	private final String cameraId;

	CameraExtensionCharacteristics(String cameraId) {
		this.cameraId = cameraId;
	}

	public List<Integer> getSupportedExtensions() {
		return Collections.emptyList();
	}

	public <T> List<Size> getExtensionSupportedSizes(int extension, Class<T> klass) {
		return Collections.emptyList();
	}

	public List<Size> getExtensionSupportedSizes(int extension, int format) {
		return Collections.emptyList();
	}

	public <T> List<CaptureRequest.Key> getAvailableCaptureRequestKeys(int extension) {
		return Collections.emptyList();
	}

	public <T> List<CaptureResult.Key> getAvailableCaptureResultKeys(int extension) {
		return Collections.emptyList();
	}

	public boolean isCaptureProcessProgressAvailable(int extension) {
		return false;
	}

	public boolean isPostviewAvailable(int extension) {
		return false;
	}

	public Set<Long> getPostviewSupportedSizes(int extension) {
		return Collections.emptySet();
	}

	@Override
	public String toString() {
		return "CameraExtensionCharacteristics(" + cameraId + ")";
	}
}
