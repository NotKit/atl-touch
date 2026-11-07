import android.app.ContextImpl;
import android.atl.ATLLoadedApp;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.text.Layout;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;
import java.lang.reflect.Method;

public class TextViewGeometryTest {
	private static final int WIDTH = 320, HEIGHT = 130;
	private static int assertions;

	private static class Field extends EditText {
		Field(Context context) { super(context); }
		@Override public boolean isFocused() { return true; }
	}

	// Telegram translates by top padding, suppresses the next padding reads,
	// then draws its own cursor using TextView's reflected gravity offset.
	private static class WrappedField extends Field {
		private boolean ignoreTop, ignoreBottom;
		WrappedField(Context context) { super(context); }
		@Override public int getExtendedPaddingTop() {
			if (!ignoreTop) return super.getExtendedPaddingTop();
			ignoreTop = false;
			return 0;
		}
		@Override public int getExtendedPaddingBottom() {
			if (!ignoreBottom) return super.getExtendedPaddingBottom();
			ignoreBottom = false;
			return 0;
		}
		@Override public void onDraw(Canvas canvas) {
			int top = getExtendedPaddingTop();
			ignoreTop = ignoreBottom = true;
			canvas.save();
			canvas.translate(0, top);
			super.onDraw(canvas);
			canvas.restore();
			check(!ignoreTop && !ignoreBottom, "both padding reads must be consumed");
		}
	}

	private static class Cursor extends ColorDrawable {
		Cursor() { super(Color.RED); }
		@Override public int getIntrinsicWidth() { return 3; }
	}

	public static void main(String[] args) throws Exception {
		Context context = new ContextImpl(Resources.getSystem(),
		    ATLLoadedApp.getPrimaryApplication(), com.android.internal.R.style.Theme_Material_Light);
		Field defaults = new Field(context);
		check(defaults.getMinimumHeight() == 0, "EditText must not impose a 48 dp minimum");
		check((defaults.getGravity() & Gravity.VERTICAL_GRAVITY_MASK) == Gravity.CENTER_VERTICAL,
		    "EditText must resolve the theme's gravity");
		check(defaults.getIncludeFontPadding(), "font padding defaults to enabled");
		check((new EditText(context, null, 0, 0).getGravity() & Gravity.VERTICAL_GRAVITY_MASK) == Gravity.TOP,
		    "constructors must preserve explicitly unstyled gravity");

		for (int gravity : new int[] {Gravity.TOP, Gravity.CENTER_VERTICAL, Gravity.BOTTOM}) {
			for (String text : new String[] {"", "Hello", "Hello\nworld"}) {
				Field plain = field(new Field(context), gravity, text);
				Field wrapped = field(new WrappedField(context), gravity, text);
				Bitmap expected = render(plain), actual = render(wrapped);
				checkSame(expected, actual, "translated field gravity=" + gravity + " text=" + text);
				check(plain.getTextCursorDrawable().getBounds().top == 0,
				    "cursor drawable bounds must be relative to its line");
				Method offset = TextView.class.getDeclaredMethod("getVerticalOffset", boolean.class);
				offset.setAccessible(true);
				check(plain.getTotalPaddingTop() == plain.getExtendedPaddingTop() + (int)offset.invoke(plain, true),
				    "total padding must agree with the reflected cursor offset");
			}
			for (String sample : new String[] {"Hello", "Hello\nworld"}) {
				Field hint = field(new Field(context), gravity, "");
				hint.setCursorVisible(false);
				hint.setHint(sample);
				hint.setHintTextColor(Color.BLACK);
				Field text = field(new Field(context), gravity, sample);
				text.setCursorVisible(false);
				checkSame(render(text), render(hint), "hint baseline gravity=" + gravity);
			}
		}

		Field composer = field(new Field(context), Gravity.BOTTOM, "Hello");
		composer.setBackgroundDrawable(null);
		composer.measure(View.MeasureSpec.makeMeasureSpec(WIDTH, View.MeasureSpec.EXACTLY),
		    View.MeasureSpec.makeMeasureSpec(HEIGHT, View.MeasureSpec.AT_MOST));
		check(composer.getMeasuredHeight() == composer.getLayout().getHeight() + 19,
		    "wrap_content composer must measure text plus its 9/10 px padding");

		composer.setMaxLines(2);
		composer.setText("one\ntwo\nthree\nfour");
		render(composer);
		check(composer.getLayout().getLineCount() == 4, "maxLines must preserve editable layout lines");
		int visibleHeight = composer.getLayout().getLineTop(2);
		int height = visibleHeight + 19 + 20;
		composer.measure(View.MeasureSpec.makeMeasureSpec(WIDTH, View.MeasureSpec.EXACTLY),
		    View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
		composer.layout(0, 0, WIDTH, height);
		check(composer.getExtendedPaddingTop() == 29,
		    "bottom gravity must put spare space above the visible maxLines");
		check(composer.getExtendedPaddingBottom() == 10, "bottom compound padding must be preserved");
		composer.setIncludeFontPadding(true);
		Layout padded = composer.getLayout();
		composer.setIncludeFontPadding(false);
		check(composer.getLayout() != padded, "font padding changes must rebuild the layout");
		System.out.println("text view geometry: passed " + assertions + " assertions");
	}

	private static <T extends Field> T field(T view, int gravity, String text) {
		view.setTextSize(TypedValue.COMPLEX_UNIT_PX, 32);
		view.setTextColor(Color.BLACK);
		view.setIncludeFontPadding(false);
		view.setPadding(7, 9, 5, 10);
		view.setGravity(gravity);
		view.setText(text);
		view.setSelection(0);
		view.setTextCursorDrawable(new Cursor());
		return view;
	}

	private static Bitmap render(Field view) {
		view.measure(View.MeasureSpec.makeMeasureSpec(WIDTH, View.MeasureSpec.EXACTLY),
		    View.MeasureSpec.makeMeasureSpec(HEIGHT, View.MeasureSpec.EXACTLY));
		view.layout(0, 0, WIDTH, HEIGHT);
		Bitmap bitmap = Bitmap.createBitmap(WIDTH, HEIGHT, Bitmap.Config.ARGB_8888);
		Canvas canvas = new Canvas(bitmap);
		canvas.drawColor(Color.WHITE);
		view.onDraw(canvas);
		return bitmap;
	}

	private static void checkSame(Bitmap expected, Bitmap actual, String label) {
		for (int y = 0; y < HEIGHT; y++)
			for (int x = 0; x < WIDTH; x++)
				if (expected.getPixel(x, y) != actual.getPixel(x, y))
					throw new AssertionError(label + " differs at " + x + "," + y);
		assertions++;
	}

	private static void check(boolean condition, String label) {
		if (!condition) throw new AssertionError(label);
		assertions++;
	}
}
