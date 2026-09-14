package android.util;

/**
 * An immutable inclusive [lower, upper] interval, as camera2 hands out for
 * exposure, sensitivity and target fps.
 */
public final class Range<T extends Comparable<? super T>> {
	private final T lower;
	private final T upper;

	public Range(T lower, T upper) {
		if (lower == null || upper == null)
			throw new NullPointerException("Range endpoints must not be null");
		if (lower.compareTo(upper) > 0)
			throw new IllegalArgumentException("lower must be less than or equal to upper");
		this.lower = lower;
		this.upper = upper;
	}

	public static <T extends Comparable<? super T>> Range<T> create(T lower, T upper) {
		return new Range<T>(lower, upper);
	}

	public T getLower() {
		return lower;
	}

	public T getUpper() {
		return upper;
	}

	public boolean contains(T value) {
		return lower.compareTo(value) <= 0 && upper.compareTo(value) >= 0;
	}

	public boolean contains(Range<T> range) {
		return lower.compareTo(range.lower) <= 0 && upper.compareTo(range.upper) >= 0;
	}

	public T clamp(T value) {
		if (lower.compareTo(value) > 0)
			return lower;
		if (upper.compareTo(value) < 0)
			return upper;
		return value;
	}

	public Range<T> extend(Range<T> range) {
		return new Range<T>(lower.compareTo(range.lower) <= 0 ? lower : range.lower,
		    upper.compareTo(range.upper) >= 0 ? upper : range.upper);
	}

	public Range<T> extend(T lower, T upper) {
		return extend(new Range<T>(lower, upper));
	}

	public Range<T> extend(T value) {
		return extend(value, value);
	}

	public Range<T> intersect(Range<T> range) {
		return new Range<T>(lower.compareTo(range.lower) >= 0 ? lower : range.lower,
		    upper.compareTo(range.upper) <= 0 ? upper : range.upper);
	}

	public Range<T> intersect(T lower, T upper) {
		return intersect(new Range<T>(lower, upper));
	}

	@Override
	public boolean equals(Object other) {
		if (this == other)
			return true;
		if (!(other instanceof Range))
			return false;
		Range<?> range = (Range<?>)other;
		return lower.equals(range.lower) && upper.equals(range.upper);
	}

	@Override
	public int hashCode() {
		return lower.hashCode() * 31 + upper.hashCode();
	}

	@Override
	public String toString() {
		return "[" + lower + ", " + upper + "]";
	}
}
