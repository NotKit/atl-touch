package android.hardware;

public interface SensorEventListener {

	public void onSensorChanged(SensorEvent event);

	public default void onAccuracyChanged(android.hardware.Sensor a0, int a1) { }
}
