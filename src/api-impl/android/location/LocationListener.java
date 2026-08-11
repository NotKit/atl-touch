package android.location;

public interface LocationListener {

	public void onLocationChanged(Location location);

	public default void onProviderDisabled(java.lang.String a0) { }

	public default void onProviderEnabled(java.lang.String a0) { }

	public default void onStatusChanged(java.lang.String a0, int a1, android.os.Bundle a2) { }
}
