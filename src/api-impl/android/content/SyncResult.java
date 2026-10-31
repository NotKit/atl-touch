package android.content;

public final class SyncResult {
	public final SyncStats stats = new SyncStats();
	public boolean syncAlreadyInProgress;
	public boolean tooManyDeletions;
	public boolean tooManyRetries;
	public boolean databaseError;
	public boolean fullSyncRequested;
	public boolean partialSyncUnavailable;
	public boolean moreRecordsToGet;
	public long delayUntil;

	public boolean hasHardError() {
		return stats.numParseExceptions > 0 || stats.numConflictDetectedExceptions > 0 || stats.numAuthExceptions > 0
		    || tooManyDeletions || tooManyRetries || databaseError;
	}

	public boolean hasSoftError() {
		return syncAlreadyInProgress || stats.numIoExceptions > 0;
	}

	public boolean hasError() {
		return hasSoftError() || hasHardError();
	}

	@Override
	public String toString() {
		return "SyncResult{" + stats + ", tooManyDeletions=" + tooManyDeletions + ", tooManyRetries=" + tooManyRetries
		     + ", databaseError=" + databaseError + ", fullSyncRequested=" + fullSyncRequested
		     + ", moreRecordsToGet=" + moreRecordsToGet + ", delayUntil=" + delayUntil + "}";
	}
}
