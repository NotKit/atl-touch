package android.view.inputmethod;

import android.os.Parcel;
import android.os.Parcelable;

public class ExtractedText implements Parcelable {
	public static final int FLAG_SELECTING = 0x0002;
	public static final int FLAG_SINGLE_LINE = 0x0001;

	public int flags;
	public int partialEndOffset;
	public int partialStartOffset;
	public int selectionEnd;
	public int selectionStart;
	public int startOffset;
	public CharSequence text;
	public CharSequence hint;

	public static final Parcelable.Creator<ExtractedText> CREATOR = new Parcelable.Creator<ExtractedText>() {
		public ExtractedText createFromParcel(Parcel source) { return new ExtractedText(); }

		public ExtractedText[] newArray(int size) { return new ExtractedText[size]; }
	};
}
