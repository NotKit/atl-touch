package android.view;

public class AbsSavedState implements android.os.Parcelable {
	/* the probe in testapps/shapeprobe needs an instance, and it is the only
	   way an app can get one without a Parcel */
	public static final AbsSavedState EMPTY_STATE = new AbsSavedState();
}
