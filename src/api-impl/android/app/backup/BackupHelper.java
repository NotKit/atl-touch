package android.app.backup;

/**
 * One part of a backup set. Like {@link BackupAgent}, this leaves out the
 * data-shuttling methods: nothing here ever runs a backup or a restore, so a
 * helper is only ever registered and forgotten.
 */
public interface BackupHelper {
}
