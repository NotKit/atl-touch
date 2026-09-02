package android.telephony;

public class PhoneNumberUtils {

	/* matches any single character */
	private static final char WILD = 'N';
	/* AOSP calls two long numbers equal once this many trailing characters agree */
	private static final int MIN_MATCH = 7;
	private static final String[] INTL_PREFIXES = {"+", "00", "011"};

	public static boolean isNonSeparator(char c) {
		return c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '-';
	}

	public static boolean isGlobalPhoneNumber(String phoneNumber) {
		return phoneNumber != null && (phoneNumber.startsWith("+") || phoneNumber.startsWith("00"));
	}

	public static String stripSeparators(String phoneNumber) {
		return phoneNumber.replaceAll("[ \t\n\r-]", "");
	}

	public static boolean isDialable(char c) {
		return (c >= '0' && c <= '9') || c == '*' || c == '#' || c == '+' || c == WILD;
	}

	/* AOSP's loose comparison: two numbers are the same when their last MIN_MATCH
	 * dialable characters agree and what precedes them is no more than an
	 * international or trunk prefix, so "+15551234567" matches "05551234567".
	 * Numbers shorter than that have to agree in full. */
	public static boolean compare(String a, String b) {
		if (a == null || b == null)
			return a == b;

		String na = networkPortion(a);
		String nb = networkPortion(b);
		if (na.isEmpty() || nb.isEmpty())
			return false;

		int matched = 0;
		while (matched < na.length() && matched < nb.length()) {
			char ca = na.charAt(na.length() - 1 - matched);
			char cb = nb.charAt(nb.length() - 1 - matched);
			if (ca != cb && ca != WILD && cb != WILD)
				break;
			matched++;
		}

		if (matched < MIN_MATCH)
			return matched == na.length() && matched == nb.length();
		if (matched == na.length() || matched == nb.length())
			return true;

		String ra = na.substring(0, na.length() - matched);
		String rb = nb.substring(0, nb.length() - matched);
		return (isIntlPrefix(ra) && isIntlPrefix(rb))
		    || (isTrunkPrefix(ra) && isIntlPrefixAndCc(rb))
		    || (isTrunkPrefix(rb) && isIntlPrefixAndCc(ra));
	}

	/* the dialable characters before the first post-dial pause or wait */
	private static String networkPortion(String s) {
		StringBuilder sb = new StringBuilder(s.length());
		for (int i = 0; i < s.length(); i++) {
			char c = s.charAt(i);
			if (c == ',' || c == ';')
				break;
			if (isDialable(c))
				sb.append(c);
		}
		return sb.toString();
	}

	private static boolean isIntlPrefix(String s) {
		for (String prefix : INTL_PREFIXES) {
			if (s.equals(prefix))
				return true;
		}
		return false;
	}

	private static boolean isTrunkPrefix(String s) {
		return s.equals("0");
	}

	/* an international prefix followed by a 1-3 digit country code */
	private static boolean isIntlPrefixAndCc(String s) {
		for (String prefix : INTL_PREFIXES) {
			if (!s.startsWith(prefix))
				continue;
			String cc = s.substring(prefix.length());
			if (cc.isEmpty() || cc.length() > 3)
				return false;
			for (int i = 0; i < cc.length(); i++) {
				if (cc.charAt(i) < '0' || cc.charAt(i) > '9')
					return false;
			}
			return true;
		}
		return false;
	}
}
