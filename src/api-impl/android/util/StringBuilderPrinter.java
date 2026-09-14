package android.util;

public class StringBuilderPrinter implements Printer {

	private final StringBuilder builder;

	public StringBuilderPrinter(StringBuilder builder) {
		this.builder = builder;
	}

	@Override
	public void println(String x) {
		builder.append(x).append('\n');
	}
}
