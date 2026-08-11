package android.text;

public interface Editable extends CharSequence, GetChars, Spannable, Appendable {

	public class Factory {
		public static Factory getInstance() {
			return new Factory();
		}

		public Editable newEditable(CharSequence source) {
			return new SpannableStringBuilder(source);
		}
	}

	public Editable replace(int start, int end, CharSequence source, int destoff, int destlen);

	public Editable replace(int start, int end, CharSequence text);

	public InputFilter[] getFilters();

	public void setFilters(InputFilter[] filters);

	public Editable delete(int start, int end);

	public Editable insert(int where, CharSequence text, int start, int end);

	public Editable insert(int where, CharSequence text);

	public Editable append(CharSequence text);

	public Editable append(CharSequence text, int start, int end);

	public Editable append(char text);

	public void clear();

	public void clearSpans();

	public <T> T[] getSpans(int queryStart, int queryEnd, Class<T> kind);
}
