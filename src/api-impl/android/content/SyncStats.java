package android.content;

public class SyncStats {
	public long numAuthExceptions;
	public long numIoExceptions;
	public long numParseExceptions;
	public long numConflictDetectedExceptions;
	public long numInserts;
	public long numUpdates;
	public long numDeletes;
	public long numEntries;
	public long numSkippedEntries;

	@Override
	public String toString() {
		return "SyncStats{auth=" + numAuthExceptions + ", io=" + numIoExceptions + ", parse=" + numParseExceptions
		     + ", conflict=" + numConflictDetectedExceptions + ", inserts=" + numInserts + ", updates=" + numUpdates
		     + ", deletes=" + numDeletes + ", entries=" + numEntries + ", skipped=" + numSkippedEntries + "}";
	}
}
