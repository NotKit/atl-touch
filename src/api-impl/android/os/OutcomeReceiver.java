package android.os;

public interface OutcomeReceiver<R, E extends java.lang.Throwable> {

	public void onResult(R result);

	public default void onError(E error) {}
}
