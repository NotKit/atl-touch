package android.text.util;

import java.util.regex.Matcher;

import android.text.Spannable;
import android.widget.TextView;

public class Linkify {

	public static final MatchFilter sUrlMatchFilter = new MatchFilter() {
		@Override
		public boolean acceptMatch(CharSequence s, int start, int end) {
			return start == 0 || s.charAt(start - 1) != '@';  // don't linkify the domain of email addresses
		}
	};

	public static final boolean addLinks(Spannable text, int mask) { return true; }
	public static final boolean addLinks(TextView text, int mask) { return true; }

	public interface MatchFilter {
		boolean acceptMatch(CharSequence s, int start, int end);
	}

	public interface TransformFilter {
		String transformUrl(Matcher match, String url);
	}
}
