package android.content;

import android.net.Uri;

public class ClipData {

	public static class Item {

		public Item(Uri uri) {}
	
	public android.net.Uri getUri() { return null; }

	public java.lang.CharSequence coerceToText(android.content.Context a0) { return null; }

	public java.lang.String getHtmlText() { return null; }
}

	String text;

	public ClipData(ClipDescription description, Item item) {}

	public static ClipData newPlainText(CharSequence label, CharSequence text) {
		ClipData clip = new ClipData(new ClipDescription(label, null), null);
		clip.text = text.toString();
		return clip;
	}

	public static ClipData newRawUri(CharSequence label, Uri uri) {
		ClipData clip = new ClipData(new ClipDescription(label, null), new Item(uri));
		clip.text = uri.toString();
		return clip;
	}

	public void addItem(ContentResolver resolver, Item item) {
	}

	public Item getItemAt(int index) {
		return null;
	}

	public int getItemCount() {
		return 0;
	}

	public android.content.ClipDescription getDescription() { return null; }

	public static android.content.ClipData newHtmlText(java.lang.CharSequence a0, java.lang.CharSequence a1, java.lang.String a2) { return null; }
}
