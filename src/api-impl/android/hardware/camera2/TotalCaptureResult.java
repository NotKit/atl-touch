package android.hardware.camera2;

import android.graphics.Rect;
import android.hardware.camera2.impl.CameraMetadataNative;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * The final result of a capture. ATL delivers results whole, so there are never
 * partial results to collect.
 *
 * A logical multi-camera reports which of its physical cameras took the frame in
 * android.logicalMultiCamera.activePhysicalId, and that is what the physical
 * result is keyed by. ATL runs one stream off the logical camera, so the
 * physical result is the logical one - with the regions that are relative to a
 * sensor's active array expressed in the physical camera's array instead, which
 * is what a real HAL hands over and what an app reading them assumes.
 *
 * Google Camera pairs the two: it takes the crop region out of the physical
 * result and the active array out of that physical camera's characteristics,
 * and feeds both to its stabiliser, which aborts the process on a crop wider
 * than the array. On caiman the logical camera's array is 4080x3072 and four of
 * its six physical cameras are 4032x3024, so handing the logical crop over
 * unchanged killed Google Camera whenever one of those was the active lens.
 */
public final class TotalCaptureResult extends CaptureResult {

	/** every camera's active array, which is fixed for the life of the process */
	private static final Map<String, Rect> activeArrays = new HashMap<String, Rect>();

	private final String cameraId;
	private Map<String, TotalCaptureResult> physicalResults;

	TotalCaptureResult(CameraMetadataNative results, CaptureRequest request, long frameNumber,
	    int sequenceId, String cameraId) {
		super(results, request, frameNumber, sequenceId);
		this.cameraId = cameraId;
	}

	public List<CaptureResult> getPartialResults() {
		return Collections.emptyList();
	}

	public Map<String, CaptureResult> getPhysicalCameraResults() {
		Map<String, TotalCaptureResult> results = physical();

		if (results.isEmpty())
			return Collections.emptyMap();

		Map.Entry<String, TotalCaptureResult> only = results.entrySet().iterator().next();
		return Collections.<String, CaptureResult>singletonMap(only.getKey(), only.getValue());
	}

	public Map<String, TotalCaptureResult> getPhysicalCameraTotalResults() {
		return physical();
	}

	/** built once: an app asks for it on every frame. */
	private synchronized Map<String, TotalCaptureResult> physical() {
		if (physicalResults != null)
			return physicalResults;

		String id = get(LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID);

		physicalResults = id == null
		    ? Collections.<String, TotalCaptureResult>emptyMap()
		    : Collections.singletonMap(id, forPhysicalCamera(id));
		return physicalResults;
	}

	/**
	 * This result as the physical camera would have reported it. The result
	 * stands in unchanged when the two cameras share an active array, which is
	 * the common case and costs nothing.
	 */
	private TotalCaptureResult forPhysicalCamera(String physicalId) {
		Rect logical = activeArray(cameraId);
		Rect physical = activeArray(physicalId);
		Rect crop = get(SCALER_CROP_REGION);

		if (logical == null || physical == null || logical.equals(physical) || crop == null)
			return this;

		Rect scaled = rescaleRegion(crop, logical, physical);

		if (scaled == null)
			return this;

		CameraMetadataNative metadata = copySettings();
		metadata.set(SCALER_CROP_REGION.getName(), scaled);

		TotalCaptureResult result = new TotalCaptureResult(metadata, getRequest(),
		    getFrameNumber(), getSequenceId(), physicalId);
		/* a physical result is not itself a logical camera's */
		result.physicalResults = Collections.<String, TotalCaptureResult>emptyMap();
		return result;
	}

	/**
	 * A region given in one sensor's active array, in another's. Rounding can
	 * only ever push it outward by a pixel, so the result is clipped: a consumer
	 * that compares it against the array it is now in has to see it fit.
	 */
	static Rect rescaleRegion(Rect region, Rect from, Rect to) {
		if (region == null || from == null || to == null
		    || from.width() <= 0 || from.height() <= 0)
			return null;

		double sx = (double)to.width() / from.width();
		double sy = (double)to.height() / from.height();
		Rect out = new Rect(
		    to.left + (int)Math.round((region.left - from.left) * sx),
		    to.top + (int)Math.round((region.top - from.top) * sy),
		    to.left + (int)Math.round((region.right - from.left) * sx),
		    to.top + (int)Math.round((region.bottom - from.top) * sy));

		if (out.left < to.left)
			out.left = to.left;
		if (out.top < to.top)
			out.top = to.top;
		if (out.right > to.right)
			out.right = to.right;
		if (out.bottom > to.bottom)
			out.bottom = to.bottom;
		return out;
	}

	/** null for a camera the backend does not know, or one with no array. */
	private static Rect activeArray(String cameraId) {
		if (cameraId == null)
			return null;

		synchronized (activeArrays) {
			if (activeArrays.containsKey(cameraId))
				return activeArrays.get(cameraId);

			CameraMetadataNative statics = CameraMetadataNative.getStaticMetadata(cameraId);
			Rect array = null;

			if (statics != null) {
				array = (Rect)statics.get(
				    CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE.getName(), Rect.class);
				statics.close();
			}
			activeArrays.put(cameraId, array);
			return array;
		}
	}
}
