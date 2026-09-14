package android.app;

import android.atl.ATLLoadedApp;
import android.content.Context;
import android.content.pm.ConfigurationInfo;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.Process;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public class ActivityManager {

	public static class RunningAppProcessInfo {
		public int importance;
		public int pid;
		public int uid;
		public String processName;
		/* AOSP's "why is this process at this importance" pair; ATL has one
		 * process and no reason to give */
		public int importanceReasonCode;
		public int importanceReasonPid;
		public android.content.ComponentName importanceReasonComponent;
		public int lastTrimLevel;

		// GMS' BackgroundDetector news one up itself; absent, that is a
		// NoSuchMethodError on the GoogleApiHandler thread and the app halts.
		public RunningAppProcessInfo() {}

		private RunningAppProcessInfo(int pid, String processName) {
			this.pid = pid;
			this.processName = processName;
		}
	}

	public static class TaskDescription {
		public TaskDescription(String name) {}
		public TaskDescription(String name, Bitmap icon, int color) {}
	}

	public List<RunningAppProcessInfo> getRunningAppProcesses() {
		return Arrays.asList(new RunningAppProcessInfo(Process.myPid(),
		                                               ATLLoadedApp.getPrimaryApplication().pkg.packageName));
	}

	public boolean isLowRamDevice() { return false; }

	/* ATL is nobody's test harness, and the kernel's low-memory kill reports
	 * are not readable from a host process */
	public static boolean isRunningInTestHarness() { return false; }

	public static boolean isRunningInUserTestHarness() { return false; }

	public static boolean isLowMemoryKillReportSupported() { return false; }

	public void setProcessStateSummary(byte[] state) {}

	public boolean isBackgroundRestricted() { return false; }

	public static class MemoryInfo {
		/* For now, just always report there's 10GB free RAM */
		public long availMem = 10000;

		public long totalMem = 10000;

		public long threshold = 200;

		public boolean lowMemory = false;
	}

	public void getMemoryInfo(MemoryInfo outInfo) {
		outInfo = new MemoryInfo();
	}

	public ConfigurationInfo getDeviceConfigurationInfo() {
		return new ConfigurationInfo();
	}

	public int getMemoryClass() { return 20; }      // suggested heap size in MB
	public int getLargeMemoryClass() { return 60; } // value chosen arbitrarily

	public static void getMyMemoryState(RunningAppProcessInfo outInfo) {}

	public boolean clearApplicationUserData() { return false; }

	public static class AppTask {}
	public List<ActivityManager.AppTask> getAppTasks() {
		return new ArrayList<>();
	}

	public static class RunningServiceInfo implements Parcelable {
		public RunningServiceInfo() {
		}

		public int describeContents() {
			return 0;
		}

		public void writeToParcel(Parcel dest, int flags) {
			return;
		}

		public void readFromParcel(Parcel source) {
			return;
		}
	}

	public List<RunningServiceInfo> getRunningServices(int maxNum)
	    throws SecurityException {
		return new ArrayList<>();
	}

	public List<ApplicationExitInfo> getHistoricalProcessExitReasons(String pkgname, int pid, int maxNum) {
		return Collections.emptyList();
	}

	/* ATL keeps no history of process starts or exits */
	public List<ApplicationExitInfo> getHistoricalProcessStartReasons(int maxNum) {
		return Collections.emptyList();
	}

	public static boolean isUserAMonkey() { return false; }

	public void moveTaskToFront(int taskId, int flags, Bundle options) {
	}
}
