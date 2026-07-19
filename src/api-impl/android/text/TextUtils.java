/*
 * most of this file:
 *
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.text;

import android.text.style.ReplacementSpan;

import com.android.internal.util.ArrayUtils;
import java.lang.reflect.Array;
import java.util.Iterator;
import java.util.Locale;
import java.util.regex.Pattern;

public class TextUtils {
	public static String nullIfEmpty(String str) {
		return isEmpty(str) ? null : str;
	}

	public static int getLayoutDirectionFromLocale(Locale locale) {
		return 0 /*LTR*/; // FIXME
	}

	/* split */

	private static String[] EMPTY_STRING_ARRAY = new String[] {};

	public static String[] split(String text, String expression) {
		if (text.length() == 0) {
			return EMPTY_STRING_ARRAY;
		} else {
			return text.split(expression, -1);
		}
	}

	public static String[] split(String text, Pattern pattern) {
		if (text.length() == 0) {
			return EMPTY_STRING_ARRAY;
		} else {
			return pattern.split(text, -1);
		}
	}

	/* join */

	public static String join(CharSequence delimiter, Object[] tokens) {
		StringBuilder sb = new StringBuilder();
		boolean firstTime = true;
		for (Object token : tokens) {
			if (firstTime) {
				firstTime = false;
			} else {
				sb.append(delimiter);
			}
			sb.append(token);
		}
		return sb.toString();
	}

	public static String join(CharSequence delimiter, Iterable tokens) {
		StringBuilder sb = new StringBuilder();
		boolean firstTime = true;
		for (Object token : tokens) {
			if (firstTime) {
				firstTime = false;
			} else {
				sb.append(delimiter);
			}
			sb.append(token);
		}
		return sb.toString();
	}

	public static int indexOf(CharSequence s, char ch) {
		return indexOf(s, ch, 0);
	}

	public static int indexOf(CharSequence s, char ch, int start) {
		Class<? extends CharSequence> c = s.getClass();

		if (c == String.class)
			return ((String)s).indexOf(ch, start);

		return indexOf(s, ch, start, s.length());
	}

	public static int indexOf(CharSequence s, char ch, int start, int end) {
		Class<? extends CharSequence> c = s.getClass();

		if (s instanceof GetChars || c == StringBuffer.class
		    || c == StringBuilder.class || c == String.class) {
			final int INDEX_INCREMENT = 500;
			char[] temp = obtain(INDEX_INCREMENT);

			while (start < end) {
				int segend = start + INDEX_INCREMENT;
				if (segend > end)
					segend = end;

				getChars(s, start, segend, temp, 0);

				int count = segend - start;
				for (int i = 0; i < count; i++) {
					if (temp[i] == ch) {
						recycle(temp);
						return i + start;
					}
				}

				start = segend;
			}

			recycle(temp);
			return -1;
		}

		for (int i = start; i < end; i++)
			if (s.charAt(i) == ch)
				return i;

		return -1;
	}

	public static int lastIndexOf(CharSequence s, char ch) {
		return lastIndexOf(s, ch, s.length() - 1);
	}

	public static int lastIndexOf(CharSequence s, char ch, int last) {
		Class<? extends CharSequence> c = s.getClass();

		if (c == String.class)
			return ((String)s).lastIndexOf(ch, last);

		return lastIndexOf(s, ch, 0, last);
	}

	public static int lastIndexOf(CharSequence s, char ch,
	                              int start, int last) {
		if (last < 0)
			return -1;
		if (last >= s.length())
			last = s.length() - 1;

		int end = last + 1;

		Class<? extends CharSequence> c = s.getClass();

		if (s instanceof GetChars || c == StringBuffer.class
		    || c == StringBuilder.class || c == String.class) {
			final int INDEX_INCREMENT = 500;
			char[] temp = obtain(INDEX_INCREMENT);

			while (start < end) {
				int segstart = end - INDEX_INCREMENT;
				if (segstart < start)
					segstart = start;

				getChars(s, segstart, end, temp, 0);

				int count = end - segstart;
				for (int i = count - 1; i >= 0; i--) {
					if (temp[i] == ch) {
						recycle(temp);
						return i + segstart;
					}
				}

				end = segstart;
			}

			recycle(temp);
			return -1;
		}

		for (int i = end - 1; i >= start; i--)
			if (s.charAt(i) == ch)
				return i;

		return -1;
	}

	public static int indexOf(CharSequence s, CharSequence needle) {
		return indexOf(s, needle, 0, s.length());
	}

	public static int indexOf(CharSequence s, CharSequence needle, int start) {
		return indexOf(s, needle, start, s.length());
	}

	public static int indexOf(CharSequence s, CharSequence needle,
	                          int start, int end) {
		int nlen = needle.length();
		if (nlen == 0)
			return start;

		char c = needle.charAt(0);

		for (;;) {
			start = indexOf(s, c, start);
			if (start > end - nlen) {
				break;
			}

			if (start < 0) {
				return -1;
			}

			if (regionMatches(s, start, needle, 0, nlen)) {
				return start;
			}

			start++;
		}
		return -1;
	}

	public static boolean regionMatches(CharSequence one, int toffset,
	                                    CharSequence two, int ooffset,
	                                    int len) {
		int tempLen = 2 * len;
		if (tempLen < len) {
			// Integer overflow; len is unreasonably large
			throw new IndexOutOfBoundsException();
		}
		char[] temp = obtain(tempLen);

		getChars(one, toffset, toffset + len, temp, 0);
		getChars(two, ooffset, ooffset + len, temp, len);

		boolean match = true;
		for (int i = 0; i < len; i++) {
			if (temp[i] != temp[i + len]) {
				match = false;
				break;
			}
		}

		recycle(temp);
		return match;
	}

	/* package */ static char[] obtain(int len) {
		char[] buf;

		synchronized (sLock) {
			buf = sTemp;
			sTemp = null;
		}

		if (buf == null || buf.length < len)
			buf = ArrayUtils.newUnpaddedCharArray(len);

		return buf;
	}

	/* package */ static void recycle(char[] temp) {
		if (temp.length > 1000)
			return;

		synchronized (sLock) {
			sTemp = temp;
		}
	}
	// end of unchanged from android source

	public static CharSequence join(Iterable<CharSequence> list) {
		final CharSequence delimiter = ","; // ????
		return join(delimiter, list);
	}

	public static boolean isEmpty(CharSequence str) {
		return str == null || str.length() == 0;
	}

	/**
	 * Returns true if a and b are equal, including if they are both null.
	 * <p><i>Note: In platform versions 1.1 and earlier, this method only worked well if
	 * both the arguments were instances of String.</i></p>
	 * @param a first CharSequence to check
	 * @param b second CharSequence to check
	 * @return true if a and b are equal
	 */
	public static boolean equals(CharSequence a, CharSequence b) {
		if (a == b)
			return true;
		int length;
		if (a != null && b != null && (length = a.length()) == b.length()) {
			if (a instanceof String && b instanceof String) {
				return a.equals(b);
			} else {
				for (int i = 0; i < length; i++) {
					if (a.charAt(i) != b.charAt(i))
						return false;
				}
				return true;
			}
		}
		return false;
	}

	public enum TruncateAt {
		START,
		MIDDLE,
		END,
		MARQUEE,
		END_SMALL
	}

	// Zero-width character used to fill ellipsized strings when codepoint length must be preserved.
	/* package */ static final char ELLIPSIS_FILLER = '﻿'; // ZERO WIDTH NO-BREAK SPACE

	private static final String ELLIPSIS_NORMAL = "…"; // HORIZONTAL ELLIPSIS (…)
	private static final String ELLIPSIS_TWO_DOTS = "‥"; // TWO DOT LEADER (‥)

	/** {@hide} */
	public static String getEllipsisString(TextUtils.TruncateAt method) {
		return (method == TextUtils.TruncateAt.END_SMALL) ? ELLIPSIS_TWO_DOTS : ELLIPSIS_NORMAL;
	}

	public interface EllipsizeCallback {
		/**
		 * This method is called to report that the specified region of
		 * text was ellipsized away by a call to {@link #ellipsize}.
		 */
		public void ellipsized(int start, int end);
	}

	/**
	 * Returns the original text if it fits in the specified width
	 * given the properties of the specified Paint,
	 * or, if it does not fit, a truncated
	 * copy with ellipsis character added at the specified edge or center.
	 */
	public static CharSequence ellipsize(CharSequence text, TextPaint p, float avail, TruncateAt where) {
		return ellipsize(text, p, avail, where, false, null);
	}

	/**
	 * Returns the original text if it fits in the specified width
	 * given the properties of the specified Paint,
	 * or, if it does not fit, a copy with ellipsis character added
	 * at the specified edge or center.
	 */
	public static CharSequence ellipsize(CharSequence text,
	                                     TextPaint paint,
	                                     float avail, TruncateAt where,
	                                     boolean preserveLength,
	                                     EllipsizeCallback callback) {
		return ellipsize(text, paint, avail, where, preserveLength, callback,
		                 TextDirectionHeuristics.FIRSTSTRONG_LTR,
		                 getEllipsisString(where));
	}

	/**
	 * Returns the original text if it fits in the specified width
	 * given the properties of the specified Paint,
	 * or, if it does not fit, a copy with ellipsis character added
	 * at the specified edge or center.
	 *
	 * @hide
	 */
	public static CharSequence ellipsize(CharSequence text,
	                                     TextPaint paint,
	                                     float avail, TruncateAt where,
	                                     boolean preserveLength,
	                                     EllipsizeCallback callback,
	                                     TextDirectionHeuristic textDir, String ellipsis) {

		int len = text.length();

		MeasuredParagraph mt = null;
		try {
			mt = MeasuredParagraph.buildForMeasurement(paint, text, 0, text.length(), textDir, mt);
			float width = mt.getWholeWidth();

			if (width <= avail) {
				if (callback != null) {
					callback.ellipsized(0, 0);
				}

				return text;
			}

			// XXX assumes ellipsis string does not require shaping and
			// is unaffected by style
			float ellipsiswid = paint.measureText(ellipsis);
			avail -= ellipsiswid;

			int left = 0;
			int right = len;
			if (avail < 0) {
				// it all goes
			} else if (where == TruncateAt.START) {
				right = len - mt.breakText(len, false, avail);
			} else if (where == TruncateAt.END || where == TruncateAt.END_SMALL) {
				left = mt.breakText(len, true, avail);
			} else {
				right = len - mt.breakText(len, false, avail / 2);
				avail -= mt.measure(right, len);
				left = mt.breakText(right, true, avail);
			}

			if (callback != null) {
				callback.ellipsized(left, right);
			}

			final char[] buf = mt.getChars();
			Spanned sp = text instanceof Spanned ? (Spanned)text : null;

			final int removed = right - left;
			final int remaining = len - removed;
			if (preserveLength) {
				if (remaining > 0 && removed >= ellipsis.length()) {
					ellipsis.getChars(0, ellipsis.length(), buf, left);
					left += ellipsis.length();
				} // else skip the ellipsis
				for (int i = left; i < right; i++) {
					buf[i] = ELLIPSIS_FILLER;
				}
				String s = new String(buf, 0, len);
				if (sp == null) {
					return s;
				}
				SpannableString ss = new SpannableString(s);
				copySpansFrom(sp, 0, len, Object.class, ss, 0);
				return ss;
			}

			if (remaining == 0) {
				return "";
			}

			if (sp == null) {
				StringBuilder sb = new StringBuilder(remaining + ellipsis.length());
				sb.append(buf, 0, left);
				sb.append(ellipsis);
				sb.append(buf, right, len - right);
				return sb.toString();
			}

			SpannableStringBuilder ssb = new SpannableStringBuilder();
			ssb.append(text, 0, left);
			ssb.append(ellipsis);
			ssb.append(text, right, len);
			return ssb;
		} finally {
			if (mt != null) {
				mt.recycle();
			}
		}
	}

	/**
	 * Return a new CharSequence in which each of the source strings is
	 * replaced by the corresponding element of the destinations.
	 */
	public static CharSequence replace(CharSequence template,
	                                   String[] sources,
	                                   CharSequence[] destinations) {
		SpannableStringBuilder tb = new SpannableStringBuilder(template);

		for (int i = 0; i < sources.length; i++) {
			int where = indexOf(tb, sources[i]);

			if (where >= 0)
				tb.setSpan(sources[i], where, where + sources[i].length(),
				           Spanned.SPAN_POINT_POINT);
		}

		for (int i = 0; i < sources.length; i++) {
			int start = tb.getSpanStart(sources[i]);
			int end = tb.getSpanEnd(sources[i]);

			if (start >= 0) {
				tb.replace(start, end, destinations[i]);
			}
		}

		return tb;
	}

	/**
	 * Copies the spans from the region <code>start...end</code> in
	 * <code>source</code> to the region
	 * <code>destoff...destoff+end-start</code> in <code>dest</code>.
	 */
	public static void copySpansFrom(Spanned source, int start, int end,
	                                 Class kind,
	                                 Spannable dest, int destoff) {
		if (kind == null) {
			kind = Object.class;
		}

		Object[] spans = source.getSpans(start, end, kind);

		for (int i = 0; i < spans.length; i++) {
			int st = source.getSpanStart(spans[i]);
			int en = source.getSpanEnd(spans[i]);
			int fl = source.getSpanFlags(spans[i]);

			if (st < start)
				st = start;
			if (en > end)
				en = end;

			dest.setSpan(spans[i], st - start + destoff, en - start + destoff,
			             fl);
		}
	}

	public static int getOffsetBefore(CharSequence text, int offset) {
		if (offset == 0)
			return 0;
		if (offset == 1)
			return 0;

		char c = text.charAt(offset - 1);

		if (c >= '\uDC00' && c <= '\uDFFF') {
			char c1 = text.charAt(offset - 2);

			if (c1 >= '\uD800' && c1 <= '\uDBFF')
				offset -= 2;
			else
				offset -= 1;
		} else {
			offset -= 1;
		}

		if (text instanceof Spanned) {
			ReplacementSpan[] spans = ((Spanned)text).getSpans(offset, offset,
			                                                   ReplacementSpan.class);

			for (int i = 0; i < spans.length; i++) {
				int start = ((Spanned)text).getSpanStart(spans[i]);
				int end = ((Spanned)text).getSpanEnd(spans[i]);

				if (start < offset && end > offset)
					offset = start;
			}
		}

		return offset;
	}

	public static int getOffsetAfter(CharSequence text, int offset) {
		int len = text.length();

		if (offset == len)
			return len;
		if (offset == len - 1)
			return len;

		char c = text.charAt(offset);

		if (c >= '\uD800' && c <= '\uDBFF') {
			char c1 = text.charAt(offset + 1);

			if (c1 >= '\uDC00' && c1 <= '\uDFFF')
				offset += 2;
			else
				offset += 1;
		} else {
			offset += 1;
		}

		if (text instanceof Spanned) {
			ReplacementSpan[] spans = ((Spanned)text).getSpans(offset, offset,
			                                                   ReplacementSpan.class);

			for (int i = 0; i < spans.length; i++) {
				int start = ((Spanned)text).getSpanStart(spans[i]);
				int end = ((Spanned)text).getSpanEnd(spans[i]);

				if (start < offset && end > offset)
					offset = end;
			}
		}

		return offset;
	}

	// Rough and conservative check for characters with a potential to affect RTL layout.
	/* package */
	static boolean couldAffectRtl(char c) {
		return (0x0590 <= c && c <= 0x08FF) ||          // RTL scripts
		       c == 0x200E ||                           // Bidi format character
		       c == 0x200F ||                           // Bidi format character
		       (0x202A <= c && c <= 0x202E) ||          // Bidi format characters
		       (0x2066 <= c && c <= 0x2069) ||          // Bidi format characters
		       (0xD800 <= c && c <= 0xDFFF) ||          // Surrogate pairs
		       (0xFB1D <= c && c <= 0xFDFF) ||          // Hebrew and Arabic presentation forms
		       (0xFE70 <= c && c <= 0xFEFE);            // Arabic presentation forms
	}

	// Returns true if there is no character present that may potentially affect RTL layout.
	/* package */
	static boolean doesNotNeedBidi(char[] text, int start, int len) {
		final int end = start + len;
		for (int i = start; i < end; i++) {
			if (couldAffectRtl(text[i])) {
				return false;
			}
		}
		return true;
	}

	/**
	 * @return A subset of spans where empty spans have been removed. The initial order is preserved
	 * @hide
	 */
	@SuppressWarnings("unchecked")
	public static <T> T[] removeEmptySpans(T[] spans, Spanned spanned, Class<T> klass) {
		T[] copy = null;
		int count = 0;

		for (int i = 0; i < spans.length; i++) {
			final T span = spans[i];
			final int start = spanned.getSpanStart(span);
			final int end = spanned.getSpanEnd(span);

			if (start == end) {
				if (copy == null) {
					copy = (T[])Array.newInstance(klass, spans.length - 1);
					System.arraycopy(spans, 0, copy, 0, i);
					count = i;
				}
			} else {
				if (copy != null) {
					copy[count] = span;
					count++;
				}
			}
		}

		if (copy != null) {
			T[] result = (T[])Array.newInstance(klass, count);
			System.arraycopy(copy, 0, result, 0, count);
			return result;
		} else {
			return spans;
		}
	}

	/**
	 * Pack 2 int values into a long, useful as a return value for a range
	 * @hide
	 */
	public static long packRangeInLong(int start, int end) {
		return (((long)start) << 32) | end;
	}

	/**
	 * Get the start value from a range packed in a long by {@link #packRangeInLong(int, int)}
	 * @hide
	 */
	public static int unpackRangeStartFromLong(long range) {
		return (int)(range >>> 32);
	}

	/**
	 * Get the end value from a range packed in a long by {@link #packRangeInLong(int, int)}
	 * @hide
	 */
	public static int unpackRangeEndFromLong(long range) {
		return (int)(range & 0x00000000FFFFFFFFL);
	}

	/** @hide */
	public static final int ALIGNMENT_SPAN = 1;
	/** @hide */
	public static final int FIRST_SPAN = ALIGNMENT_SPAN;
	/** @hide */
	public static final int FOREGROUND_COLOR_SPAN = 2;
	/** @hide */
	public static final int RELATIVE_SIZE_SPAN = 3;
	/** @hide */
	public static final int SCALE_X_SPAN = 4;
	/** @hide */
	public static final int STRIKETHROUGH_SPAN = 5;
	/** @hide */
	public static final int UNDERLINE_SPAN = 6;
	/** @hide */
	public static final int STYLE_SPAN = 7;
	/** @hide */
	public static final int BULLET_SPAN = 8;
	/** @hide */
	public static final int QUOTE_SPAN = 9;
	/** @hide */
	public static final int LEADING_MARGIN_SPAN = 10;
	/** @hide */
	public static final int URL_SPAN = 11;
	/** @hide */
	public static final int BACKGROUND_COLOR_SPAN = 12;
	/** @hide */
	public static final int TYPEFACE_SPAN = 13;
	/** @hide */
	public static final int SUPERSCRIPT_SPAN = 14;
	/** @hide */
	public static final int SUBSCRIPT_SPAN = 15;
	/** @hide */
	public static final int ABSOLUTE_SIZE_SPAN = 16;
	/** @hide */
	public static final int TEXT_APPEARANCE_SPAN = 17;
	/** @hide */
	public static final int ANNOTATION = 18;
	/** @hide */
	public static final int SUGGESTION_SPAN = 19;
	/** @hide */
	public static final int SPELL_CHECK_SPAN = 20;
	/** @hide */
	public static final int SUGGESTION_RANGE_SPAN = 21;
	/** @hide */
	public static final int EASY_EDIT_SPAN = 22;
	/** @hide */
	public static final int LOCALE_SPAN = 23;
	/** @hide */
	public static final int TTS_SPAN = 24;
	/** @hide */
	public static final int ACCESSIBILITY_CLICKABLE_SPAN = 25;
	/** @hide */
	public static final int ACCESSIBILITY_URL_SPAN = 26;
	/** @hide */
	public static final int LINE_BACKGROUND_SPAN = 27;
	/** @hide */
	public static final int LINE_HEIGHT_SPAN = 28;
	/** @hide */
	public static final int ACCESSIBILITY_REPLACEMENT_SPAN = 29;
	/** @hide */
	public static final int LAST_SPAN = ACCESSIBILITY_REPLACEMENT_SPAN;

	public static void getChars(CharSequence s, int start, int end, char[] dest, int destoff) {
		Class<? extends CharSequence> c = s.getClass();
		if (c == String.class)
			((String)s).getChars(start, end, dest, destoff);
		else if (c == StringBuffer.class)
			((StringBuffer)s).getChars(start, end, dest, destoff);
		else if (c == StringBuilder.class)
			((StringBuilder)s).getChars(start, end, dest, destoff);
		else if (s instanceof GetChars)
			((GetChars)s).getChars(start, end, dest, destoff);
		else {
			for (int i = start; i < end; i++)
				dest[destoff++] = s.charAt(i);
		}
	}

	public static int getCapsMode(CharSequence cs, int off, int reqModes) {
		return 0;
	}

	private static Object sLock = new Object();

	private static char[] sTemp = null;

	public static int getTrimmedLength(CharSequence s) {
		return s.toString().trim().length();
	}

	public static String htmlEncode(String s) {
		return s;
	}

	public static CharSequence concat(CharSequence[] array) {
		StringBuilder sb = new StringBuilder();
		for (CharSequence cs : array) {
			sb.append(cs);
		}
		return sb;
	}

	public static String substring(CharSequence s, int start, int end) {
		return s.subSequence(start, end).toString();
	}

	public static boolean isDigitsOnly(CharSequence str) {
		final int len = str.length();
		for (int cp, i = 0; i < len; i += Character.charCount(cp)) {
			cp = Character.codePointAt(str, i);
			if (!Character.isDigit(cp)) {
				return false;
			}
		}
		return true;
	}

	/**
	 * An interface for splitting strings according to rules that are opaque to the user of this
	 * interface. This also has less overhead than split, which uses regular expressions and
	 * allocates an array to hold the results.
	 *
	 * <p>The most efficient way to use this class is:
	 *
	 * <pre>
	 * // Once
	 * TextUtils.StringSplitter splitter = new TextUtils.SimpleStringSplitter(delimiter);
	 *
	 * // Once per string to split
	 * splitter.setString(string);
	 * for (String s : splitter) {
	 *     ...
	 * }
	 * </pre>
	 */
	public interface StringSplitter extends Iterable<String> {
		public void setString(String string);
	}

	/**
	 * A simple string splitter.
	 *
	 * <p>If the final character in the string to split is the delimiter then no empty string will
	 * be returned for the empty string after that delimeter. That is, splitting <tt>"a,b,"</tt> on
	 * comma will return <tt>"a", "b"</tt>, not <tt>"a", "b", ""</tt>.
	 */
	public static class SimpleStringSplitter implements StringSplitter, Iterator<String> {
		private String mString;
		private char mDelimiter;
		private int mPosition;
		private int mLength;

		/**
		 * Initializes the splitter. setString may be called later.
		 * @param delimiter the delimeter on which to split
		 */
		public SimpleStringSplitter(char delimiter) {
			mDelimiter = delimiter;
		}

		/**
		 * Sets the string to split
		 * @param string the string to split
		 */
		public void setString(String string) {
			mString = string;
			mPosition = 0;
			mLength = mString.length();
		}

		public Iterator<String> iterator() {
			return this;
		}

		public boolean hasNext() {
			return mPosition < mLength;
		}

		public String next() {
			int end = mString.indexOf(mDelimiter, mPosition);
			if (end == -1) {
				end = mLength;
			}
			String nextString = mString.substring(mPosition, end);
			mPosition = end + 1; // Skip the delimiter.
			return nextString;
		}

		public void remove() {
			throw new UnsupportedOperationException();
		}
	}
}
