package android.util;

import java.util.regex.Pattern;

/**
 * The handful of public regular expressions android.util.Patterns exposes.
 * These are our own, written to the same shapes the platform documents, not
 * the AOSP ones; callers use them to classify user input, so being a little
 * stricter than the platform is the safe direction.
 */
public class Patterns {

	/** Dotted-quad IPv4 address. The platform's is IPv4 only, so this is too. */
	public static final Pattern IP_ADDRESS = Pattern.compile(
		"((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\\.){3}"
		+ "(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])");

	/** Top level domain: two or more letters, or a punycode label. */
	public static final String TOP_LEVEL_DOMAIN_STR = "(?:[a-zA-Z]{2,63}|xn--[a-zA-Z0-9]{1,59})";

	public static final Pattern TOP_LEVEL_DOMAIN = Pattern.compile(TOP_LEVEL_DOMAIN_STR);

	private static final String LABEL = "[a-zA-Z0-9](?:[a-zA-Z0-9_\\-]{0,61}[a-zA-Z0-9])?";

	private static final String HOST = "(?:" + LABEL + "\\.)+" + TOP_LEVEL_DOMAIN_STR;

	public static final Pattern DOMAIN_NAME = Pattern.compile(HOST + "|" + IP_ADDRESS.pattern());

	public static final Pattern WEB_URL = Pattern.compile(
		"(?:(?:https?|ftp|file)://)?"
		+ "(?:\\S{1,64}(?::\\S{0,64})?@)?"
		+ "(?:" + HOST + "|" + IP_ADDRESS.pattern() + ")"
		+ "(?::\\d{1,5})?"
		+ "(?:/[^\\s]*)?");

	public static final Pattern EMAIL_ADDRESS = Pattern.compile(
		"[a-zA-Z0-9\\+\\.\\_\\%\\-]{1,256}@" + HOST);

	/** Loose phone number: digits with the usual separators, optional +. */
	public static final Pattern PHONE = Pattern.compile(
		"(\\+[0-9]+[\\- \\.]*)?(\\([0-9]+\\)[\\- \\.]*)?([0-9][0-9\\- \\.]{4,}[0-9])");

	private Patterns() {
	}
}
