/*
 * An invoke-interface that resolves to a java.lang.Object method (toString,
 * hashCode, equals) must dispatch on the receiver's own class. ART's optimizing
 * compiler used to turn it into an IMT-slot call whose runtime resolution was
 * cached in a table shared by every class, so the first implementation ever
 * resolved was called on every later receiver of any class.
 */
public class ImtObjectDispatchTest {
	interface Shape { int sides(); }

	static final class Triangle implements Shape {
		public int sides() { return 3; }
		@Override public String toString() { return "triangle"; }
	}

	static final class Square implements Shape {
		public int sides() { return 4; }
		@Override public String toString() { return "square"; }
	}

	/* no toString of its own: Object.toString, "ImtObjectDispatchTest$Blob@..." */
	static final class Blob implements Shape {
		public int sides() { return 0; }
	}

	/* the call goes through the interface type, so it is an invoke-interface
	 * of Object.toString in the dex */
	static String describe(Shape s) {
		return s.toString();
	}

	static int hash(Shape s) {
		return s.hashCode();
	}

	public static void main(String[] args) {
		Shape[] shapes = { new Triangle(), new Square(), new Blob() };
		String[] want = { "triangle", "square", null };
		int failures = 0;

		/* enough iterations for the JIT to compile describe(), and the AOT
		 * code is exercised from the first call */
		for (int round = 0; round < 20000; round++) {
			for (int i = 0; i < shapes.length; i++) {
				String got = describe(shapes[i]);
				String expected = want[i] != null ? want[i] : shapes[i].getClass().getName() + "@" + Integer.toHexString(shapes[i].hashCode());
				if (!expected.equals(got)) {
					if (failures++ < 5)
						System.out.println("round " + round + ": " + shapes[i].getClass().getSimpleName() + ".toString() through the interface gave '" + got + "', expected '" + expected + "'");
				}
				if (hash(shapes[i]) != shapes[i].hashCode()) {
					if (failures++ < 5)
						System.out.println("round " + round + ": hashCode through the interface differs for " + shapes[i].getClass().getSimpleName());
				}
			}
		}
		if (failures == 0)
			System.out.println("imt object dispatch: passed");
		else
			System.out.println("imt object dispatch: FAILED, " + failures + " wrong dispatches");
	}
}
