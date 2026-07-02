package android.text.format;

import java.util.Calendar;
import java.util.TimeZone;

public class Time {

	public int year;
	public int month;
	public int monthDay;
	public int hour;
	public int minute;
	public int second;
	public int yearDay;
	public int weekDay;
	public boolean allDay;
	public long gmtoff = 0;
	public String timezone;

	public Time() {
		this(TimeZone.getDefault().getID());
	}

	public Time(String timezone) {
		this.timezone = timezone;
	}

	public void set(long millis) {
		Calendar calendar = Calendar.getInstance(TimeZone.getTimeZone(timezone));
		calendar.setTimeInMillis(millis);
		year = calendar.get(Calendar.YEAR);
		month = calendar.get(Calendar.MONTH);
		monthDay = calendar.get(Calendar.DAY_OF_MONTH);
		hour = calendar.get(Calendar.HOUR_OF_DAY);
		minute = calendar.get(Calendar.MINUTE);
		second = calendar.get(Calendar.SECOND);
		yearDay = calendar.get(Calendar.DAY_OF_YEAR) - 1;
		weekDay = calendar.get(Calendar.DAY_OF_WEEK) - 1;
		gmtoff = calendar.get(Calendar.ZONE_OFFSET) / 1000;
	}

	public void setToNow() {
		set(System.currentTimeMillis());
	}

	public long toMillis(boolean ignoreDst) {
		Calendar calendar = Calendar.getInstance(TimeZone.getTimeZone(timezone));
		calendar.set(year, month, monthDay, hour, minute, second);
		calendar.set(Calendar.MILLISECOND, 0);
		return calendar.getTimeInMillis();
	}
}
