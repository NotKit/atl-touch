package android.hardware;

import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Handler;
import java.util.Arrays;
import java.util.List;

public class SensorManager {

	public static float GRAVITY_EARTH = 9.81f;

	public Sensor getDefaultSensor(int type) {
		return new Sensor(type);
	}

	public boolean registerListener(SensorEventListener listener, Sensor sensor, int samplingPeriodUs, Handler handler) {
		return true; // we could try saying that the sensor doesn't exist and hope the app just doesn't use it then, but as long as we never call the handler the app should leave this alone
	}

	public boolean registerListener(final SensorEventListener listener, final Sensor sensor, int samplingPeriodUs) {
		switch (sensor.getType()) {
			case Sensor.TYPE_ORIENTATION:
				new LocationManager().requestLocationUpdates(null, 0, 0, new LocationListener() {
					@Override
					public void onLocationChanged(Location location) {
						listener.onSensorChanged(new SensorEvent(new float[] {location.getBearing()}, sensor));
					}
				});
				return true;
			case Sensor.TYPE_ACCELEROMETER:
				register_accelerometer_listener_native(listener, sensor, samplingPeriodUs);
				return true;
			default:
				return false;
		}
	}

	public void unregisterListener(final SensorEventListener listener) {
		unregisterListener(listener, null);
	}

	public void unregisterListener(final SensorEventListener listener, Sensor sensor) {
		System.out.println("STUB: andoroid.hw.SensorManager.unregisterListener");
	}

	native void register_accelerometer_listener_native(SensorEventListener listener, Sensor sensor, int sampling_period);

	public List<Sensor> getSensorList(int type) {
		return Arrays.asList(getDefaultSensor(type));
	}

	/* The orientation maths below is AOSP's, verbatim in behaviour: it is pure
	 * arithmetic on the caller's arrays and does not touch a sensor. */

	public static boolean getRotationMatrix(float[] R, float[] I, float[] gravity, float[] geomagnetic) {
		float Ax = gravity[0], Ay = gravity[1], Az = gravity[2];
		final float normsqA = Ax * Ax + Ay * Ay + Az * Az;
		final float g = 9.81f;
		if (normsqA < 0.01f * (g * g)) // free fall: the device has no "down"
			return false;

		final float Ex = geomagnetic[0], Ey = geomagnetic[1], Ez = geomagnetic[2];
		float Hx = Ey * Az - Ez * Ay;
		float Hy = Ez * Ax - Ex * Az;
		float Hz = Ex * Ay - Ey * Ax;
		final float normH = (float)Math.sqrt(Hx * Hx + Hy * Hy + Hz * Hz);
		if (normH < 0.1f) // the geomagnetic vector is close to vertical
			return false;

		final float invH = 1.0f / normH;
		Hx *= invH; Hy *= invH; Hz *= invH;
		final float invA = 1.0f / (float)Math.sqrt(normsqA);
		Ax *= invA; Ay *= invA; Az *= invA;
		final float Mx = Ay * Hz - Az * Hy;
		final float My = Az * Hx - Ax * Hz;
		final float Mz = Ax * Hy - Ay * Hx;
		if (R != null) {
			if (R.length == 9) {
				R[0] = Hx; R[1] = Hy; R[2] = Hz;
				R[3] = Mx; R[4] = My; R[5] = Mz;
				R[6] = Ax; R[7] = Ay; R[8] = Az;
			} else if (R.length == 16) {
				R[0] = Hx; R[1] = Hy; R[2] = Hz; R[3] = 0;
				R[4] = Mx; R[5] = My; R[6] = Mz; R[7] = 0;
				R[8] = Ax; R[9] = Ay; R[10] = Az; R[11] = 0;
				R[12] = 0; R[13] = 0; R[14] = 0; R[15] = 1;
			}
		}
		if (I != null) {
			final float invE = 1.0f / (float)Math.sqrt(Ex * Ex + Ey * Ey + Ez * Ez);
			final float c = (Ex * Mx + Ey * My + Ez * Mz) * invE;
			final float s = (Ex * Ax + Ey * Ay + Ez * Az) * invE;
			if (I.length == 9) {
				I[0] = 1; I[1] = 0; I[2] = 0;
				I[3] = 0; I[4] = c; I[5] = s;
				I[6] = 0; I[7] = -s; I[8] = c;
			} else if (I.length == 16) {
				I[0] = 1; I[1] = 0; I[2] = 0; I[3] = 0;
				I[4] = 0; I[5] = c; I[6] = s; I[7] = 0;
				I[8] = 0; I[9] = -s; I[10] = c; I[11] = 0;
				I[12] = 0; I[13] = 0; I[14] = 0; I[15] = 1;
			}
		}
		return true;
	}

	public static float[] getOrientation(float[] R, float[] values) {
		if (R.length == 9) {
			values[0] = (float)Math.atan2(R[1], R[4]);
			values[1] = (float)Math.asin(-R[7]);
			values[2] = (float)Math.atan2(-R[6], R[8]);
		} else {
			values[0] = (float)Math.atan2(R[1], R[5]);
			values[1] = (float)Math.asin(-R[9]);
			values[2] = (float)Math.atan2(-R[8], R[10]);
		}
		return values;
	}

	public static void getRotationMatrixFromVector(float[] R, float[] rotationVector) {
		float q0;
		float q1 = rotationVector[0];
		float q2 = rotationVector[1];
		float q3 = rotationVector[2];
		if (rotationVector.length >= 4) {
			q0 = rotationVector[3];
		} else {
			q0 = 1 - q1 * q1 - q2 * q2 - q3 * q3;
			q0 = (q0 > 0) ? (float)Math.sqrt(q0) : 0;
		}
		float sq_q1 = 2 * q1 * q1, sq_q2 = 2 * q2 * q2, sq_q3 = 2 * q3 * q3;
		float q1_q2 = 2 * q1 * q2, q3_q0 = 2 * q3 * q0;
		float q1_q3 = 2 * q1 * q3, q2_q0 = 2 * q2 * q0;
		float q2_q3 = 2 * q2 * q3, q1_q0 = 2 * q1 * q0;
		if (R.length == 9) {
			R[0] = 1 - sq_q2 - sq_q3; R[1] = q1_q2 - q3_q0; R[2] = q1_q3 + q2_q0;
			R[3] = q1_q2 + q3_q0; R[4] = 1 - sq_q1 - sq_q3; R[5] = q2_q3 - q1_q0;
			R[6] = q1_q3 - q2_q0; R[7] = q2_q3 + q1_q0; R[8] = 1 - sq_q1 - sq_q2;
		} else if (R.length == 16) {
			R[0] = 1 - sq_q2 - sq_q3; R[1] = q1_q2 - q3_q0; R[2] = q1_q3 + q2_q0; R[3] = 0.0f;
			R[4] = q1_q2 + q3_q0; R[5] = 1 - sq_q1 - sq_q3; R[6] = q2_q3 - q1_q0; R[7] = 0.0f;
			R[8] = q1_q3 - q2_q0; R[9] = q2_q3 + q1_q0; R[10] = 1 - sq_q1 - sq_q2; R[11] = 0.0f;
			R[12] = R[13] = R[14] = 0.0f; R[15] = 1.0f;
		}
	}

	public static boolean remapCoordinateSystem(float[] inR, int X, int Y, float[] outR) {
		if (inR == outR) {
			final float[] temp = new float[16];
			if (!remapCoordinateSystem(inR, X, Y, temp))
				return false;
			System.arraycopy(temp, 0, outR, 0, outR.length);
			return true;
		}
		final int length = outR.length;
		if (inR.length != length)
			return false;
		if ((X & 0x7C) != 0 || (Y & 0x7C) != 0)
			return false;
		if (((X & 0x3) == 0) || ((Y & 0x3) == 0))
			return false;
		if ((X & 0x3) == (Y & 0x3))
			return false;

		// Z is "the other" axis; its sign comes out of the axis order below
		int Z = X ^ Y;
		final int x = (X & 0x3) - 1, y = (Y & 0x3) - 1, z = (Z & 0x3) - 1;
		final int axis_y = (z + 1) % 3, axis_z = (z + 2) % 3;
		if (((x ^ axis_y) | (y ^ axis_z)) != 0)
			Z ^= 0x80;

		final boolean sx = (X >= 0x80), sy = (Y >= 0x80), sz = (Z >= 0x80);

		final int rowLength = ((length == 16) ? 4 : 3);
		for (int j = 0; j < 3; j++) {
			final int offset = j * rowLength;
			for (int i = 0; i < 3; i++) {
				if (x == i) outR[offset + i] = sx ? -inR[offset + 0] : inR[offset + 0];
				if (y == i) outR[offset + i] = sy ? -inR[offset + 1] : inR[offset + 1];
				if (z == i) outR[offset + i] = sz ? -inR[offset + 2] : inR[offset + 2];
			}
		}
		if (length == 16) {
			outR[3] = outR[7] = outR[11] = outR[12] = outR[13] = outR[14] = 0;
			outR[15] = 1;
		}
		return true;
	}
	public static final int SENSOR_DELAY_NORMAL = 3;
}
