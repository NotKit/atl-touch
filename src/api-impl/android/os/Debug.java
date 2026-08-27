package android.os;

import java.io.IOException;

import dalvik.system.VMDebug;

public final class Debug {
	public static class MemoryInfo {
	
	public java.lang.String getMemoryStat(java.lang.String a0) { return null; }
}

	public static void waitForDebugger() {
	}

	public static class InstructionCount {
		public InstructionCount() {
		}
	}

	public static boolean isDebuggerConnected() {
		return false;
	}

	public static long getNativeHeapFreeSize() {
		return 0;
	}

	public static long getNativeHeapAllocatedSize() {
		return 0;
	}

	public static boolean waitingForDebugger() {
		return false;
	}

	public static long threadCpuTimeNanos() {
		return VMDebug.threadCpuTimeNanos();
	}

	public static void getMemoryInfo(android.os.Debug.MemoryInfo a0) { }
	/**
	 * Writes no heap dump: the layer has no ART heap dumper, and nothing here
	 * reads hprof. It exists so an app that asks for one links and takes its
	 * documented IOException path, instead of dying on a NoSuchMethodError that
	 * its {@code catch (Exception)} cannot catch.
	 */
	public static void dumpHprofData(String fileName) throws IOException {
		throw new IOException("android.os.Debug.dumpHprofData is not implemented: " + fileName);
	}
}
