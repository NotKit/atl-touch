package android.view.autofill;

/* Autofill is not implemented; every notification is a no-op. The methods still
 * have to exist: Compose's AndroidAutofillManager calls them from
 * onSemanticsChanged, i.e. inside LayoutNode.attach during a measure pass, so a
 * missing one aborts the whole layout instead of losing a feature. */
public class AutofillManager {

	public static abstract class AutofillCallback {}

	public interface AutofillClient {}

	public void registerCallback(AutofillCallback callback) {}

	public void unregisterCallback(AutofillCallback callback) {}

	public void cancel() { }

	public void commit() { }

	public void disableAutofillServices() { }

	public void notifyViewEntered(android.view.View a0) { }

	public void notifyViewEntered(android.view.View a0, int a1, android.graphics.Rect a2) { }

	public void notifyViewExited(android.view.View a0) { }

	public void notifyViewExited(android.view.View a0, int a1) { }

	public void notifyViewVisibilityChanged(android.view.View a0, boolean a1) { }

	public void notifyViewVisibilityChanged(android.view.View a0, int a1, boolean a2) { }

	public void notifyValueChanged(android.view.View a0) { }

	public void notifyValueChanged(android.view.View a0, int a1, android.view.autofill.AutofillValue a2) { }

	public void notifyViewClicked(android.view.View a0) { }

	public void notifyViewClicked(android.view.View a0, int a1) { }

	public void requestAutofill(android.view.View a0) { }

	public void requestAutofill(android.view.View a0, int a1, android.graphics.Rect a2) { }

	public boolean hasEnabledAutofillServices() { return false; }

	public boolean isEnabled() { return false; }

	public boolean isAutofillSupported() { return false; }
}
