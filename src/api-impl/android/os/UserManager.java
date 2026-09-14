package android.os;

public class UserManager {
	/* ATL runs one user, and it is the one the host session belongs to */
	public boolean isSystemUser() {
		return true;
	}

	public boolean isUserAGoat() {
		return false;
	}

	public boolean isDemoUser() {
		return false;
	}

	public boolean isUserUnlocked() {
		return true;
	}
	public static boolean supportsMultipleUsers() {
		return false;
	}

	public long getSerialNumberForUser(UserHandle user) {
		return user.getIdentifier();
	}
}
