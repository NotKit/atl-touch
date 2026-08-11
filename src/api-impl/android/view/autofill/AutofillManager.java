package android.view.autofill;

public class AutofillManager {

	public static abstract class AutofillCallback {}

	public interface AutofillClient {}

	public void registerCallback(AutofillCallback callback) {}

	public void unregisterCallback(AutofillCallback callback) {}

	public void cancel() { }

	public void commit() { }

	public void notifyViewEntered(android.view.View a0, int a1, android.graphics.Rect a2) { }

	public void notifyViewExited(android.view.View a0, int a1) { }

	public void notifyValueChanged(android.view.View a0, int a1, android.view.autofill.AutofillValue a2) { }
}
