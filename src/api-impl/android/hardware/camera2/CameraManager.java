package android.hardware.camera2;

/**
 * camera2 is not implemented; android.hardware.Camera (Camera1) is. Reporting
 * "no access" rather than an empty camera list is what makes apps fall back:
 * webrtc's Camera2Enumerator.isSupported() reads an empty id list as "camera2
 * works, this device just has no cameras" and returns true.
 */
public class CameraManager {
	public String[] getCameraIdList() throws CameraAccessException {
		throw new CameraAccessException(CameraAccessException.CAMERA_DISABLED,
		    "camera2 is not implemented in ATL, use android.hardware.Camera");
	}
}
