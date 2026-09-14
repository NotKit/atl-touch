package android.hardware.camera2.params;

import android.hardware.camera2.MultiResolutionImageReader;
import android.util.Size;
import android.view.Surface;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * One session output: the surface (or surfaces, when sharing is on) plus the
 * attributes an app asks the camera service for.
 *
 * ATL's session configuration only ever uses the surfaces; the rest is state
 * an app can set and read back, because that is what CameraX and GCam do
 * before they configure anything.
 */
public final class OutputConfiguration {

	public static final int SURFACE_GROUP_ID_NONE = -1;

	public static final int ROTATION_0 = 0;
	public static final int ROTATION_90 = 1;
	public static final int ROTATION_180 = 2;
	public static final int ROTATION_270 = 3;

	public static final int TIMESTAMP_BASE_DEFAULT = 0;
	public static final int TIMESTAMP_BASE_SENSOR = 1;
	public static final int TIMESTAMP_BASE_MONOTONIC = 2;
	public static final int TIMESTAMP_BASE_REALTIME = 3;
	public static final int TIMESTAMP_BASE_CHOREOGRAPHER_SYNCED = 4;

	public static final int MIRROR_MODE_AUTO = 0;
	public static final int MIRROR_MODE_NONE = 1;
	public static final int MIRROR_MODE_H = 2;
	public static final int MIRROR_MODE_V = 3;

	/* AOSP's own cap on a shared output */
	private static final int MAX_SURFACES_COUNT = 4;

	private final List<Surface> surfaces = new ArrayList<Surface>();
	private final Set<Integer> sensorPixelModes = new HashSet<Integer>();
	private final int surfaceGroupId;
	private final int rotation;
	private final Size surfaceSize;
	private final Class<?> surfaceClass;

	private String physicalCameraId;
	private boolean surfaceSharing;
	private long streamUseCase;
	private long dynamicRangeProfile = DynamicRangeProfiles.STANDARD;
	private int timestampBase = TIMESTAMP_BASE_DEFAULT;
	private int mirrorMode = MIRROR_MODE_AUTO;
	private boolean readoutTimestampEnabled;

	public OutputConfiguration(Surface surface) {
		this(SURFACE_GROUP_ID_NONE, surface, ROTATION_0);
	}

	public OutputConfiguration(int surfaceGroupId, Surface surface) {
		this(surfaceGroupId, surface, ROTATION_0);
	}

	public OutputConfiguration(int surfaceGroupId, Surface surface, int rotation) {
		if (surface == null)
			throw new IllegalArgumentException("surface must not be null");
		surfaces.add(surface);
		this.surfaceGroupId = surfaceGroupId;
		this.rotation = rotation;
		this.surfaceSize = null;
		this.surfaceClass = null;
	}

	/** A deferred output: the surface comes later, through addSurface(). */
	public OutputConfiguration(Size surfaceSize, Class<?> klass) {
		if (surfaceSize == null || klass == null)
			throw new IllegalArgumentException("a deferred output needs a size and a class");
		this.surfaceGroupId = SURFACE_GROUP_ID_NONE;
		this.rotation = ROTATION_0;
		this.surfaceSize = surfaceSize;
		this.surfaceClass = klass;
	}

	public OutputConfiguration(int surfaceGroupId, Size surfaceSize, Class<?> klass) {
		if (surfaceSize == null || klass == null)
			throw new IllegalArgumentException("a deferred output needs a size and a class");
		this.surfaceGroupId = surfaceGroupId;
		this.rotation = ROTATION_0;
		this.surfaceSize = surfaceSize;
		this.surfaceClass = klass;
	}

	/** ATL has no physical sub-cameras: one reader, one configuration. */
	public static List<OutputConfiguration> createInstancesForMultiResolutionOutput(
	    MultiResolutionImageReader multiResolutionImageReader) {
		if (multiResolutionImageReader == null)
			throw new IllegalArgumentException("multiResolutionImageReader must not be null");
		return Collections.singletonList(
		    new OutputConfiguration(multiResolutionImageReader.getSurface()));
	}

	public Surface getSurface() {
		return surfaces.isEmpty() ? null : surfaces.get(0);
	}

	public List<Surface> getSurfaces() {
		return Collections.unmodifiableList(surfaces);
	}

	public void addSurface(Surface surface) {
		if (surface == null)
			throw new IllegalArgumentException("surface must not be null");
		if (surfaces.contains(surface))
			throw new IllegalStateException("the surface is already part of this configuration");
		if (!surfaces.isEmpty() && !surfaceSharing)
			throw new IllegalStateException("surface sharing is not enabled on this configuration");
		if (surfaces.size() >= MAX_SURFACES_COUNT)
			throw new IllegalArgumentException("no more than " + MAX_SURFACES_COUNT + " shared surfaces");
		surfaces.add(surface);
	}

	public void removeSurface(Surface surface) {
		if (getSurface() == surface)
			throw new IllegalArgumentException("the first surface cannot be removed");
		surfaces.remove(surface);
	}

	public void enableSurfaceSharing() {
		surfaceSharing = true;
	}

	public int getMaxSharedSurfaceCount() {
		return MAX_SURFACES_COUNT;
	}

	public int getSurfaceGroupId() {
		return surfaceGroupId;
	}

	public int getRotation() {
		return rotation;
	}

	public Size getSurfaceSize() {
		return surfaceSize;
	}

	public Class<?> getSurfaceClass() {
		return surfaceClass;
	}

	public void setPhysicalCameraId(String physicalCameraId) {
		this.physicalCameraId = physicalCameraId;
	}

	public String getPhysicalCameraId() {
		return physicalCameraId;
	}

	public void setStreamUseCase(long streamUseCase) {
		this.streamUseCase = streamUseCase;
	}

	public long getStreamUseCase() {
		return streamUseCase;
	}

	public void setDynamicRangeProfile(long profile) {
		this.dynamicRangeProfile = profile;
	}

	public long getDynamicRangeProfile() {
		return dynamicRangeProfile;
	}

	public void setTimestampBase(int timestampBase) {
		this.timestampBase = timestampBase;
	}

	public int getTimestampBase() {
		return timestampBase;
	}

	public void setMirrorMode(int mirrorMode) {
		this.mirrorMode = mirrorMode;
	}

	public int getMirrorMode() {
		return mirrorMode;
	}

	public void setReadoutTimestampEnabled(boolean on) {
		readoutTimestampEnabled = on;
	}

	public boolean isReadoutTimestampEnabled() {
		return readoutTimestampEnabled;
	}

	public void addSensorPixelModeUsed(int sensorPixelMode) {
		sensorPixelModes.add(Integer.valueOf(sensorPixelMode));
	}

	public void removeSensorPixelModeUsed(int sensorPixelMode) {
		sensorPixelModes.remove(Integer.valueOf(sensorPixelMode));
	}

	@Override
	public String toString() {
		return "OutputConfiguration(" + surfaces.size() + " surface(s), group " + surfaceGroupId +
		    ", physical " + physicalCameraId + ")";
	}
}
