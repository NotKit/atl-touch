package android.app.job;

import android.net.Network;
import android.net.Uri;
import android.os.Parcelable;
import android.os.PersistableBundle;

public class JobParameters implements Parcelable {

	public static final Creator<JobParameters> CREATOR = null;

	public static final int STOP_REASON_UNDEFINED = 0;
	public static final int STOP_REASON_CANCELLED_BY_APP = 1;
	public static final int STOP_REASON_PREEMPT = 2;
	public static final int STOP_REASON_TIMEOUT = 3;
	public static final int STOP_REASON_DEVICE_STATE = 4;
	public static final int STOP_REASON_CONSTRAINT_BATTERY_NOT_LOW = 5;
	public static final int STOP_REASON_CONSTRAINT_CHARGING = 6;
	public static final int STOP_REASON_CONSTRAINT_CONNECTIVITY = 7;
	public static final int STOP_REASON_CONSTRAINT_DEVICE_IDLE = 8;
	public static final int STOP_REASON_CONSTRAINT_STORAGE_NOT_LOW = 9;
	public static final int STOP_REASON_QUOTA = 10;
	public static final int STOP_REASON_BACKGROUND_RESTRICTION = 11;
	public static final int STOP_REASON_APP_STANDBY = 12;
	public static final int STOP_REASON_USER = 13;
	public static final int STOP_REASON_SYSTEM_PROCESSING = 14;
	public static final int STOP_REASON_ESTIMATED_APP_LAUNCH_TIME_CHANGED = 15;

	JobInfo jobInfo;

	JobParameters(JobInfo jobInfo) {
		this.jobInfo = jobInfo;
	}

	public PersistableBundle getExtras() {
		return jobInfo.getExtras();
	}

	public Uri[] getTriggeredContentUris() {
		return new Uri[0];
	}

	public String[] getTriggeredContentAuthorities() {
		return new String[0];
	}

	/** ATL never binds a job to a network, so there is none to hand back. */
	public Network getNetwork() {
		return null;
	}

	public int getStopReason() {
		return STOP_REASON_UNDEFINED;
	}
}
