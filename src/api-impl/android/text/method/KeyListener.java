package android.text.method;

public interface KeyListener {

	public default boolean onKeyDown(android.view.View a0, android.text.Editable a1, int a2, android.view.KeyEvent a3) { return false; }

	public default boolean onKeyOther(android.view.View a0, android.text.Editable a1, android.view.KeyEvent a2) { return false; }

	public default boolean onKeyUp(android.view.View a0, android.text.Editable a1, int a2, android.view.KeyEvent a3) { return false; }
}
